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

#include "src/agent/auto_jvmti_breakpoint.h"

#include "gmock/gmock.h"

#include "src/agent/test_util/fake_jni.h"
#include "src/agent/test_util/mock_breakpoint.h"
#include "src/agent/test_util/mock_breakpoints_manager.h"
#include "src/agent/test_util/mock_jvmti_env.h"

using testing::_;
using testing::Return;
using testing::StrictMock;

namespace devtools {
namespace cdbg {

static const jmethodID kMethod1 = reinterpret_cast<jmethodID>(0x23794);
static const jmethodID kMethod2 = reinterpret_cast<jmethodID>(0x37856);

static constexpr jlocation kLocation1 = 7823;
static constexpr jlocation kLocation2 = 8542;

class AutoJvmtiBreakpointTest : public ::testing::Test {
 public:
  AutoJvmtiBreakpointTest()
      : global_jvm_(fake_jni_.jvmti(), fake_jni_.jni()),
        auto_jvmti_breakpoint_(&breakpoints_manager_) {
  }

 protected:
  FakeJni fake_jni_;
  GlobalJvmEnv global_jvm_;
  StrictMock<MockBreakpointsManager> breakpoints_manager_;
  AutoJvmtiBreakpoint auto_jvmti_breakpoint_;
  std::shared_ptr<Breakpoint> breakpoint_ { new MockBreakpoint("a") };
};


TEST_F(AutoJvmtiBreakpointTest, SetSuccessAndClear) {
  EXPECT_CALL(
      breakpoints_manager_,
      SetJvmtiBreakpoint(kMethod1, kLocation1, breakpoint_))
      .WillOnce(Return(true));

  EXPECT_TRUE(
      auto_jvmti_breakpoint_.Set(kMethod1, kLocation1, breakpoint_));

  EXPECT_CALL(
      breakpoints_manager_,
      ClearJvmtiBreakpoint(kMethod1, kLocation1, breakpoint_))
      .WillOnce(Return());

  auto_jvmti_breakpoint_.Clear(breakpoint_);
}


TEST_F(AutoJvmtiBreakpointTest, ClearNoSet) {
  auto_jvmti_breakpoint_.Clear(breakpoint_);
}


TEST_F(AutoJvmtiBreakpointTest, SetSameLocation) {
  EXPECT_CALL(
      breakpoints_manager_,
      SetJvmtiBreakpoint(kMethod1, kLocation1, breakpoint_))
      .WillOnce(Return(true));

  EXPECT_TRUE(
      auto_jvmti_breakpoint_.Set(kMethod1, kLocation1, breakpoint_));

  EXPECT_TRUE(
      auto_jvmti_breakpoint_.Set(kMethod1, kLocation1, breakpoint_));

  EXPECT_CALL(
      breakpoints_manager_,
      ClearJvmtiBreakpoint(kMethod1, kLocation1, breakpoint_))
      .WillOnce(Return());

  auto_jvmti_breakpoint_.Clear(breakpoint_);
}


TEST_F(AutoJvmtiBreakpointTest, SetDifferentMethod) {
  EXPECT_CALL(
      breakpoints_manager_,
      SetJvmtiBreakpoint(kMethod1, kLocation1, breakpoint_))
      .WillOnce(Return(true));

  EXPECT_TRUE(
      auto_jvmti_breakpoint_.Set(kMethod1, kLocation1, breakpoint_));

  EXPECT_CALL(
      breakpoints_manager_,
      ClearJvmtiBreakpoint(kMethod1, kLocation1, breakpoint_))
      .WillOnce(Return());

  EXPECT_CALL(
      breakpoints_manager_,
      SetJvmtiBreakpoint(kMethod2, kLocation1, breakpoint_))
      .WillOnce(Return(true));

  EXPECT_TRUE(
      auto_jvmti_breakpoint_.Set(kMethod2, kLocation1, breakpoint_));

  EXPECT_CALL(
      breakpoints_manager_,
      ClearJvmtiBreakpoint(kMethod2, kLocation1, breakpoint_))
      .WillOnce(Return());

  auto_jvmti_breakpoint_.Clear(breakpoint_);
}


TEST_F(AutoJvmtiBreakpointTest, SetDifferentLocation) {
  EXPECT_CALL(
      breakpoints_manager_,
      SetJvmtiBreakpoint(kMethod1, kLocation1, breakpoint_))
      .WillOnce(Return(true));

  EXPECT_TRUE(
      auto_jvmti_breakpoint_.Set(kMethod1, kLocation1, breakpoint_));

  EXPECT_CALL(
      breakpoints_manager_,
      ClearJvmtiBreakpoint(kMethod1, kLocation1, breakpoint_))
      .WillOnce(Return());

  EXPECT_CALL(
      breakpoints_manager_,
      SetJvmtiBreakpoint(kMethod1, kLocation2, breakpoint_))
      .WillOnce(Return(true));

  EXPECT_TRUE(
      auto_jvmti_breakpoint_.Set(kMethod1, kLocation2, breakpoint_));

  EXPECT_CALL(
      breakpoints_manager_,
      ClearJvmtiBreakpoint(kMethod1, kLocation2, breakpoint_))
      .WillOnce(Return());

  auto_jvmti_breakpoint_.Clear(breakpoint_);
}

}  // namespace cdbg
}  // namespace devtools


