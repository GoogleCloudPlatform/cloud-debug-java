/**
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "src/agent/callbacks_monitor.h"

#include <cstdint>

#include "gtest/gtest.h"

namespace devtools {
namespace cdbg {

class CallbacksMonitorTest : public ::testing::Test {
 public:
  CallbacksMonitorTest()
     : stuck_calls_monitor_(10, [this] () { return current_time_ms_; }) {
  }

 protected:
  int64_t current_time_ms_ = 100000;
  CallbacksMonitor stuck_calls_monitor_;
};


TEST_F(CallbacksMonitorTest, Empty) {
  EXPECT_TRUE(stuck_calls_monitor_.IsHealthy(1));
}


TEST_F(CallbacksMonitorTest, OngoingCalls) {
  auto id1 = stuck_calls_monitor_.RegisterCall("first");
  current_time_ms_+= 3;

  auto id2 = stuck_calls_monitor_.RegisterCall("second");
  current_time_ms_ += 4;

  EXPECT_TRUE(stuck_calls_monitor_.IsHealthy(current_time_ms_ - 5));

  stuck_calls_monitor_.CompleteCall(id1);
  stuck_calls_monitor_.CompleteCall(id2);
}


TEST_F(CallbacksMonitorTest, CompletedCall) {
  auto id1 = stuck_calls_monitor_.RegisterCall("first");
  stuck_calls_monitor_.CompleteCall(id1);

  current_time_ms_+= 10;

  EXPECT_TRUE(stuck_calls_monitor_.IsHealthy(1));
}


TEST_F(CallbacksMonitorTest, StuckOngoingCall) {
  auto id1 = stuck_calls_monitor_.RegisterCall("first");
  current_time_ms_+= 10;

  auto id2 = stuck_calls_monitor_.RegisterCall("second");
  current_time_ms_ += 2;

  EXPECT_FALSE(stuck_calls_monitor_.IsHealthy(current_time_ms_ - 5));

  stuck_calls_monitor_.CompleteCall(id1);
  stuck_calls_monitor_.CompleteCall(id2);
}


TEST_F(CallbacksMonitorTest, LongPastCall) {
  auto id1 = stuck_calls_monitor_.RegisterCall("first");
  current_time_ms_+= 11;

  stuck_calls_monitor_.CompleteCall(id1);

  EXPECT_FALSE(stuck_calls_monitor_.IsHealthy(current_time_ms_ - 1));
  EXPECT_TRUE(stuck_calls_monitor_.IsHealthy(current_time_ms_ + 1));
}


}  // namespace cdbg
}  // namespace devtools
