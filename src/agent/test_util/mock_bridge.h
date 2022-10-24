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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_BRIDGE_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_BRIDGE_H_

#include "gmock/gmock.h"

#include "src/agent/bridge.h"

namespace devtools {
namespace cdbg {

class ClassPathLookup;

class MockBridge : public Bridge {
 public:
  MockBridge() {
    ON_CALL(*this, Bind(testing::_))
        .WillByDefault(testing::Return(true));

    ON_CALL(
        *this,
        ListActiveBreakpoints(testing::NotNull()))
        .WillByDefault(testing::Return(HangingGetResult::SUCCESS));
  }

  MOCK_METHOD(bool, Bind, (ClassPathLookup*), (override));

  MOCK_METHOD(void, Shutdown, (), (override));

  MOCK_METHOD(bool, RegisterDebuggee, (bool*, const DebuggeeLabels&),
              (override));

  MOCK_METHOD(HangingGetResult, ListActiveBreakpoints,
              (std::vector<std::unique_ptr<BreakpointModel>>*), (override));

  // See b/10817494 for more details on this workaround.
  void EnqueueBreakpointUpdate(
      std::unique_ptr<BreakpointModel> breakpoint) override {
    EnqueueBreakpointUpdateProxy(breakpoint.get());
  }

  MOCK_METHOD(void, EnqueueBreakpointUpdateProxy, (BreakpointModel*));

  MOCK_METHOD(void, TransmitBreakpointUpdates, (), (override));

  MOCK_METHOD(bool, HasPendingMessages, (), (const, override));

  MOCK_METHOD(bool, RegisterBreakpointCanary, (const std::string&), (override));

  MOCK_METHOD(bool, ApproveBreakpointCanary, (const std::string&), (override));

  MOCK_METHOD(bool, IsEnabled, (bool*), (override));
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_BRIDGE_H_
