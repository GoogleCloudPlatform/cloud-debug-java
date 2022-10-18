/**
 * Copyright 2018 Google Inc. All Rights Reserved.
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

#include "data_visibility_scrubbed_eval_call_stack.h"

#include "gmock/gmock.h"

#include "fake_jni.h"
#include "mock_data_visibility_policy.h"
#include "mock_eval_call_stack.h"
#include "mock_jvmti_env.h"

namespace devtools {
namespace cdbg {

using ::testing::_;
using ::testing::ByMove;
using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Return;


class DataVisibilityScrubbedEvalCallStackTest : public ::testing::Test {
 protected:
  DataVisibilityScrubbedEvalCallStackTest()
      : fake_jni_(&jvmti_),
        global_jvm_(&jvmti_, fake_jni_.jni()) {
  }

  void SetUp() override {
    EXPECT_CALL(jvmti_, Deallocate(NotNull()))
        .WillRepeatedly(Invoke([] (unsigned char* buffer) {
          delete[] buffer;
          return JVMTI_ERROR_NONE;
        }));
  }

  void TearDown() override {
    for (jclass cls : fake_classes_) {
      jni()->DeleteLocalRef(cls);
    }
  }

  void AddStackFrame(const std::string& method_name, bool is_sensitive,
                     std::vector<EvalCallStack::JvmFrame>* result,
                     bool is_already_scrubbed = false) {
    // Register a fake method and class with fake_jni_.  This is needed to
    // allow jni calls within the test to function
    FakeJni::MethodMetadata method_metadata;
    if (is_already_scrubbed) {
      method_metadata.id = nullptr;
    } else {
      method_metadata.id = reinterpret_cast<jmethodID>(next_method_id_);
    }
    method_metadata.metadata.name = method_name;
    method_metadata.metadata.signature = "(V)V";

    FakeJni::ClassMetadata cls_metadata;
    cls_metadata.file_name = method_name + ".cc";
    const std::string sig = "L/com/My" + method_name + "Class;";
    cls_metadata.signature = sig;
    cls_metadata.generic = sig;
    cls_metadata.methods.push_back(method_metadata);

    jclass test_class = fake_jni_.CreateNewClass(cls_metadata);
    fake_classes_.push_back(test_class);

    // Now create a JvmFrame.  The class/method created above are
    // used as the reference id.
    EvalCallStack::JvmFrame frame;
    frame.code_location.method = method_metadata.id;
    result->push_back(frame);


    // Increment method id for next call
    next_method_id_ += 200;

    if (is_already_scrubbed) {
      // No need to setup any expectations on the method.
      return;
    }

    // Setup mock expectations
    EXPECT_CALL(jvmti_, GetMethodDeclaringClass(method_metadata.id, NotNull()))
        .WillOnce(Invoke([this, test_class] (jmethodID method_id, jclass* cls) {
          *cls = test_class;
          return JVMTI_ERROR_NONE;
        }));

    std::unique_ptr<DataVisibilityPolicy::Class> policy_class(nullptr);

    if (is_sensitive) {
      MockDataVisibilityPolicy::Class* mock_policy_class =
          new MockDataVisibilityPolicy::Class;

      EXPECT_CALL(
          *mock_policy_class,
          IsMethodVisible(method_name, method_metadata.metadata.signature, 0))
          .WillOnce(Return(false));

      policy_class.reset(mock_policy_class);
    }

    EXPECT_CALL(mock_policy_, GetClassVisibility(test_class))
        .WillOnce(Return(ByMove(std::move(policy_class))));
  }

 protected:
  MockJvmtiEnv jvmti_;
  FakeJni fake_jni_;
  GlobalJvmEnv global_jvm_;
  MockDataVisibilityPolicy mock_policy_;
  std::vector<jclass> fake_classes_;
  int next_method_id_ { 4234234 };
};


TEST_F(DataVisibilityScrubbedEvalCallStackTest, ReadEmpty) {
  MockEvalCallStack* mock_call_stack = new MockEvalCallStack();
  DataVisibilityScrubbedEvalCallStack scrubber(
      std::unique_ptr<EvalCallStack>(mock_call_stack),
      &mock_policy_);

  jthread thread = nullptr;
  std::vector<EvalCallStack::JvmFrame> result;
  EXPECT_CALL(*mock_call_stack, Read(thread, &result));
  scrubber.Read(thread, &result);
  EXPECT_EQ(0, result.size());
}


TEST_F(DataVisibilityScrubbedEvalCallStackTest, ReadNoSensitive) {
  MockEvalCallStack* mock_call_stack = new MockEvalCallStack();
  MockDataVisibilityPolicy mock_policy;
  DataVisibilityScrubbedEvalCallStack scrubber(
      std::unique_ptr<EvalCallStack>(mock_call_stack),
      &mock_policy_);

  EXPECT_CALL(*mock_call_stack, Read(_, _));

  jthread thread = nullptr;
  std::vector<EvalCallStack::JvmFrame> result;

  // Each AddStackFrame adds the stack frame to result but also
  // registers a new class with fake_jni_ and sets approriate expectations
  // so that the method "exists" in the jni.
  AddStackFrame("unsensitive", false, &result);
  AddStackFrame("unsensitive2", false, &result);

  scrubber.Read(thread, &result);

  // Assert that nothing was scrubbed
  ASSERT_EQ(2, result.size());
  EXPECT_NE(nullptr, result.at(0).code_location.method);
  EXPECT_NE(nullptr, result.at(1).code_location.method);
}


TEST_F(DataVisibilityScrubbedEvalCallStackTest, SensitiveTopFrame) {
  MockEvalCallStack* mock_call_stack = new MockEvalCallStack();
  MockDataVisibilityPolicy mock_policy;
  DataVisibilityScrubbedEvalCallStack scrubber(
      std::unique_ptr<EvalCallStack>(mock_call_stack),
      &mock_policy_);

  EXPECT_CALL(*mock_call_stack, Read(_, _));

  jthread thread = nullptr;
  std::vector<EvalCallStack::JvmFrame> result;

  AddStackFrame("sensitive", true, &result);
  AddStackFrame("unsensitive", false, &result);

  scrubber.Read(thread, &result);

  // Assert that nothing was scrubbed
  ASSERT_EQ(2, result.size());
  EXPECT_NE(nullptr, result.at(0).code_location.method);
  EXPECT_NE(nullptr, result.at(1).code_location.method);
}


TEST_F(DataVisibilityScrubbedEvalCallStackTest, ReadSensitiveIsParent) {
  MockEvalCallStack* mock_call_stack = new MockEvalCallStack();
  MockDataVisibilityPolicy mock_policy;
  DataVisibilityScrubbedEvalCallStack scrubber(
      std::unique_ptr<EvalCallStack>(mock_call_stack),
      &mock_policy_);

  EXPECT_CALL(*mock_call_stack, Read(_, _));

  jthread thread = nullptr;
  std::vector<EvalCallStack::JvmFrame> result;

  AddStackFrame("unsensitive", false, &result);
  AddStackFrame("sensitive", true, &result);

  scrubber.Read(thread, &result);

  // Assert that the child was scrubbed
  ASSERT_EQ(2, result.size());
  EXPECT_EQ(nullptr, result.at(0).code_location.method);
  EXPECT_NE(nullptr, result.at(1).code_location.method);
}


TEST_F(DataVisibilityScrubbedEvalCallStackTest, ReadBackToBackSensitive) {
  MockEvalCallStack* mock_call_stack = new MockEvalCallStack();
  MockDataVisibilityPolicy mock_policy;
  DataVisibilityScrubbedEvalCallStack scrubber(
      std::unique_ptr<EvalCallStack>(mock_call_stack),
      &mock_policy_);

  // Seven stack levels (from child to parent)
  //   - child1       (scrub)
  //   - child2       (scrub)
  //   - sensitive    (keep)
  //   - sensitive2   (keep)
  //   - sensitive3   (keep)
  //   - unsensitive  (keep)
  //   - unsensitive2 (keep)
  //
  //   "child1" and "child2" should be scrubbed
  EXPECT_CALL(*mock_call_stack, Read(_, _));

  jthread thread = nullptr;
  std::vector<EvalCallStack::JvmFrame> result;

  AddStackFrame("child1", false, &result);
  AddStackFrame("child2", false, &result);
  AddStackFrame("sensitive", true, &result);
  AddStackFrame("sensitive2", true, &result);
  AddStackFrame("sensitive3", true, &result);
  AddStackFrame("unsensitive", false, &result);
  AddStackFrame("unsensitive2", false, &result);

  scrubber.Read(thread, &result);

  // Assert that child1 and child2 have their method fields set to nullptr
  ASSERT_EQ(7, result.size());
  EXPECT_EQ(nullptr, result.at(0).code_location.method);
  EXPECT_EQ(nullptr, result.at(1).code_location.method);
  EXPECT_NE(nullptr, result.at(2).code_location.method);
  EXPECT_NE(nullptr, result.at(3).code_location.method);
  EXPECT_NE(nullptr, result.at(4).code_location.method);
  EXPECT_NE(nullptr, result.at(5).code_location.method);
  EXPECT_NE(nullptr, result.at(6).code_location.method);
}


TEST_F(DataVisibilityScrubbedEvalCallStackTest, ReadWithSensitiveGap) {
  MockEvalCallStack* mock_call_stack = new MockEvalCallStack();
  MockDataVisibilityPolicy mock_policy;
  DataVisibilityScrubbedEvalCallStack scrubber(
      std::unique_ptr<EvalCallStack>(mock_call_stack),
      &mock_policy_);

  // Six stack levels (from child to parent)
  //   - child1       (scrub)
  //   - child2       (scrub)
  //   - sensitive    (keep)
  //   - sensitive2   (keep)
  //   - unsensitive  (scrub)
  //   - sensitive3   (keep)
  //   - unsensitive2 (keep)
  //
  // Every child of "sensitive2" should be scrubbed.
  EXPECT_CALL(*mock_call_stack, Read(_, _));

  jthread thread = nullptr;
  std::vector<EvalCallStack::JvmFrame> result;

  AddStackFrame("child1", false, &result);
  AddStackFrame("child2", false, &result);
  AddStackFrame("sensitive", true, &result);
  AddStackFrame("sensitive2", true, &result);
  AddStackFrame("unsensitive", false, &result);
  AddStackFrame("sensitive3", true, &result);
  AddStackFrame("unsensitive2", false, &result);

  scrubber.Read(thread, &result);

  // Assert that all fields under sensitive2 are blocked.
  ASSERT_EQ(7, result.size());
  EXPECT_EQ(nullptr, result.at(0).code_location.method);
  EXPECT_EQ(nullptr, result.at(1).code_location.method);
  EXPECT_NE(nullptr, result.at(2).code_location.method);
  EXPECT_NE(nullptr, result.at(3).code_location.method);
  EXPECT_EQ(nullptr, result.at(4).code_location.method);
  EXPECT_NE(nullptr, result.at(5).code_location.method);
  EXPECT_NE(nullptr, result.at(6).code_location.method);
}


TEST_F(DataVisibilityScrubbedEvalCallStackTest, ReadAlreadyScrubbed) {
  MockEvalCallStack* mock_call_stack = new MockEvalCallStack();
  MockDataVisibilityPolicy mock_policy;
  DataVisibilityScrubbedEvalCallStack scrubber(
      std::unique_ptr<EvalCallStack>(mock_call_stack),
      &mock_policy_);

  EXPECT_CALL(*mock_call_stack, Read(_, _));

  jthread thread = nullptr;
  std::vector<EvalCallStack::JvmFrame> result;

  // Assume the middle frame was already scrubbed by the nested scrubber.
  bool kAlreadyScrubbed = true;
  AddStackFrame("unsensitive", false, &result);
  AddStackFrame("nested-scrubbed", true, &result, kAlreadyScrubbed);
  AddStackFrame("unsensitive", false, &result);

  scrubber.Read(thread, &result);

  // Assert that the already-scrubbed frame was ignored and frames 0 and 2 have
  // a valid method.
  ASSERT_EQ(3, result.size());
  EXPECT_NE(nullptr, result.at(0).code_location.method);
  EXPECT_NE(nullptr, result.at(2).code_location.method);
}


}  // namespace cdbg
}  // namespace devtools
