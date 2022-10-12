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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_BREAKPOINTS_MANAGER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_BREAKPOINTS_MANAGER_H_

#include "gmock/gmock.h"

#include "breakpoints_manager.h"
#include "model_json.h"

namespace devtools {
namespace cdbg {

class MockBreakpointsManager : public BreakpointsManager {
 public:
  MOCK_METHOD(void, Cleanup, (), (override));

  // See b/10817494 for more details on this workaround.
  void SetActiveBreakpointsList(
      std::vector<std::unique_ptr<BreakpointModel>> breakpoints) override {
    std::vector<std::string> json_breakpoints;
    for (const auto& breakpoint : breakpoints) {
      json_breakpoints.push_back(BreakpointToPrettyJson(*breakpoint).data);
    }

    SetActiveBreakpointsList(json_breakpoints);
  }

  // See b/10817494 for more details on this workaround.
  MOCK_METHOD(void, SetActiveBreakpointsList,
              (const std::vector<std::string>&));

  MOCK_METHOD(void, JvmtiOnCompiledMethodUnload, (jmethodID), (override));

  MOCK_METHOD(void, JvmtiOnBreakpoint, (jthread, jmethodID, jlocation),
              (override));

  MOCK_METHOD(bool, SetJvmtiBreakpoint,
              (jmethodID, jlocation, std::shared_ptr<Breakpoint>), (override));

  MOCK_METHOD(void, ClearJvmtiBreakpoint,
              (jmethodID, jlocation, std::shared_ptr<Breakpoint>), (override));

  MOCK_METHOD(void, CompleteBreakpoint, (std::string), (override));

  MOCK_METHOD(LeakyBucket*, GetGlobalConditionCostLimiter, (), (override));

  MOCK_METHOD(LeakyBucket*, GetGlobalDynamicLogLimiter, (), (override));

  MOCK_METHOD(LeakyBucket*, GetGlobalDynamicLogBytesLimiter, (), (override));
};

}  // namespace cdbg
}  // namespace devtools


#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_BREAKPOINTS_MANAGER_H_
