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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_CANARY_CONTROL_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_CANARY_CONTROL_H_

#include <cstdint>
#include <functional>
#include <map>

#include "callbacks_monitor.h"
#include "bridge.h"
#include "common.h"
#include "model.h"
#include "mutex.h"

namespace devtools {
namespace cdbg {

// Keeps track of canary breakpoints and approves them as necessary.
// This class is thread safe.
class CanaryControl {
 public:
  CanaryControl(CallbacksMonitor* callbacks_monitor, Bridge* bridge);

  // Tries to register the breakpoint for canary. Returns false on failure.
  // The caller must not activate the breakpoint until this call succeeds.
  // The "fn_complete" argument is a function that will finalize and complete
  // the breakpoint (used when the breakpoint is determined to be unhealthy).
  bool RegisterBreakpointCanary(
      const std::string& breakpoint_id,
      std::function<void(std::unique_ptr<StatusMessageModel>)> fn_complete);

  // Indicates that the breakpoint has been finalized. This automatically
  // takes out the breakpoint from a canary.
  void BreakpointCompleted(const std::string& breakpoint_id);

  // Approves all the canary breakpoints that have been tested for
  // the necessary period of time and the debuglet asserted to be harmless.
  void ApproveHealtyBreakpoints();

 private:
  struct CanaryBreakpoint {
    // Time (in milliseconds) when the breakpoint was registered for canary.
    int64_t register_time;

    // Callback to complete the breakpoint with the specified status.
    std::function<void(std::unique_ptr<StatusMessageModel>)> fn_complete;
  };

  // Monitors all callbacks into the agent to detect those that may be stuck.
  CallbacksMonitor* const callbacks_monitor_;

  // Implements calling "RegisterBreakpointCanary" and
  // "ApproveBreakpointCanary" on the backend.
  Bridge* const bridge_;

  // Locks access to all breakpoint related data structures.
  absl::Mutex mu_;

  // List of breakpoints currently in canary. The key is the breakpoint ID.
  std::map<std::string, CanaryBreakpoint> canary_breakpoints_;

  DISALLOW_COPY_AND_ASSIGN(CanaryControl);
};

}  // namespace cdbg
}  // namespace devtools


#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_CANARY_CONTROL_H_
