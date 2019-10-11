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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_BREAKPOINTS_MANAGER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_BREAKPOINTS_MANAGER_H_

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "leaky_bucket.h"
#include "breakpoints_manager.h"
#include "canary_control.h"
#include "class_indexer.h"
#include "common.h"
#include "mutex.h"

namespace devtools {
namespace cdbg {

struct JvmEvaluators;
class FormatQueue;

// Manages list of active breakpoints and processes breakpoint hit events.
// This class is thread safe.
class JvmBreakpointsManager : public BreakpointsManager {
 public:
  // "evaluators" and "format_queue" not owned by this class and must outlive
  // this class.
  JvmBreakpointsManager(
    std::function<std::shared_ptr<Breakpoint>(
        BreakpointsManager*,
        std::unique_ptr<BreakpointModel>)> breakpoint_factory,
    JvmEvaluators* evaluators,
    FormatQueue* format_queue,
    CanaryControl* canary_control);

  ~JvmBreakpointsManager() override;

  void Cleanup() override;

  void SetActiveBreakpointsList(
      std::vector<std::unique_ptr<BreakpointModel>> breakpoints) override;

  void JvmtiOnCompiledMethodUnload(jmethodID method) override;

  void JvmtiOnBreakpoint(
      jthread thread,
      jmethodID method,
      jlocation location) override;

  bool SetJvmtiBreakpoint(
      jmethodID method,
      jlocation location,
      std::shared_ptr<Breakpoint> breakpoint) override;

  void ClearJvmtiBreakpoint(
      jmethodID method,
      jlocation location,
      std::shared_ptr<Breakpoint> breakpoint) override;

  // "breakpoint_id" is not reference because it is a string that might get
  // deleted when breakpoint gets completed.
  void CompleteBreakpoint(std::string breakpoint_id) override;

  LeakyBucket* GetGlobalConditionCostLimiter() override {
    return global_condition_cost_limiter_.get();
  }

  LeakyBucket* GetGlobalDynamicLogLimiter() override {
    return global_dynamic_log_limiter_.get();
  }

  LeakyBucket* GetGlobalDynamicLogBytesLimiter() override {
    return global_dynamic_log_bytes_limiter_.get();
  }

 private:
  // Copies current list of active breakpoints (under "mu_data_" lock) into
  // a temporary list. The motivation to copy is to avoid lock while iterating
  // through "active_breakpoints_". All calls to "Breakpoint" have to be made
  // without lock. Otherwise we will get deadlock since "Breakpoint" may cause
  // other JVMTI callbacks to happen.
  std::vector<std::shared_ptr<Breakpoint>> GetActiveBreakpoints();

  // Callback invoked when JVM initialized (aka prepared) a Java class.
  void OnClassPrepared(const std::string& type_name,
                       const std::string& class_signature);

 private:
  // Functor to create new instances of "Breakpoint".
  const std::function<std::shared_ptr<Breakpoint>(
      BreakpointsManager*,
      std::unique_ptr<BreakpointModel>)> breakpoint_factory_;

  // Bundles all the evaluation classes together. Not owned by this class.
  JvmEvaluators* const evaluators_;

  // Breakpoint hit results that wait to be reported to the hub.  Not owned by
  // this class.
  FormatQueue* const format_queue_;

  // Optional manager of canary breakpoints.
  CanaryControl* const canary_control_;

  // Registration of a callbacks when a class has been loaded.
  ClassIndexer::OnClassPreparedEvent::Cookie on_class_prepared_cookie_;

  // Locks access to all breakpoint related data structures.
  absl::Mutex mu_data_;

  // Serializes calls to "SetActiveBreakpointsList" so that two simultaneous
  // calls don't intermingle.
  absl::Mutex mu_set_active_breakpoints_list_;

  // List of currently active breakpoints (keyed by breakpoint ID).
  std::map<std::string, std::shared_ptr<Breakpoint>> active_breakpoints_;

  // List of recently completed breakpoint IDs that were removed from
  // "active_breakpoints_" and should not go back even if listed
  // in "SetActiveBreakpointsList" in case of a race condition between
  // reporting a breakpoint hit and receiving list of active breakpoints
  // from the server. Entries are removed from the set when hub stops listing
  // the breakpoint as active.
  std::set<std::string> completed_breakpoints_;

  // Reverse map of breakpoints. "method_map_" allows quick lookup of
  // breakpoint given code location (useful on breakpoint hit) and lookup
  // of all breakpoints in a certain method (to support method unload).
  std::map<
    jmethodID,
    std::vector<
      std::pair<jlocation,
                std::shared_ptr<Breakpoint>>>> method_map_;

  // Global limit of the cost of condition checks.
  const std::unique_ptr<LeakyBucket> global_condition_cost_limiter_;

  // Global limit on total number of dynamic logs.
  const std::unique_ptr<LeakyBucket> global_dynamic_log_limiter_;

  // Global limit on total number of dynamic log bytes.
  const std::unique_ptr<LeakyBucket> global_dynamic_log_bytes_limiter_;

  DISALLOW_COPY_AND_ASSIGN(JvmBreakpointsManager);
};


}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_BREAKPOINTS_MANAGER_H_
