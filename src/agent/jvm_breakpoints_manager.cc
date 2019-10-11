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

#include "jvm_breakpoints_manager.h"

#include <algorithm>

#include "callbacks_monitor.h"
#include "breakpoint.h"
#include "format_queue.h"
#include "jvm_evaluators.h"
#include "rate_limit.h"
#include "statistician.h"

namespace devtools {
namespace cdbg {

JvmBreakpointsManager::JvmBreakpointsManager(
    std::function<std::shared_ptr<Breakpoint>(
        BreakpointsManager*,
        std::unique_ptr<BreakpointModel>)> breakpoint_factory,
    JvmEvaluators* evaluators,
    FormatQueue* format_queue,
    CanaryControl* canary_control)
    : breakpoint_factory_(breakpoint_factory),
      evaluators_(evaluators),
      format_queue_(format_queue),
      canary_control_(canary_control),
      global_condition_cost_limiter_(
          CreateGlobalCostLimiter(CostLimitType::BreakpointCondition)),
      global_dynamic_log_limiter_(
          CreateGlobalCostLimiter(CostLimitType::DynamicLog)),
      global_dynamic_log_bytes_limiter_(
          CreateGlobalCostLimiter(CostLimitType::DynamicLogBytes)) {
  on_class_prepared_cookie_ =
      evaluators_->class_indexer->SubscribeOnClassPreparedEvents(
          std::bind(
              &JvmBreakpointsManager::OnClassPrepared,
              this,
              std::placeholders::_1,
              std::placeholders::_2));
}


JvmBreakpointsManager::~JvmBreakpointsManager() {
}


void JvmBreakpointsManager::Cleanup() {
  // Release all references held by compiled expressions in "Breakpoint". This
  // has to be done without "mu_data_" lock because "Breakpoint" calls back
  // into "JvmBreakpointsManager" and "mu_data_" is not reentrant.
  for (const std::shared_ptr<Breakpoint>& entry : GetActiveBreakpoints()) {
    entry->ResetToPending();
  }

  // We don't expect any pending calls during Cleanup, so all breakpoint
  // objects should be unloaded by now.
  DCHECK(method_map_.empty());

  evaluators_->class_indexer->UnsubscribeOnClassPreparedEvents(
      std::move(on_class_prepared_cookie_));

  format_queue_->RemoveAll();
}


void JvmBreakpointsManager::SetActiveBreakpointsList(
    std::vector<std::unique_ptr<BreakpointModel>> breakpoints) {
  ScopedStat ss(statBreakpointsUpdateTime);

  // Serialize simultaneous calls to "SetActiveBreakpointsList".
  absl::MutexLock lock_set_active_breakpoints_list(
      &mu_set_active_breakpoints_list_);

  std::map<std::string, std::shared_ptr<Breakpoint>> updated_active_breakpoints;
  std::set<std::string> updated_completed_breakpoints;
  std::vector<std::unique_ptr<BreakpointModel>> new_breakpoints;

  // Identify deleted and new breakpoints.
  {
    ScopedMonitoredCall monitored_call(
        "BreakpointsManager:SetActiveBreakpoints:Scan");

    absl::MutexLock lock_data(&mu_data_);

    for (int i = 0; i < breakpoints.size(); ++i) {
      std::unique_ptr<BreakpointModel> breakpoint = std::move(breakpoints[i]);

      const std::string& id = breakpoint->id;

      // Ignore completed breakpoints.
      if (completed_breakpoints_.find(id) != completed_breakpoints_.end()) {
        updated_completed_breakpoints.insert(id);
        continue;
      }

      // Keep already active breakpoints.
      auto it = active_breakpoints_.find(id);
      if (it != active_breakpoints_.end()) {
        updated_active_breakpoints.insert(*it);
        it->second = nullptr;
      } else {
        new_breakpoints.push_back(std::move(breakpoint));
      }
    }

    active_breakpoints_.swap(updated_active_breakpoints);

    // Remove entries from completed_breakpoints_ that weren't listed in
    // "breakpoints" array. These are confirmed to have been removed by the hub
    // and the debuglet can now assume that they will never show up ever again.
    completed_breakpoints_.swap(updated_completed_breakpoints);
  }

  // Create new breakpoints.
  for (std::unique_ptr<BreakpointModel>& new_breakpoint : new_breakpoints) {
    bool is_canary = new_breakpoint->is_canary;
    std::shared_ptr<Breakpoint> jvm_breakpoint =
        breakpoint_factory_(this, std::move(new_breakpoint));

    if (is_canary) {
      if (canary_control_ != nullptr) {
        if (!canary_control_->RegisterBreakpointCanary(
                jvm_breakpoint->id(),
                std::bind(&Breakpoint::CompleteBreakpointWithStatus,
                          jvm_breakpoint,
                          std::placeholders::_1))) {
          LOG(WARNING) << "Failed to register canary breakpoint, skipping...";
          continue;
        }
      } else {
        LOG(ERROR) << "Breakpoint canary ignored";
      }
    }

    ScopedMonitoredCall monitored_call(
        "BreakpointsManager:SetActiveBreakpoints:SetNewBreakpoint");

    LOG(INFO) << "Setting new breakpoint: " << jvm_breakpoint->id();

    {
      absl::MutexLock lock_data(&mu_data_);
      active_breakpoints_.insert(
          std::make_pair(jvm_breakpoint->id(), jvm_breakpoint));
    }

    // Is it the responsibility of "Breakpoint" to properly deal with any
    // errors (sending final breakpoint update and completing the breakpoint).
    jvm_breakpoint->Initialize();
  }

  // Remove breakpoints that were not listed.
  for (auto& breakpoint : updated_active_breakpoints) {
    ScopedMonitoredCall monitored_call(
        "BreakpointsManager:SetActiveBreakpoints:RemoveCompletedBreakpoint");

    if (breakpoint.second != nullptr) {
      LOG(INFO) << "Completing breakpoint " << breakpoint.second->id()
                << " (removed from active list by backend)";
      breakpoint.second->ResetToPending();
      CompleteBreakpoint(breakpoint.second->id());
    }
  }

  // TODO: remove breakpoints that the hub doesn't care about from
  // format_queue_. It needs an efficient lookup and special care about the
  // top element that might be transmitted right now.
}


// Note: JNIEnv* is not available through jni() call.
void JvmBreakpointsManager::JvmtiOnCompiledMethodUnload(jmethodID method) {
  // Each breakpoint holds a global reference to the class that contains
  // the code on which the breakpoint is set. This guarantees that a method
  // with breakpoint will never get unloaded. Verify it here.

  absl::MutexLock lock_data(&mu_data_);
  if (method_map_.find(method) != method_map_.end()) {
    LOG(ERROR) << "Method with breakpoint is being unloaded"
                  ", method = " << method;
  }
}


void JvmBreakpointsManager::JvmtiOnBreakpoint(
    jthread thread,
    jmethodID method,
    jlocation location) {
  // Identify the list of breakpoints that were hit.
  std::vector<std::shared_ptr<Breakpoint>> breakpoints;

  {
    absl::MutexLock lock_data(&mu_data_);

    auto it_method = method_map_.find(method);
    if (it_method == method_map_.end()) {
      // This can happen once in a while: two threads are executing a function
      // that has a breakpoint and hit the breakpoint simultaneously. By the
      // time thread B gets to "JvmtiOnBreakpoint", thread A already finished
      // the evaluation and completed breakpoint.
      // If this situation happens often, it indicates a bug.
      LOG(INFO) << "Breakpoint hit on a method without breakpoints, method = "
                << method << ", location: "
                << std::hex << std::showbase << location;
      return;
    }

    for (auto& breakpoint_location : it_method->second) {
      if (breakpoint_location.first == location) {
        DCHECK_EQ(std::count(
                      breakpoints.begin(),
                      breakpoints.end(),
                      breakpoint_location.second),
                  0);

        breakpoints.push_back(breakpoint_location.second);
      }
    }
  }

  if (breakpoints.empty()) {
    LOG(WARNING) << "No locations matched on breakpoint hit, method = "
                 << method << ", location: "
                 << std::hex << std::showbase << location;
    return;
  }

  // Process the breakpoint hits.
  for (std::shared_ptr<Breakpoint> breakpoint : breakpoints) {
    breakpoint->OnJvmBreakpointHit(
          thread,
          method,
          location);
  }
}

void JvmBreakpointsManager::CompleteBreakpoint(std::string breakpoint_id) {
  if (canary_control_ != nullptr) {
    canary_control_->BreakpointCompleted(breakpoint_id);
  }

  absl::MutexLock lock_data(&mu_data_);

  // Do nothing if the breakpoint is not in active list. It is possible that
  // two threads hit breakpoint simultaneously and both call
  // "CompleteBreakpoint" for the same breakpoint.
  auto it = active_breakpoints_.find(breakpoint_id);
  if (it != active_breakpoints_.end()) {
    LOG(INFO) << "Breakpoint " << breakpoint_id
              << " removed from active breakpoints list";

    active_breakpoints_.erase(it);
  }

  completed_breakpoints_.insert(breakpoint_id);

  // It is still possible that some other threads are processing breakpoint
  // hit or other events for the completed breakpoint.
}

bool JvmBreakpointsManager::SetJvmtiBreakpoint(
    jmethodID method,
    jlocation location,
    std::shared_ptr<Breakpoint> jvm_breakpoint) {
  absl::MutexLock lock_data(&mu_data_);

  std::vector<std::pair<jlocation, std::shared_ptr<Breakpoint>>> empty;
  auto it_method = method_map_.insert(std::make_pair(method, empty)).first;

  auto& location_list = it_method->second;

  const bool is_new =
    std::count_if(
        location_list.begin(),
        location_list.end(),
        [location] (std::pair<jlocation, std::shared_ptr<Breakpoint>>& p) {
          return p.first == location;
        }) == 0;

  if (is_new) {
    LOG(INFO) << "Setting new JVMTI breakpoint, method = " << method
              << ", location = " << std::hex << std::showbase << location;

    jvmtiError err = jvmti()->SetBreakpoint(method, location);
    if (err != JVMTI_ERROR_NONE) {
      LOG(ERROR) << "Failed to set a breakpoint, method = " << method
                 << ", location = " << std::hex << std::showbase << location
                 << ", err = " << err;

      return false;
    }
  }

  location_list.push_back(std::make_pair(location, jvm_breakpoint));

  return true;
}


void JvmBreakpointsManager::ClearJvmtiBreakpoint(
    jmethodID method,
    jlocation location,
    std::shared_ptr<Breakpoint> jvm_breakpoint) {
  absl::MutexLock lock_data(&mu_data_);

  jvmtiError err = JVMTI_ERROR_NONE;

  auto it_method = method_map_.find(method);
  if (it_method == method_map_.end()) {
    return;
  }

  auto& location_list = it_method->second;
  int remove_count = 0;
  int skip_count = 0;
  for (auto it_location = location_list.begin();
       it_location != location_list.end(); ) {
    if (it_location->second == jvm_breakpoint) {
      it_location = location_list.erase(it_location);
      ++remove_count;
    } else {
      if (it_location->first == location) {
        // We found some other breakpoint at the same statement. It means that
        // the JVMTI breakpoint should stay for the benefit of the other
        // breakpoint.
        ++skip_count;
      }
      ++it_location;
    }
  }

  DCHECK_EQ(remove_count, 1);
  LOG_IF(WARNING, remove_count != 1)
      << "Code location found " << remove_count << " times "
         "in method_map_ (only once expected)";

  // Clear the JVMTI breakpoint if no more breakpoints need this location.
  if (skip_count == 0) {
    LOG(INFO) << "Clearing JVMTI breakpoint, method = " << method
              << ", location = " << location;

    err = jvmti()->ClearBreakpoint(method, location);
    if (err != JVMTI_ERROR_NONE) {
      LOG(ERROR) << std::hex << std::showbase
                 << "Failed to clear the breakpoint, method = " << method
                 << ", location = " << location << ", err = " << err
                 << ", ignoring...";
    }
  }

  // Clean up the entry in method_map_ (small performance optimization).
  if (location_list.empty()) {
    method_map_.erase(it_method);
  }
}


std::vector<std::shared_ptr<Breakpoint>>
JvmBreakpointsManager::GetActiveBreakpoints() {
  absl::MutexLock lock_data(&mu_data_);

  std::vector<std::shared_ptr<Breakpoint>> breakpoints;
  breakpoints.reserve(active_breakpoints_.size());

  for (const auto& active_breakpoint : active_breakpoints_) {
    breakpoints.push_back(active_breakpoint.second);
  }

  return breakpoints;
}

void JvmBreakpointsManager::OnClassPrepared(
    const std::string& type_name, const std::string& class_signature) {
  // Propagate the event to all active breakpoints. Let each breakpoint decide
  // whether it needs to take action.
  for (const auto& breakpoint : GetActiveBreakpoints()) {
    breakpoint->OnClassPrepared(type_name, class_signature);
  }
}

}  // namespace cdbg
}  // namespace devtools


