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

#include "src/agent/capture_data_collector.h"

#include <cstdint>

#include "src/agent/common.h"
#include "src/agent/expression_evaluator.h"
#include "src/agent/expression_util.h"
#include "src/agent/instance_field_reader.h"
#include "src/agent/jvm_evaluators.h"
#include "src/agent/local_variable_reader.h"
#include "src/agent/messages.h"
#include "src/agent/model_json.h"
#include "src/agent/model_util.h"
#include "src/agent/static_field_reader.h"
#include "src/agent/value_formatter.h"

#include "gmock/gmock.h"
#include "tests/agent/json_eq_matcher.h"
#include "tests/agent/fake_instance_field_reader.h"
#include "tests/agent/mock_breakpoint_labels_provider.h"
#include "tests/agent/mock_class_indexer.h"
#include "tests/agent/mock_eval_call_stack.h"
#include "tests/agent/mock_jni_env.h"
#include "tests/agent/mock_jvmti_env.h"
#include "tests/agent/mock_object_evaluator.h"
#include "tests/agent/mock_user_id_provider.h"
#include "tests/agent/named_jvariant_test_util.h"

ABSL_DECLARE_FLAG(bool, cdbg_capture_user_id);

using testing::_;
using testing::DoAll;
using testing::Exactly;
using testing::IsNull;
using testing::NiceMock;
using testing::NotNull;
using testing::Return;
using testing::ReturnRef;
using testing::Invoke;
using testing::InvokeArgument;
using testing::SetArgPointee;

namespace devtools {
namespace cdbg {

static const jthread thread = reinterpret_cast<jthread>(0x67125374);

static const jmethodID method1 = reinterpret_cast<jmethodID>(0x10001);
static const jmethodID method2 = reinterpret_cast<jmethodID>(0x20002);

static const jobject object1 = reinterpret_cast<jobject>(0x101);
static const jobject object2 = reinterpret_cast<jobject>(0x102);
static const jobject object3 = reinterpret_cast<jobject>(0x103);

class CaptureDataCollectorTest : public ::testing::Test {
 protected:
  // Override ReadLocalVariables to inject fake local variables.
  class MockedCollector : public CaptureDataCollector {
   public:
    MockedCollector(
        JvmEvaluators* evaluators,
        std::map<int, std::vector<NamedJVariant>>* fake_locals)
        : CaptureDataCollector(evaluators),
          fake_locals_(fake_locals) {
    }

   protected:
    // Reads local variables at a particular call frame. The function is marked
    // as virtual and protected for unit testing purposes.
    void ReadLocalVariables(
        const EvaluationContext& evaluation_context,
        jmethodID method,
        jlocation location,
        std::vector<NamedJVariant>* arguments,
        std::vector<NamedJVariant>* local_variables) override {
      local_variables->swap((*fake_locals_)[evaluation_context.frame_depth]);
    }

   private:
    std::map<int, std::vector<NamedJVariant>>* const fake_locals_;
  };

  CaptureDataCollectorTest()
      : global_jvm_(&jvmti_, &jni_),
        named_jvariant_util_(&jni_) {
  }

  static jclass GetObjectClass(jobject obj) {
    return reinterpret_cast<jclass>(reinterpret_cast<uint64_t>(obj) |
                                    0x1000000);
  }

  void SetUp() override {
    EXPECT_CALL(jvmti_, GetObjectHashCode(_, NotNull()))
        .WillRepeatedly(DoAll(SetArgPointee<1>(0), Return(JVMTI_ERROR_NONE)));

    EXPECT_CALL(jni_, IsSameObject(_, _))
        .WillRepeatedly(Invoke([] (jobject obj1, jobject obj2) {
          return obj1 == obj2;
        }));

    EXPECT_CALL(jni_, DeleteLocalRef(_))
        .WillRepeatedly(Return());

    EXPECT_CALL(jni_, NewGlobalRef(_))
        .WillRepeatedly(Invoke([] (jobject obj) {
          return obj;
        }));

    EXPECT_CALL(jni_, DeleteGlobalRef(_))
        .WillRepeatedly(Return());

    EXPECT_CALL(jni_, GetObjectClass(_))
        .WillRepeatedly(Invoke([] (jobject obj) {
          return GetObjectClass(obj);
        }));

    const struct {
      jclass cls;
      const char* signature;
    } known_signatures[] = {
        { GetObjectClass(object1), "LObject1;" },
        { GetObjectClass(object2), "LObject2;" },
        { GetObjectClass(object3), "LObject3;" }
    };

    for (const auto& known_signature : known_signatures) {
      EXPECT_CALL(jvmti_,
                  GetClassSignature(known_signature.cls, NotNull(), nullptr))
          .WillRepeatedly(DoAll(
              SetArgPointee<1>(const_cast<char*>(known_signature.signature)),
              Return(JVMTI_ERROR_NONE)));
    }

    EXPECT_CALL(jni_, GetObjectRefType(_))
        .WillRepeatedly(Return(JNILocalRefType));

    EXPECT_CALL(jni_, ExceptionOccurred())
        .WillRepeatedly(Return(nullptr));
  }

  std::unique_ptr<BreakpointModel> Collect() {
    return CollectWithLabelsAndUserId(nullptr, nullptr);
  }

  std::unique_ptr<BreakpointModel> CollectWithLabels(
      MockBreakpointLabelsProvider* labels_provider,
      const std::map<std::string, std::string>& pre_existing_labels = {}) {
    return CollectWithLabelsAndUserId(labels_provider, nullptr,
                                      pre_existing_labels);
  }

  std::unique_ptr<BreakpointModel> CollectWithUserId(
      MockUserIdProvider* user_id_provider) {
    return CollectWithLabelsAndUserId(nullptr, user_id_provider);
  }

  // Collects data given the expected state of all the mock evaluators. Then
  // formats the collected data into "Breakpoint" structure. Finally exports
  // the formatted data structure as JSON.
  std::unique_ptr<BreakpointModel> CollectWithLabelsAndUserId(
      MockBreakpointLabelsProvider* labels_provider,
      MockUserIdProvider* user_id_provider,
      const std::map<std::string, std::string>& pre_existing_labels = {}) {
    if (labels_provider == nullptr) {
      labels_provider = new NiceMock<MockBreakpointLabelsProvider>;
    }

    if (absl::GetFlag(FLAGS_cdbg_capture_user_id) &&
        user_id_provider == nullptr) {
      user_id_provider = new NiceMock<MockUserIdProvider>;
    }

    EXPECT_CALL(*labels_provider, Collect()).Times(Exactly(1));

    if (absl::GetFlag(FLAGS_cdbg_capture_user_id)) {
      EXPECT_CALL(*user_id_provider, Collect()).Times(Exactly(1));
    }

    JvmEvaluators evaluators;
    evaluators.class_indexer = &class_indexer_;
    evaluators.eval_call_stack = &eval_call_stack_;
    evaluators.object_evaluator = &object_evaluator_;
    evaluators.method_caller_factory = [] (Config::MethodCallQuotaType type) {
      return nullptr;
    };
    evaluators.labels_factory = [labels_provider] () {
      return std::unique_ptr<BreakpointLabelsProvider>(labels_provider);
    };
    evaluators.user_id_provider_factory = [user_id_provider]() {
      return std::unique_ptr<UserIdProvider>(user_id_provider);
    };

    MockedCollector collector(&evaluators, &fake_locals_);
    collector.Collect(watches_, thread);

    BreakpointBuilder breakpointBuilder;
    breakpointBuilder.set_id("BP");

    for (const auto& existing_label : pre_existing_labels) {
      breakpointBuilder.add_label(existing_label.first, existing_label.second);
    }

    std::unique_ptr<BreakpointModel> breakpoint = breakpointBuilder.build();

    collector.Format(breakpoint.get());
    collector.ReleaseRefs();

    return breakpoint;
  }

  void ExpectEvaluateLocalVariables(
      int depth,
      std::vector<NamedJVariant>* locals) {
    fake_locals_[depth].swap(*locals);
  }

  void ExpectEvaluateObjectMembers(
      jobject obj,
      const std::vector<NamedJVariant>& fake_members) {
    EXPECT_CALL(object_evaluator_, Evaluate(_, obj, _, NotNull()))
        .WillRepeatedly(Invoke([this, &fake_members] (
            MethodCaller* method_caller,
            jobject obj,
            bool is_watch_expression,
            std::vector<NamedJVariant>* members) {
          named_jvariant_util_.CopyNamedJVariant(fake_members, members);
        }));
  }

 protected:
  MockJvmtiEnv jvmti_;
  MockJNIEnv jni_;
  GlobalJvmEnv global_jvm_;
  MockEvalCallStack eval_call_stack_;
  MockClassIndexer class_indexer_;
  NiceMock<MockObjectEvaluator> object_evaluator_;
  NamedJvariantTestUtil named_jvariant_util_;

  // Fake values of local variables for each call frame.
  std::map<int, std::vector<NamedJVariant>> fake_locals_;

  // Watched expressions to evaluate on data collection.
  std::vector<CompiledExpression> watches_;

  // Buffer of simulated Java strings.
  std::list<std::vector<jchar>> jstring_buffers_;
};


TEST_F(CaptureDataCollectorTest, Empty) {
  EXPECT_CALL(eval_call_stack_, Read(thread, NotNull()))
      .WillOnce(Return());

  ExpectJsonEq(
      *BreakpointBuilder()
          .set_id("BP")
          .add_capture_buffer_full_variable_table_item()
          .build(),
      Collect().get());
}


TEST_F(CaptureDataCollectorTest, EvalCallStack) {
  const std::vector<EvalCallStack::JvmFrame> jvm_frames = {
    { { method1, 100 }, 0 },
    { { method2, 200 }, 1 },
    { { method1, 150 }, 2 },
    { { method1, 100 }, 0 }
  };

  const EvalCallStack::FrameInfo frame_infos[] = {
      {
          "LClass1;",     // class_signature
          std::string(),  // class_generic
          "Method1",      // method_name
          "Class1.java",  // source_file_name
          10              // line_number
      },
      {
          "LClass2;",     // class_signature
          std::string(),  // class_generic
          "Method2",      // method_name
          "Class2.java",  // source_file_name
          20              // line_number
      },
      {
          "LClass1;",     // class_signature
          std::string(),  // class_generic
          "Method1",      // method_name
          "Class1.java",  // source_file_name
          15              // line_number
      }};

  EXPECT_CALL(eval_call_stack_, Read(thread, NotNull()))
      .WillOnce(SetArgPointee<1>(jvm_frames));

  for (int i = 0; i < arraysize(frame_infos); ++i) {
    EXPECT_CALL(eval_call_stack_, ResolveCallFrameKey(i))
        .WillRepeatedly(ReturnRef(frame_infos[i]));
  }

  ExpectJsonEq(
      *BreakpointBuilder()
          .set_id("BP")
          .add_stack_frame(StackFrameBuilder()
              .set_function("Class1.Method1")
              .set_location("Class1.java", 10))
          .add_stack_frame(StackFrameBuilder()
              .set_function("Class2.Method2")
              .set_location("Class2.java", 20))
          .add_stack_frame(StackFrameBuilder()
              .set_function("Class1.Method1")
              .set_location("Class1.java", 15))
          .add_stack_frame(StackFrameBuilder()
              .set_function("Class1.Method1")
              .set_location("Class1.java", 10))
          .add_capture_buffer_full_variable_table_item()
          .build(),
      Collect().get());
}


TEST_F(CaptureDataCollectorTest, LocalVariables) {
  EvalCallStack::JvmFrame jvm_frame = { { method1, 100 }, 0 };
  const std::vector<EvalCallStack::JvmFrame> jvm_frames(10, jvm_frame);

  const EvalCallStack::FrameInfo frame_info = {
      "LClass1;",     // class_signature
      std::string(),  // class_generic
      "Method1",      // method_name
      "Class1.java",  // source_file_name
      10              // line_number
  };

  std::vector<NamedJVariant> locals[10];

  // Frame 0.
  named_jvariant_util_.AddNumericVariable("i", 83, &locals[0]);
  named_jvariant_util_.AddStringVariable(
      "my_str",
      "this is a string",
      &locals[0]);
  named_jvariant_util_.AddNumericVariable("PI", 3.1415, &locals[0]);

  // Frame 1.
  named_jvariant_util_.AddNumericVariable("a", 1, &locals[1]);

  // Frame 2.
  named_jvariant_util_.AddNumericVariable("b", 2, &locals[2]);

  // Frame 3.
  named_jvariant_util_.AddNumericVariable("c", 3, &locals[3]);

  // Frame 4.
  named_jvariant_util_.AddNumericVariable("d", 4, &locals[4]);

  // Frame 5.
  named_jvariant_util_.AddNumericVariable("e", 5, &locals[5]);

  // Frame 6.
  named_jvariant_util_.AddNumericVariable("f", 6, &locals[6]);

  // Frame 7.
  named_jvariant_util_.AddNumericVariable("g", 7, &locals[7]);

  // Frame 8.
  named_jvariant_util_.AddNumericVariable("h", 8, &locals[8]);

  // Frame 9.
  named_jvariant_util_.AddNumericVariable("i", 9, &locals[9]);

  EXPECT_CALL(eval_call_stack_, Read(thread, NotNull()))
      .WillOnce(SetArgPointee<1>(jvm_frames));

  EXPECT_CALL(eval_call_stack_, ResolveCallFrameKey(0))
      .WillRepeatedly(ReturnRef(frame_info));

  for (int depth = 0; depth < arraysize(locals); ++depth) {
    ExpectEvaluateLocalVariables(depth, &locals[depth]);
  }

  ExpectJsonEq(
      *BreakpointBuilder()
          .set_id("BP")
          .add_stack_frame(StackFrameBuilder()
              .set_function("Class1.Method1")
              .set_location("Class1.java", 10)
              .add_local(VariableBuilder()
                  .set_name("i")
                  .set_value("83")
                  .set_type("int"))
              .add_local(VariableBuilder()
                  .set_name("my_str")
                  .set_value("\"this is a string\"")
                  .set_type("String"))
              .add_local(VariableBuilder()
                  .set_name("PI")
                  .set_value("3.1415")
                  .set_type("double")))
          .add_stack_frame(StackFrameBuilder()
              .set_function("Class1.Method1")
              .set_location("Class1.java", 10)
              .add_local(VariableBuilder()
                  .set_name("a")
                  .set_value("1")
                  .set_type("int")))
          .add_stack_frame(StackFrameBuilder()
              .set_function("Class1.Method1")
              .set_location("Class1.java", 10)
              .add_local(VariableBuilder()
                  .set_name("b")
                  .set_value("2")
                  .set_type("int")))
          .add_stack_frame(StackFrameBuilder()
              .set_function("Class1.Method1")
              .set_location("Class1.java", 10)
              .add_local(VariableBuilder()
                  .set_name("c")
                  .set_value("3")
                  .set_type("int")))
          .add_stack_frame(StackFrameBuilder()
              .set_function("Class1.Method1")
              .set_location("Class1.java", 10)
              .add_local(VariableBuilder()
                  .set_name("d")
                  .set_value("4")
                  .set_type("int")))
          .add_stack_frame(StackFrameBuilder()
              .set_function("Class1.Method1")
              .set_location("Class1.java", 10))
          .add_stack_frame(StackFrameBuilder()
              .set_function("Class1.Method1")
              .set_location("Class1.java", 10))
          .add_stack_frame(StackFrameBuilder()
              .set_function("Class1.Method1")
              .set_location("Class1.java", 10))
          .add_stack_frame(StackFrameBuilder()
              .set_function("Class1.Method1")
              .set_location("Class1.java", 10))
          .add_stack_frame(StackFrameBuilder()
              .set_function("Class1.Method1")
              .set_location("Class1.java", 10))
          .add_capture_buffer_full_variable_table_item()
          .build(),
      Collect().get());
}


TEST_F(CaptureDataCollectorTest, ObjectsRef) {
  EvalCallStack::JvmFrame jvm_frame = { { method1, 100 }, 0 };

  const EvalCallStack::FrameInfo frame_info = {
      "LClass1;",     // class_signature
      std::string(),  // class_generic
      "Method1",      // method_name
      "Class1.java",  // source_file_name
      10              // line_number
  };

  std::vector<NamedJVariant> locals;
  named_jvariant_util_.AddRefVariable("ref1_a", object1, &locals);
  named_jvariant_util_.AddRefVariable("ref2_b", object2, &locals);
  named_jvariant_util_.AddRefVariable("ref3_a", object1, &locals);

  std::vector<NamedJVariant> object1_members;
  named_jvariant_util_.AddRefVariable("ref_self", object1, &object1_members);
  named_jvariant_util_.AddRefVariable("ref_object2", object2, &object1_members);
  named_jvariant_util_.AddStringVariable("a", "first", &object1_members);

  std::vector<NamedJVariant> object2_members;
  named_jvariant_util_.AddRefVariable("ref_object1", object1, &object2_members);
  named_jvariant_util_.AddRefVariable("ref_object3", object3, &object2_members);
  named_jvariant_util_.AddStringVariable("b", "second", &object2_members);

  std::vector<NamedJVariant> object3_members;
  named_jvariant_util_.AddRefVariable("ref_object1", object1, &object3_members);
  named_jvariant_util_.AddStringVariable("c", "third", &object3_members);

  EXPECT_CALL(eval_call_stack_, Read(thread, NotNull()))
      .WillOnce(SetArgPointee<1>(
          std::vector<EvalCallStack::JvmFrame>(1, jvm_frame)));

  EXPECT_CALL(eval_call_stack_, ResolveCallFrameKey(0))
      .WillRepeatedly(ReturnRef(frame_info));

  ExpectEvaluateLocalVariables(0, &locals);

  ExpectEvaluateObjectMembers(object1, object1_members);
  ExpectEvaluateObjectMembers(object2, object2_members);
  ExpectEvaluateObjectMembers(object3, object3_members);

  ExpectJsonEq(
      *BreakpointBuilder()
          .set_id("BP")
          .add_stack_frame(StackFrameBuilder()
              .set_function("Class1.Method1")
              .set_location("Class1.java", 10)
              .add_local(VariableBuilder()
                  .set_name("ref1_a")
                  .set_var_table_index(1))
              .add_local(VariableBuilder()
                  .set_name("ref2_b")
                  .set_var_table_index(2))
              .add_local(VariableBuilder()
                  .set_name("ref3_a")
                  .set_var_table_index(1)))
          .add_capture_buffer_full_variable_table_item()
          .add_variable_table_item(VariableBuilder()
              .set_type("Object1")
              .add_member(VariableBuilder()
                  .set_name("ref_self")
                  .set_var_table_index(1))
              .add_member(VariableBuilder()
                  .set_name("ref_object2")
                  .set_var_table_index(2))
              .add_member(VariableBuilder()
                  .set_name("a")
                  .set_value("\"first\"")
                  .set_type("String")))
          .add_variable_table_item(VariableBuilder()
              .set_type("Object2")
              .add_member(VariableBuilder()
                  .set_name("ref_object1")
                  .set_var_table_index(1))
              .add_member(VariableBuilder()
                  .set_name("ref_object3")
                  .set_var_table_index(3))
              .add_member(VariableBuilder()
                  .set_name("b")
                  .set_value("\"second\"")
                  .set_type("String")))
          .add_variable_table_item(VariableBuilder()
              .set_type("Object3")
              .add_member(VariableBuilder()
                  .set_name("ref_object1")
                  .set_var_table_index(1))
              .add_member(VariableBuilder()
                  .set_name("c")
                  .set_value("\"third\"")
                  .set_type("String")))
          .build(),
      Collect().get());
}

TEST_F(CaptureDataCollectorTest, ByteArray) {
  EvalCallStack::JvmFrame jvm_frame = { { method1, 100 }, 0 };

  const EvalCallStack::FrameInfo frame_info = {
      "LClass1;",     // class_signature
      std::string(),  // class_generic
      "Method1",      // method_name
      "Class1.java",  // source_file_name
      10              // line_number
  };

  std::vector<NamedJVariant> locals;
  named_jvariant_util_.AddRefVariable("bytes1", object1, &locals);
  named_jvariant_util_.AddRefVariable("bytes2", object2, &locals);

  // base64 is w7w=. utf8 is ü.
  std::vector<NamedJVariant> bytes1_members;
  named_jvariant_util_.AddNumericVariable<jint>("length", 2, &bytes1_members);
  named_jvariant_util_.AddNumericVariable<jbyte>("[0]", -61, &bytes1_members);
  named_jvariant_util_.AddNumericVariable<jbyte>("[1]", -68, &bytes1_members);

  // base64 is /w==. Invalid utf8.
  std::vector<NamedJVariant> bytes2_members;
  named_jvariant_util_.AddNumericVariable<jint>("length", 1, &bytes2_members);
  named_jvariant_util_.AddNumericVariable<jbyte>("[0]", -1, &bytes2_members);

  EXPECT_CALL(eval_call_stack_, Read(thread, NotNull()))
      .WillOnce(SetArgPointee<1>(
          std::vector<EvalCallStack::JvmFrame>(1, jvm_frame)));

  EXPECT_CALL(eval_call_stack_, ResolveCallFrameKey(0))
      .WillRepeatedly(ReturnRef(frame_info));

  EXPECT_CALL(jvmti_, GetClassSignature(_, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(const_cast<char*>("[B")),
                            Return(JVMTI_ERROR_NONE)));

  ExpectEvaluateLocalVariables(0, &locals);
  ExpectEvaluateObjectMembers(object1, bytes1_members);
  ExpectEvaluateObjectMembers(object2, bytes2_members);

  ExpectJsonEq(
      *BreakpointBuilder()
          .set_id("BP")
          .add_stack_frame(StackFrameBuilder()
              .set_function("Class1.Method1")
              .set_location("Class1.java", 10)
              .add_local(VariableBuilder()
                  .set_name("bytes1")
                  .set_var_table_index(1))
              .add_local(VariableBuilder()
                  .set_name("bytes2")
                  .set_var_table_index(2)))
          .add_capture_buffer_full_variable_table_item()
          .add_variable_table_item(VariableBuilder()
              .set_type("byte[]")
              .add_member(VariableBuilder()
                  .set_name("$utf8")
                  .set_value("\"ü\"")
                  .set_type("String"))
              .add_member(VariableBuilder()
                  .set_name("$base64")
                  .set_value("w7w=")
                  .set_type("String"))
              .add_member(VariableBuilder()
                  .set_name("length")
                  .set_value("2")
                  .set_type("int"))
              .add_member(VariableBuilder()
                  .set_name("[0]")
                  .set_value("-61")
                  .set_type("byte"))
              .add_member(VariableBuilder()
                  .set_name("[1]")
                  .set_value("-68")
                  .set_type("byte")))
          .add_variable_table_item(VariableBuilder()
              .set_type("byte[]")
              .add_member(VariableBuilder()
                  .set_name("$base64")
                  .set_value("/w==")
                  .set_type("String"))
              .add_member(VariableBuilder()
                  .set_name("length")
                  .set_value("1")
                  .set_type("int"))
              .add_member(VariableBuilder()
                  .set_name("[0]")
                  .set_value("-1")
                  .set_type("byte")))
          .build(),
      Collect().get());
}

TEST_F(CaptureDataCollectorTest, Quota) {
  EvalCallStack::JvmFrame jvm_frame = { { method1, 100 }, 0 };

  const EvalCallStack::FrameInfo frame_info = {
      "LClass1;",     // class_signature
      std::string(),  // class_generic
      "Method1",      // method_name
      "Class1.java",  // source_file_name
      10              // line_number
  };

  StackFrameBuilder expected_frame;
  expected_frame.set_function("Class1.Method1");
  expected_frame.set_location("Class1.java", 10);
  expected_frame.add_local(VariableBuilder()
      .set_name("ref1_a")
      .set_var_table_index(1));

  std::vector<NamedJVariant> locals;

  named_jvariant_util_.AddRefVariable("ref1_a", object1, &locals);

  int remaining_space = kBreakpointMaxCaptureSize - 50;
  while (remaining_space > 0) {
    // Use the same name for all local variables.
    const std::string loc_name = "loc";

    // Additional size that each local variable will occupy (name and 2
    // characters for double quotes). This is the same formula used in
    // "ValueFormatter::GetTotalDataSize".
    const int extra_size = 2 + loc_name.size();

    // Value of the current string variable.
    const std::string loc_string(
        std::min(remaining_space - extra_size, kDefaultMaxStringLength - 20),
        'A');

    named_jvariant_util_.AddStringVariable(loc_name, loc_string, &locals);

    expected_frame.add_local(VariableBuilder()
        .set_name(loc_name)
        .set_value("\"" + loc_string + "\"")
        .set_type("String"));

    remaining_space -= loc_string.size() + extra_size;
  }

  std::vector<NamedJVariant> object1_members;
  named_jvariant_util_.AddRefVariable("ref_object2", object2, &object1_members);
  named_jvariant_util_.AddStringVariable("a", std::string(100, 'B'),
                                         &object1_members);

  std::vector<NamedJVariant> object2_members;
  named_jvariant_util_.AddStringVariable("b", "second", &object2_members);

  EXPECT_CALL(eval_call_stack_, Read(thread, NotNull()))
      .WillOnce(SetArgPointee<1>(
          std::vector<EvalCallStack::JvmFrame>(1, jvm_frame)));

  EXPECT_CALL(eval_call_stack_, ResolveCallFrameKey(0))
      .WillRepeatedly(ReturnRef(frame_info));

  ExpectEvaluateLocalVariables(0, &locals);

  ExpectEvaluateObjectMembers(object1, object1_members);
  ExpectEvaluateObjectMembers(object2, object2_members);

  ExpectJsonEq(
      *BreakpointBuilder()
           .set_id("BP")
           .add_stack_frame(expected_frame)
           .add_capture_buffer_full_variable_table_item()
           .add_variable_table_item(
               VariableBuilder()
                   .set_type("Object1")
                   .add_member(VariableBuilder()
                                   .set_name("ref_object2")
                                   .set_var_table_index(0))
                   .add_member(
                       VariableBuilder()
                           .set_name("a")
                           .set_value("\"" + std::string(100, 'B') + "\"")
                           .set_type("String")))
           .build(),
      Collect().get());
}


TEST_F(CaptureDataCollectorTest, StackFrameLocationConstruction) {
  const struct {
    std::string class_signature;
    std::string expected_function_name;
    std::string expected_source_path;
  } test_cases[] = {
    {
      "LClass1;",
      "Class1.Method1",
      "Class1.java"
    },
    {
      "Lorg/Class1;",
      "org.Class1.Method1",
      "org/Class1.java"
    },
    {
      "Lorg/prod/Class1;",
      "org.prod.Class1.Method1",
      "org/prod/Class1.java"
    },
    {
      "La/b/c/d/e/f/g/h/Class1;",
      "a.b.c.d.e.f.g.h.Class1.Method1",
      "a/b/c/d/e/f/g/h/Class1.java"
    },
    {
      "Lorg/prod/Class1$Inner;",
      "org.prod.Class1.Inner.Method1",
      "org/prod/Class1.java"
    },
    {
      "Lorg/prod/Class1$Inn1$Inn2;",
      "org.prod.Class1.Inn1.Inn2.Method1",
      "org/prod/Class1.java"
    },

    // Incorrect syntax that Collector still accepts.
    {
      "",
      ".Method1",
      "Class1.java"
    },
    {
      "L",
      ".Method1",
      "Class1.java"
    },
    {
      "L;",
      ".Method1",
      "Class1.java"
    },
    {
      ";",
      ".Method1",
      "Class1.java"
    },
    {
      "Class1;",
      "Class1.Method1",
      "Class1.java"
    },
    {
      "LClass1",
      "Class1.Method1",
      "Class1.java"
    },
    {
      "Class1",
      "Class1.Method1",
      "Class1.java"
    },
    {
      "A/B/Class1;",
      "A.B.Class1.Method1",
      "A/B/Class1.java"
    },
    {
      "LA/B/Class1",
      "A.B.Class1.Method1",
      "A/B/Class1.java"
    },
    {
      "A/B/Class1",
      "A.B.Class1.Method1",
      "A/B/Class1.java"
    },
    {
      "LA.Class1;",
      "A.Class1.Method1",
      "Class1.java"
    }
  };

  for (const auto& test_case : test_cases) {
    const EvalCallStack::JvmFrame jvm_frame = { { method1, 100 }, 0 };

    const EvalCallStack::FrameInfo frame_info = {
        test_case.class_signature,
        std::string(),  // class_generic
        "Method1",      // method_name
        "Class1.java",  // source_file_name
        10              // line_number
    };

    EXPECT_CALL(eval_call_stack_, Read(thread, NotNull()))
        .WillOnce(SetArgPointee<1>(
            std::vector<EvalCallStack::JvmFrame>(1, jvm_frame)));

    EXPECT_CALL(eval_call_stack_, ResolveCallFrameKey(0))
        .WillRepeatedly(ReturnRef(frame_info));

    ExpectJsonEq(
        *BreakpointBuilder()
            .set_id("BP")
            .add_stack_frame(StackFrameBuilder()
                .set_function(test_case.expected_function_name)
                .set_location(test_case.expected_source_path, 10))
            .add_capture_buffer_full_variable_table_item()
            .build(),
        Collect().get());
  }
}


TEST_F(CaptureDataCollectorTest, WatchedExpressions) {
  EvalCallStack::JvmFrame jvm_frame = { { method1, 100 }, 0 };

  const EvalCallStack::FrameInfo frame_info = {
      "LClass1;",     // class_signature
      std::string(),  // class_generic
      "Method1",      // method_name
      "Class1.java",  // source_file_name
      10              // line_number
  };

  std::vector<NamedJVariant> locals;

  EXPECT_CALL(eval_call_stack_, Read(thread, NotNull()))
      .WillOnce(SetArgPointee<1>(
          std::vector<EvalCallStack::JvmFrame>(1, jvm_frame)));

  EXPECT_CALL(eval_call_stack_, ResolveCallFrameKey(0))
      .WillRepeatedly(ReturnRef(frame_info));

  ExpectEvaluateLocalVariables(0, &locals);

  // TODO: replace nullptr with "MockReadersFactory" instance.
  // For now all the expressions are static and don't require it, so don't
  // include it to speed up the build.
  watches_.push_back(CompileExpression("2 + 3", nullptr));
  watches_.push_back(CompileExpression("true", nullptr));
  watches_.push_back(CompileExpression("null", nullptr));

  ExpectJsonEq(
      *BreakpointBuilder()
          .set_id("BP")
          .add_stack_frame(StackFrameBuilder()
              .set_function("Class1.Method1")
              .set_location("Class1.java", 10))
          .add_evaluated_expression(VariableBuilder()
              .set_value("5")
              .set_type("int"))
          .add_evaluated_expression(VariableBuilder()
              .set_value("true")
              .set_type("boolean"))
          .add_evaluated_expression(VariableBuilder()
              .set_value("null"))
          .add_capture_buffer_full_variable_table_item()
          .build(),
      Collect().get());
}


TEST_F(CaptureDataCollectorTest, DynamicString) {
  EvalCallStack::JvmFrame jvm_frame = { { method1, 100 }, 0 };

  const EvalCallStack::FrameInfo frame_info = {
      "LClass1;",     // class_signature
      std::string(),  // class_generic
      "Method1",      // method_name
      "Class1.java",  // source_file_name
      10              // line_number
  };

  std::vector<NamedJVariant> locals;
  named_jvariant_util_.AddRefVariable("a", object1, &locals);

  std::vector<NamedJVariant> a_members;
  named_jvariant_util_.AddStringVariable("", "rhinoceros", &a_members);

  EXPECT_CALL(eval_call_stack_, Read(thread, NotNull()))
      .WillOnce(SetArgPointee<1>(
          std::vector<EvalCallStack::JvmFrame>(1, jvm_frame)));

  EXPECT_CALL(eval_call_stack_, ResolveCallFrameKey(0))
      .WillRepeatedly(ReturnRef(frame_info));

  ExpectEvaluateLocalVariables(0, &locals);
  ExpectEvaluateObjectMembers(object1, a_members);

  ExpectJsonEq(
      *BreakpointBuilder()
          .set_id("BP")
          .add_stack_frame(StackFrameBuilder()
              .set_function("Class1.Method1")
              .set_location("Class1.java", 10)
              .add_local(VariableBuilder()
                  .set_name("a")
                  .set_var_table_index(1)))
          .add_capture_buffer_full_variable_table_item()
          .add_variable_table_item(VariableBuilder()
              .set_value("\"rhinoceros\"")
              .set_type("String"))
          .build(),
      Collect().get());
}


TEST_F(CaptureDataCollectorTest, BreakpointLabels) {
  auto* labels_provider = new MockBreakpointLabelsProvider;
  EXPECT_CALL(*labels_provider, Format())
      .WillOnce(Return(std::map<std::string, std::string>{
          std::make_pair("key1", "value1"), std::make_pair("key2", "value2")}));

  EXPECT_CALL(eval_call_stack_, Read(thread, NotNull()))
      .WillOnce(Return());

  ExpectJsonEq(
      *BreakpointBuilder()
          .set_id("BP")
          .add_capture_buffer_full_variable_table_item()
          .add_label("key1", "value1")
          .add_label("key2", "value2")
          .build(),
      CollectWithLabels(labels_provider).get());
}

// This test ensures that any pre existing labels present in the Breakpoint
// before the Agent labels are added get preserved and are not wiped out.
TEST_F(CaptureDataCollectorTest, BreakpointExistingLabelsSurvive) {
  auto* labels_provider = new MockBreakpointLabelsProvider;
  EXPECT_CALL(*labels_provider, Format())
      .WillOnce(Return(std::map<std::string, std::string>{
          std::make_pair("key1", "value1"), std::make_pair("key2", "value2")}));

  EXPECT_CALL(eval_call_stack_, Read(thread, NotNull()))
      .WillOnce(Return());

  ExpectJsonEq(*BreakpointBuilder()
                    .set_id("BP")
                    .add_capture_buffer_full_variable_table_item()
                    .add_label("key1", "value1")
                    .add_label("key2", "value2")
                    .add_label("key3", "value3")
                    .add_label("key4", "value4")
                    .build(),
               CollectWithLabels(labels_provider,
                                 {{"key3", "value3"}, {"key4", "value4"}})
                   .get());
}


// This test ensures that any pre existing labels present in the Breakpoint that
// conflict with an agent label has priority and is preserved over the agent's
// label value.
TEST_F(CaptureDataCollectorTest, BreakpointExistingLabelsPriority) {
  auto* labels_provider = new MockBreakpointLabelsProvider;
  EXPECT_CALL(*labels_provider, Format())
      .WillOnce(Return(std::map<std::string, std::string>{
          std::make_pair("key1", "value1"), std::make_pair("key2", "value2")}));

  EXPECT_CALL(eval_call_stack_, Read(thread, NotNull()))
      .WillOnce(Return());

  // The prexising label 'key1' has value 'foobar', which should have
  // priority over the value of 'value1' the agent would otherwise be setting.
  ExpectJsonEq(*BreakpointBuilder()
                    .set_id("BP")
                    .add_capture_buffer_full_variable_table_item()
                    .add_label("key1", "foobar")
                    .add_label("key2", "value2")
                    .add_label("key3", "value3")
                    .build(),
               CollectWithLabels(labels_provider,
                                 {{"key1", "foobar"}, {"key3", "value3"}})
                   .get());
}


TEST_F(CaptureDataCollectorTest, BreakpointUserIdEnabled) {
  auto* user_id_provider = new MockUserIdProvider;
  EXPECT_CALL(*user_id_provider, Format(NotNull(), NotNull()))
      .WillOnce(DoAll(
          SetArgPointee<0>("mdb_user"),
          SetArgPointee<1>("noogler"),
          Return(true)));

  EXPECT_CALL(eval_call_stack_, Read(thread, NotNull()))
      .WillOnce(Return());

  ExpectJsonEq(
      *BreakpointBuilder()
          .set_id("BP")
          .add_capture_buffer_full_variable_table_item()
          .set_evaluated_user_id(UserIdBuilder()
              .set_kind("mdb_user")
              .set_id("noogler"))
          .build(),
      CollectWithUserId(user_id_provider).get());
}


TEST_F(CaptureDataCollectorTest, BreakpointUserIdDisabled) {
  absl::FlagSaver fs;

  absl::SetFlag(&FLAGS_cdbg_capture_user_id, false);

  MockUserIdProvider user_id_provider;  // ownership never transferred.
  EXPECT_CALL(user_id_provider, Format(NotNull(), NotNull()))
      .Times(0);

  EXPECT_CALL(eval_call_stack_, Read(thread, NotNull()))
      .WillOnce(Return());

  ExpectJsonEq(
      *BreakpointBuilder()
          .set_id("BP")
          .add_capture_buffer_full_variable_table_item()
          .build(),
      CollectWithUserId(&user_id_provider).get());
}

}  // namespace cdbg
}  // namespace devtools
