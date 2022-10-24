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

#include "src/agent/jvm_breakpoints_manager.h"

#include "gtest/gtest.h"
#include "src/agent/callbacks_monitor.h"
#include "src/agent/format_queue.h"
#include "src/agent/jvm_evaluators.h"
#include "src/agent/messages.h"
#include "src/agent/method_locals.h"
#include "src/agent/model_util.h"
#include "src/agent/object_evaluator.h"
#include "src/agent/resolved_source_location.h"
#include "src/agent/statistician.h"
#include "tests/agent/fake_jni.h"
#include "tests/agent/mock_breakpoint.h"
#include "tests/agent/mock_bridge.h"
#include "tests/agent/mock_class_indexer.h"
#include "tests/agent/mock_class_metadata_reader.h"
#include "tests/agent/mock_class_path_lookup.h"
#include "tests/agent/mock_dynamic_logger.h"
#include "tests/agent/mock_eval_call_stack.h"
#include "tests/agent/mock_jvmti_env.h"
#include "tests/agent/mock_object_evaluator.h"

using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::IsNull;
using testing::Matcher;
using testing::MatcherInterface;
using testing::MatchResultListener;
using testing::NiceMock;
using testing::NotNull;
using testing::Test;
using testing::Return;
using testing::ReturnRef;
using testing::StrictMock;
using testing::WithArgs;
using testing::Invoke;
using testing::InvokeArgument;
using testing::SetArgPointee;

namespace devtools {
namespace cdbg {

static ClassMetadataReader::Method make_method(const std::string& name) {
  ClassMetadataReader::Method metadata;
  metadata.name = name;

  return metadata;
}

static const jthread kThread = reinterpret_cast<jthread>(0x67125374);

static const FakeJni::ClassMetadata kClass1Metadata = {
    "Class1.java",                        // file_name
    "Lcom/prod/Class1;",                  // signature
    std::string(),                        // generic
    {{                                    // Method
      reinterpret_cast<jmethodID>(1001),  // id
      make_method("firstMethod"),         // metadata.name
      {                                   // line_number_table
       {10031, 31},
       {10035, 35}}}}};

static const FakeJni::ClassMetadata kClass2Metadata = {
    "Class2.java",                        // file_name
    "Lcom/prod/Class2;",                  // signature
    std::string(),                        // generic
    {{                                    // Method
      reinterpret_cast<jmethodID>(2001),  // id
      make_method("secondMethod"),        // metadata.name
      {                                   // line_number_table
       {20100, 100}}}}};

static const EvalCallStack::FrameInfo frame_info_keys[] = {
    {
        "Frame1_ClassSignature",  // class_signature
        std::string(),            // class_generic
        "Frame1_Method",          // method_name
        "Frame1_SourceFileName",  // source_file_name
        1                         // line_number
    },
    {
        "Frame2_ClassSignature",  // class_signature
        std::string(),            // class_generic
        "Frame2_Method",          // method_name
        "Frame2_SourceFileName",  // source_file_name
        2                         // line_number
    }};

class JvmBreakpointsManagerTest : public ::testing::Test {
 protected:
  JvmBreakpointsManagerTest()
      : fake_jni_(&jvmti_),
        global_jvm_(fake_jni_.jvmti(), fake_jni_.jni()),
        method_locals_(nullptr) {
    evaluators_.class_path_lookup = &class_path_lookup_;
    evaluators_.class_indexer = &class_indexer_;
    evaluators_.eval_call_stack = &eval_call_stack_;
    evaluators_.method_locals = &method_locals_;
    evaluators_.class_metadata_reader = &class_metadata_reader_;
    evaluators_.object_evaluator = &object_evaluator_;
  }

  MOCK_METHOD(std::shared_ptr<Breakpoint>, BreakpointFactory,
              (const std::string&));

  void SetUp() override {
    InitializeStatisticians();
    CallbacksMonitor::InitializeSingleton(1000);

    EXPECT_CALL(class_indexer_, FindClassBySignature(_))
        .WillRepeatedly(Invoke([this](const std::string& class_signature) {
          return JniLocalRef(fake_jni_.FindClassBySignature(class_signature));
        }));

    // By default assume the location can't be resolved.
    ResolvedSourceLocation bad_source_code;
    bad_source_code.error_message = { "Bad source code" };

    EXPECT_CALL(
        class_path_lookup_,
        ResolveSourceLocation(_, _, NotNull()))
        .WillRepeatedly(SetArgPointee<2>(bad_source_code));

    // Set up call stack.
    EXPECT_CALL(eval_call_stack_, Read(kThread, NotNull()))
        .WillRepeatedly(SetArgPointee<1>(std::vector<EvalCallStack::JvmFrame>({
            { { reinterpret_cast<jmethodID>(9001), 100 }, 0 },
            { { reinterpret_cast<jmethodID>(9002), 200 }, 1 }
        })));

    for (int i = 0; i < arraysize(frame_info_keys); ++i) {
      EXPECT_CALL(eval_call_stack_, ResolveCallFrameKey(i))
          .WillRepeatedly(ReturnRef(frame_info_keys[i]));
    }

    // Simulate no local variables.
    EXPECT_CALL(jvmti_, GetMethodDeclaringClass(_, NotNull()))
        .WillRepeatedly(DoAll(
            SetArgPointee<1>(nullptr),
            Return(JVMTI_ERROR_NONE)));

    EXPECT_CALL(jvmti_, GetLocalVariableTable(_, NotNull(), NotNull()))
        .WillRepeatedly(Invoke([] (
            jmethodID method,
            jint* entry_count,
            jvmtiLocalVariableEntry** table) {
          *entry_count = 0;
          *table = reinterpret_cast<jvmtiLocalVariableEntry*>(new char[0]);

          return JVMTI_ERROR_NONE;
        }));

    // Simulate static methods so that we don't need to mock extraction of
    // local instance.
    EXPECT_CALL(jvmti_, GetMethodModifiers(_, NotNull()))
        .WillRepeatedly(DoAll(
            SetArgPointee<1>(JVM_ACC_STATIC),
            Return(JVMTI_ERROR_NONE)));
  }

  void TearDown() override {
    EXPECT_CALL(jvmti_, ClearBreakpoint(_, _))
        .WillRepeatedly(Return(JVMTI_ERROR_NONE));

    breakpoints_manager_->Cleanup();

    format_queue_.RemoveAll();

    CallbacksMonitor::CleanupSingleton();
    CleanupStatisticians();
  }

  void InitializeBreakpointsManager(CanaryControl* canary_control = nullptr) {
    auto factory = [this] (
        BreakpointsManager* breakpoints_manager,
        std::unique_ptr<BreakpointModel> breakpoint_definition) {
      EXPECT_EQ(breakpoints_manager_.get(), breakpoints_manager);
      return BreakpointFactory(breakpoint_definition->id);
    };

    breakpoints_manager_.reset(new JvmBreakpointsManager(
        factory,
        &evaluators_,
        &format_queue_,
        canary_control));
  }

  void SetActiveBreakpointsList(
      const std::vector<BreakpointModel*>& breakpoint_ptrs) {
    // Clone the breakpoints list.
    std::vector<std::unique_ptr<BreakpointModel>> breakpoints;
    for (BreakpointModel* breakpoint : breakpoint_ptrs) {
      breakpoints.push_back(BreakpointBuilder(*breakpoint).build());
    }

    breakpoints_manager_->SetActiveBreakpointsList(std::move(breakpoints));
  }

  void ExpectResolveSourceLocation(const std::string& source_path,
                                   int line_number,
                                   const std::string& resolved_class_signature,
                                   const std::string& resolved_method_name,
                                   int adjusted_line_number) {
    ResolvedSourceLocation location;
    location.class_signature = resolved_class_signature;
    location.method_name = resolved_method_name;
    location.adjusted_line_number = adjusted_line_number;

    EXPECT_CALL(
        class_path_lookup_,
        ResolveSourceLocation(source_path, line_number, NotNull()))
        .WillRepeatedly(SetArgPointee<2>(location));
  }

  void ExpectSetBreakpoint(jmethodID method, jlocation location) {
    EXPECT_CALL(jvmti_, SetBreakpoint(method, location))
        .WillOnce(Return(JVMTI_ERROR_NONE))
        .RetiresOnSaturation();
  }

  void ExpectClearBreakpoint(jmethodID method, jlocation location) {
    EXPECT_CALL(jvmti_, ClearBreakpoint(method, location))
        .WillOnce(Return(JVMTI_ERROR_NONE))
        .RetiresOnSaturation();
  }


 protected:
  NiceMock<MockJvmtiEnv> jvmti_;
  FakeJni fake_jni_;
  GlobalJvmEnv global_jvm_;
  MockClassPathLookup class_path_lookup_;
  MockClassIndexer class_indexer_;
  MockEvalCallStack eval_call_stack_;
  MethodLocals method_locals_;
  MockClassMetadataReader class_metadata_reader_;
  NiceMock<MockObjectEvaluator> object_evaluator_;
  JvmEvaluators evaluators_;
  FormatQueue format_queue_;
  MockDynamicLogger dynamic_logger_;
  StrictMock<MockBridge> bridge_;
  std::unique_ptr<BreakpointsManager> breakpoints_manager_;
};


TEST_F(JvmBreakpointsManagerTest, Empty) {
  InitializeBreakpointsManager();
  SetActiveBreakpointsList({});
}


TEST_F(JvmBreakpointsManagerTest, AddSingle) {
  auto bp = BreakpointBuilder()
      .set_id("ID_A")
      .set_location("Class1.java", 31)
      .build();

  auto breakpoint = std::make_shared<NiceMock<MockBreakpoint>>("ID_A");
  EXPECT_CALL(*this, BreakpointFactory("ID_A"))
      .WillOnce(Return(breakpoint));

  CanaryControl canary_control(CallbacksMonitor::GetInstance(), &bridge_);
  InitializeBreakpointsManager(&canary_control);
  SetActiveBreakpointsList({ bp.get() });
}


TEST_F(JvmBreakpointsManagerTest, BreakpointInitializationCallbacks) {
  // Simulate JVMTI callbacks from within Breakpoint::Initialize to verify
  // no deadlocks.
  auto bp = BreakpointBuilder()
      .set_id("ID_A")
      .set_location("Class1.java", 32)
      .build();

  EXPECT_CALL(*this, BreakpointFactory("ID_A"))
      .WillOnce(Invoke([this](const std::string& id) {
        auto breakpoint = std::make_shared<NiceMock<MockBreakpoint>>(id);
        EXPECT_CALL(*breakpoint, Initialize())
            .WillOnce(Invoke([this] () {
              class_indexer_.FireOnClassPrepared(
                  "com.prod.SomeOtherClass.InnerClass",
                  "Lcom/prod/SomeOtherClass$InnerClass;");
            }));

        return breakpoint;
      }));

  InitializeBreakpointsManager();
  SetActiveBreakpointsList({ bp.get() });
}


TEST_F(JvmBreakpointsManagerTest, AddTwoBreakpointsSameLocation) {
  auto definition1 = BreakpointBuilder()
      .set_id("ID_A")
      .set_location("Class1.java", 31)
      .build();

  auto definition2 = BreakpointBuilder()
      .set_id("ID_B")
      .set_location("Class1.java", 31)
      .build();

  std::shared_ptr<MockBreakpoint> breakpoints[] = {
    std::make_shared<NiceMock<MockBreakpoint>>("ID_A"),
    std::make_shared<NiceMock<MockBreakpoint>>("ID_B")
  };

  EXPECT_CALL(*this, BreakpointFactory("ID_A"))
      .WillOnce(Return(breakpoints[0]));

  EXPECT_CALL(*this, BreakpointFactory("ID_B"))
      .WillOnce(Return(breakpoints[1]));

  for (auto& breakpoint : breakpoints) {
    EXPECT_CALL(*breakpoint, Initialize())
        .WillOnce(Invoke([this, &breakpoint] () {
          breakpoints_manager_->SetJvmtiBreakpoint(
              reinterpret_cast<jmethodID>(1001),
              10031,
              breakpoint);
        }))
        .RetiresOnSaturation();

    EXPECT_CALL(*breakpoint, ResetToPending())
        .WillOnce(Invoke([this, &breakpoint] () {
          breakpoints_manager_->ClearJvmtiBreakpoint(
              reinterpret_cast<jmethodID>(1001),
              10031,
              breakpoint);
        }))
        .RetiresOnSaturation();
  }

  InitializeBreakpointsManager();

  ExpectSetBreakpoint(reinterpret_cast<jmethodID>(1001), 10031);
  SetActiveBreakpointsList({ definition1.get(), definition2.get() });

  ExpectClearBreakpoint(reinterpret_cast<jmethodID>(1001), 10031);
  SetActiveBreakpointsList({});
}


TEST_F(JvmBreakpointsManagerTest, RefreshNoChange) {
  auto definition1 = BreakpointBuilder()
      .set_id("ID_A")
      .set_location("Class1.java", 31)
      .build();

  auto definition2 = BreakpointBuilder()
      .set_id("ID_B")
      .set_location("Class1.java", 35)
      .build();

  std::shared_ptr<MockBreakpoint> breakpoints[] = {
    std::make_shared<NiceMock<MockBreakpoint>>("ID_A"),
    std::make_shared<NiceMock<MockBreakpoint>>("ID_B")
  };

  EXPECT_CALL(*this, BreakpointFactory("ID_A"))
      .WillOnce(Return(breakpoints[0]));

  EXPECT_CALL(*this, BreakpointFactory("ID_B"))
      .WillOnce(Return(breakpoints[1]));

  InitializeBreakpointsManager();
  SetActiveBreakpointsList({ definition1.get(), definition2.get() });
  SetActiveBreakpointsList({ definition1.get(), definition2.get() });
  SetActiveBreakpointsList({ definition2.get(), definition1.get() });
}


TEST_F(JvmBreakpointsManagerTest, IncrementalAdd) {
  auto definition1 = BreakpointBuilder()
      .set_id("ID_A")
      .set_location("Class1.java", 31)
      .build();

  auto definition2 = BreakpointBuilder()
      .set_id("ID_B")
      .set_location("Class1.java", 31)
      .build();

  auto definition3 = BreakpointBuilder()
      .set_id("ID_C")
      .set_location("Class1.java", 35)
      .build();

  auto definition4 = BreakpointBuilder()
      .set_id("ID_D")
      .set_location("Class2.java", 100)
      .build();

  std::shared_ptr<MockBreakpoint> breakpoints[] = {
    std::make_shared<NiceMock<MockBreakpoint>>("ID_A"),
    std::make_shared<NiceMock<MockBreakpoint>>("ID_B"),
    std::make_shared<NiceMock<MockBreakpoint>>("ID_C"),
    std::make_shared<NiceMock<MockBreakpoint>>("ID_D")
  };

  EXPECT_CALL(*this, BreakpointFactory("ID_A"))
      .WillOnce(Return(breakpoints[0]));

  InitializeBreakpointsManager();

  SetActiveBreakpointsList(
      {
        definition1.get()
      });

  EXPECT_CALL(*this, BreakpointFactory("ID_B"))
      .WillOnce(Return(breakpoints[1]));
  SetActiveBreakpointsList(
      {
        definition2.get(),
        definition1.get()
      });

  EXPECT_CALL(*this, BreakpointFactory("ID_C"))
      .WillOnce(Return(breakpoints[2]));
  SetActiveBreakpointsList(
      {
        definition2.get(),
        definition3.get(),
        definition1.get()
      });

  EXPECT_CALL(*this, BreakpointFactory("ID_D"))
      .WillOnce(Return(breakpoints[3]));
  SetActiveBreakpointsList(
      {
        definition2.get(),
        definition3.get(),
        definition4.get(),
        definition1.get()
      });
}


TEST_F(JvmBreakpointsManagerTest, OnClassPreparedBroadcast) {
  auto definition1 = BreakpointBuilder()
      .set_id("ID_A")
      .set_location("Class1.java", 31)
      .build();

  auto definition2 = BreakpointBuilder()
      .set_id("ID_B")
      .set_location("Class1.java", 35)
      .build();

  std::shared_ptr<MockBreakpoint> breakpoints[] = {
    std::make_shared<NiceMock<MockBreakpoint>>("ID_A"),
    std::make_shared<NiceMock<MockBreakpoint>>("ID_B")
  };

  EXPECT_CALL(*this, BreakpointFactory("ID_A"))
      .WillOnce(Return(breakpoints[0]));

  EXPECT_CALL(*this, BreakpointFactory("ID_B"))
      .WillOnce(Return(breakpoints[1]));

  InitializeBreakpointsManager();

  SetActiveBreakpointsList({ definition1.get(), definition2.get() });

  for (const auto& breakpoint : breakpoints) {
    EXPECT_CALL(*breakpoint,
                OnClassPrepared("Lcom/prod/PendingClass;", "whateverMethod"))
        .Times(1);
  }

  class_indexer_.FireOnClassPrepared(
      "Lcom/prod/PendingClass;",
      "whateverMethod");
}


TEST_F(JvmBreakpointsManagerTest, RemovePendingBreakpoint) {
  auto definition = BreakpointBuilder()
      .set_id("ID_A")
      .set_location("Class1.java", 31)
      .build();

  std::shared_ptr<MockBreakpoint> breakpoint =
      std::make_shared<NiceMock<MockBreakpoint>>("ID_A");

  EXPECT_CALL(*this, BreakpointFactory("ID_A"))
      .WillOnce(Return(breakpoint));

  InitializeBreakpointsManager();

  SetActiveBreakpointsList({ definition.get() });

  EXPECT_CALL(*breakpoint, ResetToPending())
      .Times(1);

  SetActiveBreakpointsList({});

  EXPECT_EQ(nullptr, format_queue_.FormatAndPop());
}


TEST_F(JvmBreakpointsManagerTest, UnrecognizedBreakpointHit) {
  InitializeBreakpointsManager();

  breakpoints_manager_->JvmtiOnBreakpoint(
      kThread,
      reinterpret_cast<jmethodID>(1002),
      10031);

  breakpoints_manager_->JvmtiOnBreakpoint(
      kThread,
      reinterpret_cast<jmethodID>(1001),
      10032);

  EXPECT_EQ(nullptr, format_queue_.FormatAndPop());
}


TEST_F(JvmBreakpointsManagerTest, BreakpointHit) {
  const jmethodID method = reinterpret_cast<jmethodID>(1001);
  const jlocation location = 10031;

  auto definition1 = BreakpointBuilder()
      .set_id("ID_A")
      .set_location("Class1.java", 31)
      .build();

  auto definition2 = BreakpointBuilder()
      .set_id("ID_B")
      .set_location("Class1.java", 31)
      .build();

  std::shared_ptr<MockBreakpoint> breakpoints[] = {
    std::make_shared<NiceMock<MockBreakpoint>>("ID_A"),
    std::make_shared<NiceMock<MockBreakpoint>>("ID_B")
  };

  EXPECT_CALL(*this, BreakpointFactory("ID_A"))
      .WillOnce(Return(breakpoints[0]));

  EXPECT_CALL(*this, BreakpointFactory("ID_B"))
      .WillOnce(Return(breakpoints[1]));

  InitializeBreakpointsManager();

  SetActiveBreakpointsList({ definition1.get(), definition2.get() });

  ExpectSetBreakpoint(method, location);
  breakpoints_manager_->SetJvmtiBreakpoint(method, location, breakpoints[0]);
  breakpoints_manager_->SetJvmtiBreakpoint(method, location, breakpoints[1]);

  EXPECT_CALL(*breakpoints[0], OnJvmBreakpointHit(kThread, method, location))
      .Times(1);

  EXPECT_CALL(*breakpoints[1], OnJvmBreakpointHit(kThread, method, location))
      .Times(1);

  breakpoints_manager_->JvmtiOnBreakpoint(kThread, method, location);

  ExpectClearBreakpoint(method, location);
  breakpoints_manager_->ClearJvmtiBreakpoint(method, location, breakpoints[0]);
  breakpoints_manager_->ClearJvmtiBreakpoint(method, location, breakpoints[1]);
}


TEST_F(JvmBreakpointsManagerTest, CompletedBreakpointsListCleanup) {
  auto definition = BreakpointBuilder()
      .set_id("ID_A")
      .set_location("Class1.java", 31)
      .build();

  std::shared_ptr<MockBreakpoint> breakpoint =
      std::make_shared<NiceMock<MockBreakpoint>>("ID_A");

  EXPECT_CALL(*this, BreakpointFactory("ID_A"))
      .WillOnce(Return(breakpoint));

  InitializeBreakpointsManager();

  SetActiveBreakpointsList({ definition.get() });

  breakpoints_manager_->CompleteBreakpoint("ID_A");

  // The completed breakpoint should be in "completed_breakpoints_" list,
  // so setting the same breakpoint should have no effect.
  SetActiveBreakpointsList({ definition.get() });

  // Now send update without our breakpoint to clear "completed_breakpoints_".
  SetActiveBreakpointsList({});

  // At this point "completed_breakpoints_" should be empty, so setting the
  // same breakpoint again will work.
  EXPECT_CALL(*this, BreakpointFactory("ID_A"))
      .WillOnce(Return(breakpoint));

  SetActiveBreakpointsList({ definition.get() });
}


TEST_F(JvmBreakpointsManagerTest, AddCanarySuccess) {
  auto bp = BreakpointBuilder()
      .set_id("ID_A")
      .set_location("Class1.java", 31)
      .set_is_canary(true)
      .build();

  auto breakpoint = std::make_shared<StrictMock<MockBreakpoint>>("ID_A");
  EXPECT_CALL(*this, BreakpointFactory("ID_A"))
      .WillOnce(Return(breakpoint));

  const std::string id = "ID_A";
  EXPECT_CALL(*breakpoint, id())
      .WillRepeatedly(ReturnRef(id));

  EXPECT_CALL(*breakpoint, Initialize())
      .WillOnce(Return());

  EXPECT_CALL(*breakpoint, ResetToPending())
      .WillOnce(Return());

  EXPECT_CALL(bridge_, RegisterBreakpointCanary("ID_A"))
      .WillOnce(Return(true));

  CanaryControl canary_control(CallbacksMonitor::GetInstance(), &bridge_);
  InitializeBreakpointsManager(&canary_control);
  SetActiveBreakpointsList({ bp.get() });
}


TEST_F(JvmBreakpointsManagerTest, AddCanaryFailure) {
  auto bp = BreakpointBuilder()
      .set_id("ID_A")
      .set_location("Class1.java", 31)
      .set_is_canary(true)
      .build();

  auto breakpoint = std::make_shared<StrictMock<MockBreakpoint>>("ID_A");
  EXPECT_CALL(*this, BreakpointFactory("ID_A"))
      .WillOnce(Return(breakpoint));

  const std::string id = "ID_A";
  EXPECT_CALL(*breakpoint, id())
      .WillRepeatedly(ReturnRef(id));

  EXPECT_CALL(bridge_, RegisterBreakpointCanary("ID_A"))
      .WillRepeatedly(Return(false));

  CanaryControl canary_control(CallbacksMonitor::GetInstance(), &bridge_);
  InitializeBreakpointsManager(&canary_control);
  SetActiveBreakpointsList({ bp.get() });
}

}  // namespace cdbg
}  // namespace devtools
