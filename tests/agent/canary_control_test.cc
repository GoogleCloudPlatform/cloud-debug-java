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

#include "src/agent/canary_control.h"

#include <cstdint>

#include "gmock/gmock.h"

#include "src/agent/callbacks_monitor.h"
#include "tests/agent/fake_jni.h"
#include "tests/agent/mock_bridge.h"
#include "tests/agent/mock_jvmti_env.h"
#include "src/agent/model_util.h"

using testing::_;
using testing::Return;
using testing::StrictMock;

namespace devtools {
namespace cdbg {

class CanaryControlTest : public ::testing::Test {
 protected:
  CanaryControlTest()
      : global_jvm_(fake_jni_.jvmti(), fake_jni_.jni()),
        callbacks_monitor_(1000, [this] () { return current_time_ms_; }),
        canary_control_(&callbacks_monitor_, &bridge_) {
  }

  static void Unexpected(std::unique_ptr<StatusMessageModel> status) {
    FAIL() << "Unexpected callback: " << status;
  }

 protected:
  FakeJni fake_jni_;
  GlobalJvmEnv global_jvm_;
  int64_t current_time_ms_ = 100000;
  CallbacksMonitor callbacks_monitor_;
  StrictMock<MockBridge> bridge_;
  CanaryControl canary_control_;
};


TEST_F(CanaryControlTest, RegisterSuccess) {
  EXPECT_CALL(bridge_, RegisterBreakpointCanary("bp1"))
      .WillOnce(Return(true));

  EXPECT_TRUE(canary_control_.RegisterBreakpointCanary("bp1", Unexpected));
}


TEST_F(CanaryControlTest, RegisterRetry) {
  EXPECT_CALL(bridge_, RegisterBreakpointCanary("bp1"))
      .WillOnce(Return(false))
      .WillOnce(Return(false))
      .WillOnce(Return(true));

  EXPECT_TRUE(canary_control_.RegisterBreakpointCanary("bp1", Unexpected));
}


TEST_F(CanaryControlTest, RegisterFailure) {
  EXPECT_CALL(bridge_, RegisterBreakpointCanary("bp1"))
      .WillRepeatedly(Return(false));

  EXPECT_FALSE(canary_control_.RegisterBreakpointCanary("bp1", Unexpected));
}


TEST_F(CanaryControlTest, ApproveNoOp) {
  EXPECT_CALL(bridge_, RegisterBreakpointCanary("bp1"))
      .WillRepeatedly(Return(true));

  EXPECT_TRUE(canary_control_.RegisterBreakpointCanary("bp1", Unexpected));

  current_time_ms_ += 34000;

  canary_control_.ApproveHealtyBreakpoints();
}


TEST_F(CanaryControlTest, ApproveSuccess) {
  EXPECT_CALL(bridge_, RegisterBreakpointCanary("bp1"))
      .WillRepeatedly(Return(true));

  EXPECT_TRUE(canary_control_.RegisterBreakpointCanary("bp1", Unexpected));

  current_time_ms_ += 36000;

  EXPECT_CALL(bridge_, ApproveBreakpointCanary("bp1"))
      .WillRepeatedly(Return(true));

  canary_control_.ApproveHealtyBreakpoints();
}


TEST_F(CanaryControlTest, ApproveRetry) {
  EXPECT_CALL(bridge_, RegisterBreakpointCanary("bp1"))
      .WillRepeatedly(Return(true));

  EXPECT_TRUE(canary_control_.RegisterBreakpointCanary("bp1", Unexpected));

  current_time_ms_ += 36000;

  EXPECT_CALL(bridge_, ApproveBreakpointCanary("bp1"))
      .WillOnce(Return(false))
      .WillOnce(Return(false))
      .WillOnce(Return(true));

  canary_control_.ApproveHealtyBreakpoints();
}


TEST_F(CanaryControlTest, ApproveFailure) {
  EXPECT_CALL(bridge_, RegisterBreakpointCanary("bp1"))
      .WillRepeatedly(Return(true));

  EXPECT_TRUE(canary_control_.RegisterBreakpointCanary("bp1", Unexpected));

  current_time_ms_ += 36000;

  EXPECT_CALL(bridge_, ApproveBreakpointCanary("bp1"))
      .WillRepeatedly(Return(false));

  canary_control_.ApproveHealtyBreakpoints();

  EXPECT_CALL(bridge_, ApproveBreakpointCanary("bp1"))
      .WillOnce(Return(true));

  canary_control_.ApproveHealtyBreakpoints();
}


TEST_F(CanaryControlTest, ApproveUnhealthy) {
  EXPECT_CALL(bridge_, RegisterBreakpointCanary("bp1"))
      .WillRepeatedly(Return(true));

  int complete_counter = 0;
  EXPECT_TRUE(canary_control_.RegisterBreakpointCanary(
      "bp1",
      [&complete_counter] (std::unique_ptr<StatusMessageModel> status) {
        ++complete_counter;
      }));

  current_time_ms_ += 1;
  auto call_id = callbacks_monitor_.RegisterCall("stuck");
  current_time_ms_ += 36000;

  canary_control_.ApproveHealtyBreakpoints();

  EXPECT_EQ(1, complete_counter);

  callbacks_monitor_.CompleteCall(call_id);
}

}  // namespace cdbg
}  // namespace devtools


