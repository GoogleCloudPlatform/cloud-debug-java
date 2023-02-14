/**
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/agent/jvm_breakpoint.h"

#include <cstdint>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "src/agent/class_metadata_reader.h"
#include "src/agent/format_queue.h"
#include "src/agent/instance_field_reader.h"
#include "src/agent/jni_utils.h"
#include "src/agent/jvm_evaluators.h"
#include "src/agent/messages.h"
#include "src/agent/method_locals.h"
#include "src/agent/model_json.h"
#include "src/agent/model_util.h"
#include "src/agent/object_evaluator.h"
#include "src/agent/resolved_source_location.h"
#include "src/agent/static_field_reader.h"
#include "src/agent/statistician.h"
#include "tests/agent/fake_jni.h"
#include "tests/agent/json_eq_matcher.h"
#include "tests/agent/mock_breakpoint_labels_provider.h"
#include "tests/agent/mock_breakpoints_manager.h"
#include "tests/agent/mock_class_indexer.h"
#include "tests/agent/mock_class_metadata_reader.h"
#include "tests/agent/mock_class_path_lookup.h"
#include "tests/agent/mock_dynamic_logger.h"
#include "tests/agent/mock_eval_call_stack.h"
#include "tests/agent/mock_jvmti_env.h"
#include "tests/agent/mock_object_evaluator.h"
#include "tests/agent/mock_user_id_provider.h"

ABSL_DECLARE_FLAG(int32, breakpoint_expiration_sec);
ABSL_DECLARE_FLAG(double, max_dynamic_log_rate);
ABSL_DECLARE_FLAG(double, max_dynamic_log_bytes_rate);
ABSL_DECLARE_FLAG(int32, dynamic_log_quota_recovery_ms);

using testing::_;
using testing::AtMost;
using testing::InSequence;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::NiceMock;
using testing::NotNull;
using testing::Return;
using testing::WithArg;

namespace devtools {
namespace cdbg {

const jthread kBreakpointThread = reinterpret_cast<jthread>(0x725423);
const jmethodID kAbstractMethod = reinterpret_cast<jmethodID>(0x123511235);
const jmethodID kBreakpointMethod = reinterpret_cast<jmethodID>(0x612375234);

class JvmBreakpointTest : public ::testing::Test {
 protected:
  JvmBreakpointTest()
      : global_jvm_(fake_jni_.jvmti(), fake_jni_.jni()),
        method_locals_(nullptr),
        scheduler_([this]() { return simulated_time_sec_; }),
        global_condition_cost_limiter_(1000000, 1000000),
        global_dynamic_log_limiter_(1000, 0),
        global_dynamic_log_bytes_limiter_(1000000, 1000000) {
    evaluators_.class_path_lookup = &class_path_lookup_;
    evaluators_.class_indexer = &class_indexer_;
    evaluators_.eval_call_stack = &eval_call_stack_;
    evaluators_.method_locals = &method_locals_;
    evaluators_.class_metadata_reader = &class_metadata_reader_;
    evaluators_.object_evaluator = &object_evaluator_;
    evaluators_.method_caller_factory = [](Config::MethodCallQuotaType type) {
      return nullptr;
    };
    evaluators_.labels_factory = []() {
      return std::unique_ptr<BreakpointLabelsProvider>(
          new NiceMock<MockBreakpointLabelsProvider>);
    };
    evaluators_.user_id_provider_factory = []() {
      return std::unique_ptr<UserIdProvider>(new NiceMock<MockUserIdProvider>);
    };
  }

  void SetUp() override {
    InitializeStatisticians();

    EXPECT_CALL(breakpoints_manager_, GetGlobalConditionCostLimiter())
        .WillRepeatedly(Return(&global_condition_cost_limiter_));

    EXPECT_CALL(breakpoints_manager_, GetGlobalDynamicLogLimiter())
        .WillRepeatedly(Return(&global_dynamic_log_limiter_));

    EXPECT_CALL(breakpoints_manager_, GetGlobalDynamicLogBytesLimiter())
        .WillRepeatedly(Return(&global_dynamic_log_bytes_limiter_));

    ON_CALL(breakpoints_manager_, SetJvmtiBreakpoint(_, _, _))
        .WillByDefault(Return(true));

    ON_CALL(breakpoints_manager_, ClearJvmtiBreakpoint(_, _, _))
        .WillByDefault(Return());

    ON_CALL(class_indexer_, FindClassBySignature(_))
        .WillByDefault(Invoke([this](const std::string& class_signature) {
          return JniLocalRef(fake_jni_.FindClassBySignature(class_signature));
        }));

    // Mocked expectations for "com/prod/MyClass1.java" class are defined
    // in "FakeJni".

    auto& myclass1_metadata =
        fake_jni_.MutableStockClassMetadata(FakeJni::StockClass::MyClass1);

    // This is an abstract method with the exact same name as the actual
    // breakpoint method below, but with a different method signature.
    FakeJni::MethodMetadata abstract_method;
    abstract_method.id = kAbstractMethod;
    abstract_method.metadata.name = "breakpointMethod";
    abstract_method.metadata.signature = "(I)I";
    myclass1_metadata.methods.push_back(abstract_method);

    FakeJni::MethodMetadata breakpoint_method;
    breakpoint_method.id = kBreakpointMethod;
    breakpoint_method.metadata.name = "breakpointMethod";
    breakpoint_method.metadata.signature = "()V";
    breakpoint_method.line_number_table.push_back({100370, 370});
    breakpoint_method.line_number_table.push_back({100371, 371});
    breakpoint_method.line_number_table.push_back({100372, 372});
    myclass1_metadata.methods.push_back(breakpoint_method);

    ON_CALL(class_path_lookup_,
            ResolveSourceLocation("com/prod/MyClass1.java", 371, NotNull()))
        .WillByDefault(WithArg<2>(Invoke([](ResolvedSourceLocation* loc) {
          loc->error_message = FormatMessageModel();
          loc->class_signature = "Lcom/prod/MyClass1;";
          loc->method_name = "breakpointMethod";
          loc->method_signature = "()V";
          loc->adjusted_line_number = 371;
        })));

    breakpoint_template_ = BreakpointBuilder()
                               .set_id("test_breakpoint_id")
                               .set_location("com/prod/MyClass1.java", 371)
                               .build();
  }

  void TearDown() override {
    // Breakpoint cleanup.
    jvm_breakpoint_->ResetToPending();

    format_queue_.RemoveAll();

    CleanupStatisticians();
  }

  void Create(std::unique_ptr<BreakpointModel> breakpoint_definition) {
    jvm_breakpoint_ = std::make_shared<JvmBreakpoint>(
        &scheduler_, &evaluators_, &format_queue_, &dynamic_logger_,
        &breakpoints_manager_,
        nullptr,  // setup_error
        std::move(breakpoint_definition));

    jvm_breakpoint_->Initialize();
  }

  void ExpectNotLoadedClassLookup(const std::string& not_loaded_class) {
    // JvmReadersFactory::FindClassByName() will try loading the class in
    // the following ways:
    EXPECT_CALL(class_indexer_, FindClassByName(not_loaded_class))
        .WillOnce(
            Invoke([](const std::string& _) { return JniLocalRef(nullptr); }));
    EXPECT_CALL(class_indexer_,
                FindClassByName("java.lang." + not_loaded_class))
        .WillOnce(
            Invoke([](const std::string& _) { return JniLocalRef(nullptr); }));
    EXPECT_CALL(class_path_lookup_, FindClassesByName(not_loaded_class))
        .WillOnce(Invoke([not_loaded_class](const std::string& _) {
          return std::vector<std::string>{not_loaded_class};
        }));

    // GetMethodDeclaringClass would be called multiple times in compilation to
    // examine various types of grammar statements.
    ON_CALL(*((MockJvmtiEnv*)fake_jni_.jvmti()), GetMethodDeclaringClass(_, _))
        .WillByDefault(Return(JVMTI_ERROR_NOT_FOUND));
  }

 protected:
  FakeJni fake_jni_;
  GlobalJvmEnv global_jvm_;
  std::unique_ptr<Config> config_ = Config::Builder().Build();
  NiceMock<MockClassPathLookup> class_path_lookup_;
  NiceMock<MockClassIndexer> class_indexer_;
  NiceMock<MockEvalCallStack> eval_call_stack_;
  MethodLocals method_locals_;
  MockClassMetadataReader class_metadata_reader_;
  NiceMock<MockObjectEvaluator> object_evaluator_;
  JvmEvaluators evaluators_;
  FormatQueue format_queue_;
  NiceMock<MockDynamicLogger> dynamic_logger_;
  Scheduler<> scheduler_;
  LeakyBucket global_condition_cost_limiter_;
  LeakyBucket global_dynamic_log_limiter_;
  LeakyBucket global_dynamic_log_bytes_limiter_;
  NiceMock<MockBreakpointsManager> breakpoints_manager_;
  std::shared_ptr<JvmBreakpoint> jvm_breakpoint_;

  // Simulated absolute time (in seconds).
  time_t simulated_time_sec_{1000000};

  // Breakpoint template used throughout this test. Each test case slightly
  // modifies the breakpoint definition. Having this template spares each
  // test case to repeat the entire definition over and over again.
  std::unique_ptr<BreakpointModel> breakpoint_template_;
};

TEST_F(JvmBreakpointTest, NullSourceLocation) {
  EXPECT_CALL(breakpoints_manager_, SetJvmtiBreakpoint(_, _, _)).Times(0);

  EXPECT_CALL(breakpoints_manager_, CompleteBreakpoint("test_breakpoint_id"))
      .WillOnce(Return());

  Create(
      BreakpointBuilder(*breakpoint_template_).set_location(nullptr).build());

  auto result = format_queue_.FormatAndPop();
  ASSERT_NE(nullptr, result);

  EXPECT_TRUE(result->status->is_error);
  EXPECT_NE("", result->status->description.format);
}

TEST_F(JvmBreakpointTest, InvalidSourceLocation) {
  EXPECT_CALL(class_path_lookup_,
              ResolveSourceLocation("com/prod/MyClass1.java", 371, NotNull()))
      .WillRepeatedly(WithArg<2>(Invoke([](ResolvedSourceLocation* loc) {
        loc->error_message = {"something not found"};
      })));

  EXPECT_CALL(breakpoints_manager_, SetJvmtiBreakpoint(_, _, _)).Times(0);

  Create(BreakpointBuilder(*breakpoint_template_).build());

  auto result = format_queue_.FormatAndPop();
  ASSERT_NE(nullptr, result);

  EXPECT_TRUE(result->status->is_error);
  EXPECT_EQ("something not found", result->status->description.format);
}

TEST_F(JvmBreakpointTest, DeferredBreakpoint) {
  EXPECT_CALL(class_path_lookup_,
              ResolveSourceLocation("com/prod/MyClass1.java", 371, NotNull()))
      .WillRepeatedly(WithArg<2>(Invoke([](ResolvedSourceLocation* loc) {
        loc->error_message = FormatMessageModel();
        loc->class_signature = "Lcom/prod/ClassThatHasntBeenLoadedYet;";
        loc->method_name = "breakpointMethod";
        loc->method_signature = "()V";
        loc->adjusted_line_number = 371;
      })));

  EXPECT_CALL(breakpoints_manager_, SetJvmtiBreakpoint(_, _, _)).Times(0);

  Create(BreakpointBuilder(*breakpoint_template_).build());

  EXPECT_EQ(nullptr, format_queue_.FormatAndPop());
}
TEST_F(JvmBreakpointTest, DeferredBreakpointWithCondition) {
  // To examine the existence of class A.B.C, the compilation
  // will break it down and look for A, and then A.B, and then A.B.C, in order
  // to identify potential nested class.
  ExpectNotLoadedClassLookup("com");
  ExpectNotLoadedClassLookup("com.prod");
  ExpectNotLoadedClassLookup("com.prod.NotLoadedClass");

  EXPECT_CALL(breakpoints_manager_, SetJvmtiBreakpoint(_, _, _)).Times(0);

  Create(BreakpointBuilder(*breakpoint_template_)
             .set_condition("com.prod.NotLoadedClass.method()")
             .build());

  EXPECT_EQ(nullptr, format_queue_.FormatAndPop());
}
TEST_F(JvmBreakpointTest, ImmediateBreakpoint) {
  Create(BreakpointBuilder(*breakpoint_template_).build());

  EXPECT_EQ(nullptr, format_queue_.FormatAndPop());
}

TEST_F(JvmBreakpointTest, ClassPreparedOnDeferredBreakpoint) {
  // Simulate class not loaded.
  EXPECT_CALL(class_indexer_, FindClassBySignature(_))
      .WillRepeatedly(InvokeWithoutArgs([]() { return nullptr; }));

  Create(BreakpointBuilder(*breakpoint_template_).build());

  // The class is loaded from here on.
  EXPECT_CALL(class_indexer_, FindClassBySignature(_))
      .WillRepeatedly(Invoke([this](const std::string& class_signature) {
        return JniLocalRef(fake_jni_.FindClassBySignature(class_signature));
      }));

  // Not the type we need, has no effect.
  jvm_breakpoint_->OnClassPrepared("com.prod.MyClass2", "Lcom/prod/MyClass2;");

  EXPECT_CALL(breakpoints_manager_, SetJvmtiBreakpoint(_, _, _)).Times(1);

  jvm_breakpoint_->OnClassPrepared("com.prod.MyClass1", "Lcom/prod/MyClass1;");
}

// This situation shouldn't normally happen. We simulate that
// "ClassPathLookup.resolveSourceLocation" successfully mapped the source line
// to a method, but when this method is loaded, the function that
// "ClassPathLookup" found isn't there.
TEST_F(JvmBreakpointTest, SourceResolutionMismatch_MissingMethod) {
  auto& myclass1_metadata =
      fake_jni_.MutableStockClassMetadata(FakeJni::StockClass::MyClass1);
  myclass1_metadata.methods.clear();

  Create(BreakpointBuilder(*breakpoint_template_).build());

  // Verify only one hit result with error.
  auto result = format_queue_.FormatAndPop();
  ASSERT_NE(nullptr, result);
  EXPECT_TRUE(result->is_final_state);
  EXPECT_EQ(INTERNAL_ERROR_MESSAGE.format, result->status->description.format);

  EXPECT_EQ(nullptr, format_queue_.FormatAndPop());
}

// Same as "SourceResolutionMismatch_MissingMethod", but with wrong line.
TEST_F(JvmBreakpointTest, SourceResolutionMismatch_BadLine) {
  auto& myclass1_metadata =
      fake_jni_.MutableStockClassMetadata(FakeJni::StockClass::MyClass1);
  myclass1_metadata.methods[1].line_number_table.clear();

  Create(BreakpointBuilder(*breakpoint_template_).build());

  // Verify only one hit result with error.
  auto result = format_queue_.FormatAndPop();
  ASSERT_NE(nullptr, result);
  EXPECT_TRUE(result->is_final_state);
  EXPECT_EQ(INTERNAL_ERROR_MESSAGE.format, result->status->description.format);

  EXPECT_EQ(nullptr, format_queue_.FormatAndPop());
}

TEST_F(JvmBreakpointTest, Condition_Match) {
  Create(
      BreakpointBuilder(*breakpoint_template_).set_condition("2 < 3").build());

  jvm_breakpoint_->OnJvmBreakpointHit(kBreakpointThread, kBreakpointMethod,
                                      100371);

  // Verify only one hit result.
  auto result = format_queue_.FormatAndPop();
  ASSERT_NE(nullptr, result);
  ASSERT_EQ(nullptr, result->status);

  EXPECT_EQ(nullptr, format_queue_.FormatAndPop());
}

TEST_F(JvmBreakpointTest, Condition_NoMatch) {
  Create(
      BreakpointBuilder(*breakpoint_template_).set_condition("2 > 3").build());

  jvm_breakpoint_->OnJvmBreakpointHit(kBreakpointThread, kBreakpointMethod,
                                      100371);

  // Verify no hit results.
  EXPECT_EQ(nullptr, format_queue_.FormatAndPop());
}

TEST_F(JvmBreakpointTest, BadCondition) {
  const struct {
    std::string condition;
    FormatMessageModel expected_error;
  } test_cases[] = {{// Syntax error.
                     "2 + (4 -",
                     {ExpressionParserError}},
                    {// Not a boolean condition.
                     "2 + 3",
                     {ConditionNotBoolean, {"int"}}}};

  // Not expecting "SetBreakpoint" JVMTI call due to invalid expression.
  EXPECT_CALL(breakpoints_manager_, SetJvmtiBreakpoint(_, _, _)).Times(0);

  for (const auto& test_case : test_cases) {
    LOG(INFO) << "Testing bad condition '" << test_case.condition << "'";

    Create(BreakpointBuilder(*breakpoint_template_)
               .set_condition(test_case.condition)
               .build());

    // Verify only one final hit result (breakpoint failed to set).
    ExpectJsonEq(
        *BreakpointBuilder(*breakpoint_template_)
             .set_is_final_state(true)
             .set_condition(test_case.condition)
             .set_status(
                 StatusMessageBuilder()
                     .set_error()
                     .set_refers_to(
                         StatusMessageModel::Context::BREAKPOINT_CONDITION)
                     .set_description(test_case.expected_error)
                     .build())
             .build(),
        format_queue_.FormatAndPop().get());

    EXPECT_EQ(nullptr, format_queue_.FormatAndPop());
  }
}

TEST_F(JvmBreakpointTest, WatchedExpressions) {
  Create(BreakpointBuilder(*breakpoint_template_)
             .set_expressions({"2+3", "3.14*2"})
             .build());

  jvm_breakpoint_->OnJvmBreakpointHit(kBreakpointThread, kBreakpointMethod,
                                      100371);

  // Verify only one hit result.
  ExpectJsonEq(
      *BreakpointBuilder(*breakpoint_template_)
           .set_is_final_state(true)
           .set_expressions({"2+3", "3.14*2"})
           .add_evaluated_expression(
               VariableBuilder().set_name("2+3").set_value("5").set_type("int"))
           .add_evaluated_expression(
               VariableBuilder().set_name("3.14*2").set_value("6.28").set_type(
                   "double"))
           .add_capture_buffer_full_variable_table_item()
           .build(),
      format_queue_.FormatAndPop().get());

  EXPECT_EQ(nullptr, format_queue_.FormatAndPop());
}

TEST_F(JvmBreakpointTest, InterimBreakpointResults_BadExpression) {
  Create(BreakpointBuilder(*breakpoint_template_)
             .set_expressions({
                 "2+3",    // Valid expression.
                 "3.14*("  // Expression with syntax error.
             })
             .build());

  // Verify interim breakpoint update.
  auto result = format_queue_.FormatAndPop();
  ExpectJsonEq(*BreakpointBuilder(*breakpoint_template_)
                    .set_expressions({"2+3", "3.14*("})
                    .add_evaluated_expression(
                        VariableBuilder().set_name("2+3").set_value(""))
                    .add_evaluated_expression(
                        VariableBuilder().set_name("3.14*(").set_status(
                            StatusMessageBuilder()
                                .set_error()
                                .set_refers_to(
                                    StatusMessageModel::Context::VARIABLE_NAME)
                                .set_format(ExpressionParserError)
                                .build()))
                    .set_is_final_state(false)
                    .build(),
               result.get());

  EXPECT_EQ(nullptr, format_queue_.FormatAndPop());

  // Now simulate breakpoint hit.
  jvm_breakpoint_->OnJvmBreakpointHit(kBreakpointThread, kBreakpointMethod,
                                      100371);

  // Verify only one hit result.
  ExpectJsonEq(
      *BreakpointBuilder(*breakpoint_template_)
           .set_is_final_state(true)
           .set_expressions({"2+3", "3.14*("})
           .add_evaluated_expression(
               VariableBuilder().set_name("2+3").set_value("5").set_type("int"))
           .add_evaluated_expression(
               VariableBuilder().set_name("3.14*(").set_status(
                   StatusMessageBuilder()
                       .set_error()
                       .set_refers_to(
                           StatusMessageModel::Context::VARIABLE_NAME)
                       .set_format(ExpressionParserError)
                       .build()))
           .add_capture_buffer_full_variable_table_item()
           .build(),
      format_queue_.FormatAndPop().get());

  EXPECT_EQ(nullptr, format_queue_.FormatAndPop());
}

TEST_F(JvmBreakpointTest, ClassNotLoadedWhenInsert_BadExpression) {
  ExpectNotLoadedClassLookup("NotLoadedClass");

  EXPECT_CALL(breakpoints_manager_, SetJvmtiBreakpoint(_, _, _)).Times(0);

  // Create a new breakpoint and evaluate the expressions.
  // The breakpoint should remain pending.
  Create(BreakpointBuilder(*breakpoint_template_)
             .set_expressions({
                 "2+3",                      // Valid expression
                 "NotLoadedClass.SomeField"  // Expression that references
                                             // an unloaded class.
             })
             .build());

  // Breakpoint remains pending, so nothing in queue.
  EXPECT_EQ(nullptr, format_queue_.FormatAndPop());
}

TEST_F(JvmBreakpointTest, LineNumberAdjustment) {
  EXPECT_CALL(class_path_lookup_,
              ResolveSourceLocation("com/prod/MyClass1.java", 371, NotNull()))
      .WillRepeatedly(WithArg<2>(Invoke([](ResolvedSourceLocation* loc) {
        loc->error_message = FormatMessageModel();
        loc->class_signature = "Lcom/prod/MyClass1;";
        loc->method_name = "breakpointMethod";
        loc->method_signature = "()V";
        loc->adjusted_line_number = 372;
      })));

  EXPECT_CALL(breakpoints_manager_,
              SetJvmtiBreakpoint(kBreakpointMethod, 100372, _))
      .WillOnce(Return(true));

  Create(BreakpointBuilder(*breakpoint_template_).build());

  // Expect interim result saying that the breakpoint moved.
  ExpectJsonEq(*BreakpointBuilder(*breakpoint_template_)
                    .set_location("com/prod/MyClass1.java", 372)
                    .build(),
               format_queue_.FormatAndPop().get());

  // Simulate breakpoint hit.
  EXPECT_CALL(breakpoints_manager_,
              ClearJvmtiBreakpoint(kBreakpointMethod, 100372, _))
      .WillOnce(Return());

  jvm_breakpoint_->OnJvmBreakpointHit(kBreakpointThread, kBreakpointMethod,
                                      100372);

  ExpectJsonEq(*BreakpointBuilder(*breakpoint_template_)
                    .set_is_final_state(true)
                    .set_location("com/prod/MyClass1.java", 372)
                    .add_capture_buffer_full_variable_table_item()
                    .build(),
               format_queue_.FormatAndPop().get());

  EXPECT_EQ(nullptr, format_queue_.FormatAndPop());
}

TEST_F(JvmBreakpointTest, BreakpointExpirationWithCreatedTime) {
  Create(BreakpointBuilder(*breakpoint_template_)
             .set_create_time(TimestampBuilder::Build(simulated_time_sec_))
             .build());

  EXPECT_CALL(breakpoints_manager_, CompleteBreakpoint("test_breakpoint_id"))
      .WillOnce(Return());

  simulated_time_sec_ += absl::GetFlag(FLAGS_breakpoint_expiration_sec);
  scheduler_.Process();
}

TEST_F(JvmBreakpointTest, BreakpointExpirationWithCreatedTimeUnixMsec) {
  Create(BreakpointBuilder(*breakpoint_template_)
             .set_create_time_unix_msec(
                 TimestampBuilder::Build(simulated_time_sec_))
             .build());

  EXPECT_CALL(breakpoints_manager_, CompleteBreakpoint("test_breakpoint_id"))
      .WillOnce(Return());

  simulated_time_sec_ += absl::GetFlag(FLAGS_breakpoint_expiration_sec);
  scheduler_.Process();
}

TEST_F(JvmBreakpointTest, BreakpointExpirationNoCreatedTime) {
  Create(BreakpointBuilder(*breakpoint_template_).build());

  simulated_time_sec_ += absl::GetFlag(FLAGS_breakpoint_expiration_sec) - 1;
  scheduler_.Process();

  EXPECT_CALL(breakpoints_manager_, CompleteBreakpoint("test_breakpoint_id"))
      .WillOnce(Return());

  simulated_time_sec_ += 2;
  scheduler_.Process();
}

TEST_F(JvmBreakpointTest, BreakpointExpirationWithExpiresIn) {
  Create(BreakpointBuilder(*breakpoint_template_)
             .set_create_time(TimestampBuilder::Build(simulated_time_sec_))
             .set_expires_in(DurationBuilder::Build(10))
             .build());

  simulated_time_sec_ += 9;
  scheduler_.Process();

  EXPECT_CALL(breakpoints_manager_, CompleteBreakpoint("test_breakpoint_id"))
      .WillOnce(Return());

  simulated_time_sec_ += 2;
  scheduler_.Process();
}

TEST_F(JvmBreakpointTest, BreakpointExpirationWithTruncatedExpiresIn) {
  Create(BreakpointBuilder(*breakpoint_template_)
             .set_create_time(TimestampBuilder::Build(simulated_time_sec_))
             .set_expires_in(DurationBuilder::Build(
                 // Values higher than the expiration sec flag are truncated.
                 absl::GetFlag(FLAGS_breakpoint_expiration_sec) + 10))
             .build());

  EXPECT_CALL(breakpoints_manager_, CompleteBreakpoint("test_breakpoint_id"))
      .WillOnce(Return());

  simulated_time_sec_ += absl::GetFlag(FLAGS_breakpoint_expiration_sec);
  scheduler_.Process();
}

TEST_F(JvmBreakpointTest, BreakpointExpirationWithNegativeExpiresIn) {
  Create(BreakpointBuilder(*breakpoint_template_)
             .set_create_time(TimestampBuilder::Build(simulated_time_sec_))
             .set_expires_in(DurationBuilder::Build(-1))
             .build());

  EXPECT_CALL(breakpoints_manager_, CompleteBreakpoint("test_breakpoint_id"))
      .WillOnce(Return());

  scheduler_.Process();
}

TEST_F(JvmBreakpointTest, DynamicLoggerNotAvailable) {
  Create(BreakpointBuilder(*breakpoint_template_)
             .set_action(BreakpointModel::Action::LOG)
             .build());

  EXPECT_CALL(dynamic_logger_, IsAvailable()).WillRepeatedly(Return(false));

  EXPECT_CALL(breakpoints_manager_, CompleteBreakpoint("test_breakpoint_id"))
      .WillOnce(Return());

  jvm_breakpoint_->OnJvmBreakpointHit(kBreakpointThread, kBreakpointMethod,
                                      100371);
}

TEST_F(JvmBreakpointTest, DynamicLoggerNoParameters) {
  Create(BreakpointBuilder(*breakpoint_template_)
             .set_action(BreakpointModel::Action::LOG)
             .set_log_level(BreakpointModel::LogLevel::WARNING)
             .set_log_message_format("hello there")
             .build());

  EXPECT_CALL(dynamic_logger_, Log(BreakpointModel::LogLevel::WARNING, _,
                                   "LOGPOINT: hello there"))
      .WillOnce(WithArg<1>(Invoke([](const ResolvedSourceLocation& rsl) {
        EXPECT_EQ("", rsl.error_message.format);
        EXPECT_EQ("Lcom/prod/MyClass1;", rsl.class_signature);
        EXPECT_EQ("breakpointMethod", rsl.method_name);
        EXPECT_EQ(371, rsl.adjusted_line_number);
      })));

  jvm_breakpoint_->OnJvmBreakpointHit(kBreakpointThread, kBreakpointMethod,
                                      100371);
}

TEST_F(JvmBreakpointTest, DynamicLoggerWithParameters) {
  Create(BreakpointBuilder(*breakpoint_template_)
             .set_action(BreakpointModel::Action::LOG)
             .set_log_level(BreakpointModel::LogLevel::WARNING)
             .set_log_message_format("$0 should be 56 and $1 should be 36")
             .add_expression("7 * 8")
             .add_expression("6 * 6")
             .build());

  EXPECT_CALL(dynamic_logger_,
              Log(BreakpointModel::LogLevel::WARNING, _,
                  "LOGPOINT: 56 should be 56 and 36 should be 36"))
      .Times(1);

  jvm_breakpoint_->OnJvmBreakpointHit(kBreakpointThread, kBreakpointMethod,
                                      100371);
}

TEST_F(JvmBreakpointTest, DynamicLogQuotaExceededAfterSuccess) {
  // Initialize the global quota to only allow one log message ever.
  LeakyBucket global_quota(1, 0);
  EXPECT_CALL(breakpoints_manager_, GetGlobalDynamicLogLimiter())
      .WillRepeatedly(Return(&global_quota));

  Create(BreakpointBuilder(*breakpoint_template_)
             .set_action(BreakpointModel::Action::LOG)
             .set_log_level(BreakpointModel::LogLevel::WARNING)
             .set_log_message_format("log worked")
             .build());

  {
    InSequence sequence;

    EXPECT_CALL(dynamic_logger_, Log(BreakpointModel::LogLevel::WARNING, _,
                                     "LOGPOINT: log worked"))
        .Times(1);

    EXPECT_CALL(dynamic_logger_,
                Log(BreakpointModel::LogLevel::WARNING, _,
                    std::string("LOGPOINT: ") + DynamicLogOutOfCallQuota))
        .Times(1);
  }

  for (int i = 0; i < 100; ++i) {
    jvm_breakpoint_->OnJvmBreakpointHit(kBreakpointThread, kBreakpointMethod,
                                        100371);
  }
}

TEST_F(JvmBreakpointTest, DynamicLogBytesQuotaExceededAfterSuccess) {
  // Initialize the global quota to only allow one log message ever.
  LeakyBucket global_bytes_quota(arraysize("LOGPOINT: log worked"), 0);
  EXPECT_CALL(breakpoints_manager_, GetGlobalDynamicLogBytesLimiter())
      .WillRepeatedly(Return(&global_bytes_quota));

  Create(BreakpointBuilder(*breakpoint_template_)
             .set_action(BreakpointModel::Action::LOG)
             .set_log_level(BreakpointModel::LogLevel::WARNING)
             .set_log_message_format("log worked")
             .build());

  {
    InSequence sequence;

    EXPECT_CALL(dynamic_logger_, Log(BreakpointModel::LogLevel::WARNING, _,
                                     "LOGPOINT: log worked"))
        .Times(1);

    EXPECT_CALL(dynamic_logger_,
                Log(BreakpointModel::LogLevel::WARNING, _,
                    std::string("LOGPOINT: ") + DynamicLogOutOfBytesQuota))
        .Times(1);
  }

  for (int i = 0; i < 100; ++i) {
    jvm_breakpoint_->OnJvmBreakpointHit(kBreakpointThread, kBreakpointMethod,
                                        100371);
  }
}

TEST_F(JvmBreakpointTest, DynamicLogQuotaExceededOnFirstLog) {
  LeakyBucket global_quota(0, 0);
  EXPECT_CALL(breakpoints_manager_, GetGlobalDynamicLogLimiter())
      .WillRepeatedly(Return(&global_quota));

  Create(BreakpointBuilder(*breakpoint_template_)
             .set_action(BreakpointModel::Action::LOG)
             .set_log_level(BreakpointModel::LogLevel::WARNING)
             .set_log_message_format("this is unexpected")
             .build());

  EXPECT_CALL(dynamic_logger_,
              Log(BreakpointModel::LogLevel::WARNING, _,
                  std::string("LOGPOINT: ") + DynamicLogOutOfCallQuota))
      .Times(1);

  for (int i = 0; i < 10; ++i) {
    jvm_breakpoint_->OnJvmBreakpointHit(kBreakpointThread, kBreakpointMethod,
                                        100371);
  }
}

TEST_F(JvmBreakpointTest, DynamicLogBytesQuotaExceededOnFirstLog) {
  LeakyBucket global_bytes_quota(0, 0);
  EXPECT_CALL(breakpoints_manager_, GetGlobalDynamicLogBytesLimiter())
      .WillRepeatedly(Return(&global_bytes_quota));

  Create(BreakpointBuilder(*breakpoint_template_)
             .set_action(BreakpointModel::Action::LOG)
             .set_log_level(BreakpointModel::LogLevel::WARNING)
             .set_log_message_format("this is unexpected")
             .build());

  EXPECT_CALL(dynamic_logger_,
              Log(BreakpointModel::LogLevel::WARNING, _,
                  std::string("LOGPOINT: ") + DynamicLogOutOfBytesQuota))
      .Times(1);

  for (int i = 0; i < 10; ++i) {
    jvm_breakpoint_->OnJvmBreakpointHit(kBreakpointThread, kBreakpointMethod,
                                        100371);
  }
}

TEST_F(JvmBreakpointTest, DynamicLogQuotaRecovery) {
  // Initialize the global quota to only allow one log message ever.
  LeakyBucket global_quota(0, 0);
  EXPECT_CALL(breakpoints_manager_, GetGlobalDynamicLogLimiter())
      .Times(1)
      .WillRepeatedly(Return(&global_quota))
      .RetiresOnSaturation();

  Create(BreakpointBuilder(*breakpoint_template_)
             .set_action(BreakpointModel::Action::LOG)
             .set_log_level(BreakpointModel::LogLevel::WARNING)
             .set_log_message_format("log worked")
             .build());

  {
    InSequence sequence;

    EXPECT_CALL(dynamic_logger_,
                Log(BreakpointModel::LogLevel::WARNING, _,
                    std::string("LOGPOINT: ") + DynamicLogOutOfCallQuota))
        .Times(1);

    EXPECT_CALL(dynamic_logger_, Log(BreakpointModel::LogLevel::WARNING, _,
                                     "LOGPOINT: log worked"))
        .Times(3);
  }

  for (int i = 0; i < 5; ++i) {
    jvm_breakpoint_->OnJvmBreakpointHit(kBreakpointThread, kBreakpointMethod,
                                        100371);
  }

  usleep((absl::GetFlag(FLAGS_dynamic_log_quota_recovery_ms) + 50) * 1000);

  for (int i = 0; i < 3; ++i) {
    jvm_breakpoint_->OnJvmBreakpointHit(kBreakpointThread, kBreakpointMethod,
                                        100371);
  }
}

TEST_F(JvmBreakpointTest, DynamicLogBytesQuotaRecovery) {
  // Initialize the global bytes quota to only allow one log message ever.
  LeakyBucket global_bytes_quota(0, 0);
  EXPECT_CALL(breakpoints_manager_, GetGlobalDynamicLogBytesLimiter())
      .Times(1)
      .WillRepeatedly(Return(&global_bytes_quota))
      .RetiresOnSaturation();

  Create(BreakpointBuilder(*breakpoint_template_)
             .set_action(BreakpointModel::Action::LOG)
             .set_log_level(BreakpointModel::LogLevel::WARNING)
             .set_log_message_format("log worked")
             .build());

  {
    InSequence sequence;

    EXPECT_CALL(dynamic_logger_,
                Log(BreakpointModel::LogLevel::WARNING, _,
                    std::string("LOGPOINT: ") + DynamicLogOutOfBytesQuota))
        .Times(1);

    EXPECT_CALL(dynamic_logger_, Log(BreakpointModel::LogLevel::WARNING, _,
                                     "LOGPOINT: log worked"))
        .Times(3);
  }

  for (int i = 0; i < 5; ++i) {
    jvm_breakpoint_->OnJvmBreakpointHit(kBreakpointThread, kBreakpointMethod,
                                        100371);
  }

  usleep((absl::GetFlag(FLAGS_dynamic_log_quota_recovery_ms) + 50) * 1000);

  for (int i = 0; i < 3; ++i) {
    jvm_breakpoint_->OnJvmBreakpointHit(kBreakpointThread, kBreakpointMethod,
                                        100371);
  }
}

TEST_F(JvmBreakpointTest, DynamicLogPerBreakpointQuotaExceeded) {
  absl::FlagSaver fs;

  absl::SetFlag(&FLAGS_max_dynamic_log_rate, 2);

  Create(BreakpointBuilder(*breakpoint_template_)
             .set_action(BreakpointModel::Action::LOG)
             .set_log_level(BreakpointModel::LogLevel::WARNING)
             .set_log_message_format("log worked")
             .build());

  {
    InSequence sequence;

    EXPECT_CALL(dynamic_logger_, Log(BreakpointModel::LogLevel::WARNING, _,
                                     "LOGPOINT: log worked"))
        .Times(AtMost(30));

    EXPECT_CALL(dynamic_logger_,
                Log(BreakpointModel::LogLevel::WARNING, _,
                    std::string("LOGPOINT: ") + DynamicLogOutOfCallQuota))
        .Times(1);
  }

  for (int i = 0; i < 100; ++i) {
    jvm_breakpoint_->OnJvmBreakpointHit(kBreakpointThread, kBreakpointMethod,
                                        100371);
  }
}

TEST_F(JvmBreakpointTest, DynamicLogBytesPerBreakpointQuotaExceeded) {
  absl::FlagSaver fs;

  // Use 100 bytes per second, which should allow at most 4-5 log lines before
  // hitting the quota.
  absl::SetFlag(&FLAGS_max_dynamic_log_bytes_rate, 100);

  Create(BreakpointBuilder(*breakpoint_template_)
             .set_action(BreakpointModel::Action::LOG)
             .set_log_level(BreakpointModel::LogLevel::WARNING)
             .set_log_message_format("log worked")
             .build());

  {
    InSequence sequence;

    EXPECT_CALL(dynamic_logger_, Log(BreakpointModel::LogLevel::WARNING, _,
                                     "LOGPOINT: log worked"))
        .Times(AtMost(30));

    EXPECT_CALL(dynamic_logger_,
                Log(BreakpointModel::LogLevel::WARNING, _,
                    std::string("LOGPOINT: ") + DynamicLogOutOfBytesQuota))
        .Times(1);
  }

  for (int i = 0; i < 100; ++i) {
    jvm_breakpoint_->OnJvmBreakpointHit(kBreakpointThread, kBreakpointMethod,
                                        100371);
  }
}

TEST_F(JvmBreakpointTest, PreemptiveStatusSet) {
  std::unique_ptr<StatusMessageModel> setup_error =
      StatusMessageBuilder().set_error().set_format("test format").build();

  jvm_breakpoint_ = std::make_shared<JvmBreakpoint>(
      &scheduler_, &evaluators_, &format_queue_, &dynamic_logger_,
      &breakpoints_manager_, std::move(setup_error),
      BreakpointBuilder(*breakpoint_template_).build());

  jvm_breakpoint_->Initialize();

  auto result = format_queue_.FormatAndPop();
  ASSERT_NE(nullptr, result);

  EXPECT_TRUE(result->status->is_error);
  EXPECT_EQ("test format", result->status->description.format);
}

}  // namespace cdbg
}  // namespace devtools
