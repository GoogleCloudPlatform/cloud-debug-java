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

#include "canary_control.h"

#include <cstdint>

#include "messages.h"
#include "model_util.h"

// The "ApproveHealtyBreakpoints" method is called from worker thread every
// cycle of "ListActiveBreakpoints", which is once every 40 seconds. The
// constant of 35 seconds is deliberately a bit shorter than that so that the
// canary period fits in one such cycle.
ABSL_FLAG(
    int32, min_canary_duration_ms, 35000,
    "Time interval after which an enabled canary breakpoint is considered as "
    "safe for a global rollout (from this debuglet's perspective)");

namespace devtools {
namespace cdbg {

// Number of attempts to register or approve a canary breakpoint before
// failing the operation.
static constexpr int kMaxAttempts = 3;

CanaryControl::CanaryControl(
    CallbacksMonitor* callbacks_monitor,
    Bridge* bridge)
    : callbacks_monitor_(callbacks_monitor),
      bridge_(bridge) {
}

bool CanaryControl::RegisterBreakpointCanary(
    const std::string& breakpoint_id,
    std::function<void(std::unique_ptr<StatusMessageModel>)> fn_complete) {
  int64_t current_timestamp_ms = callbacks_monitor_->GetCurrentTimeMillis();

  {
    absl::MutexLock lock(&mu_);
    // Fail if already in canary.
    if (canary_breakpoints_.find(breakpoint_id) != canary_breakpoints_.end()) {
      LOG(ERROR) << "Breakpoint " << breakpoint_id
                 << " already registered for canary";
      DCHECK(false);
      return false;
    }

    // Optimistically mark the breakpoint as if in canary. We don't want to
    // keep the mutex locked throughout the call to the backend.
    canary_breakpoints_[breakpoint_id] = { current_timestamp_ms, fn_complete };
  }

  for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
    if (bridge_->RegisterBreakpointCanary(breakpoint_id)) {
      return true;
    }
  }

  // Roll back the marking that the breakpoint is in canary.
  absl::MutexLock lock(&mu_);
  canary_breakpoints_.erase(breakpoint_id);

  return false;
}

void CanaryControl::BreakpointCompleted(const std::string& breakpoint_id) {
  absl::MutexLock lock(&mu_);
  canary_breakpoints_.erase(breakpoint_id);
}

void CanaryControl::ApproveHealtyBreakpoints() {
  // Choose breakpoints that can be approved.
  std::vector<std::string> healthy_ids;
  std::map<std::string, CanaryBreakpoint> unhealthy_ids;
  {
    int64_t current_timestamp_ms = callbacks_monitor_->GetCurrentTimeMillis();
    int64_t cutoff =
        current_timestamp_ms - absl::GetFlag(FLAGS_min_canary_duration_ms);

    absl::MutexLock lock(&mu_);
    for (const auto& entry : canary_breakpoints_) {
      if (entry.second.register_time > cutoff) {
        continue;  // The breakpoint hasn't spent enough time in canary.
      }

      // Declare the canary breakpoint as benign if there are no stuck
      // callbacks.
      if (callbacks_monitor_->IsHealthy(entry.second.register_time)) {
        healthy_ids.push_back(entry.first);
        continue;
      }

      LOG(WARNING) << "Long or stuck callbacks detected during canary "
                      " breakpoint period " << entry.first;
      unhealthy_ids[entry.first] = entry.second;
    }
  }

  // Try to approve the breakpoints.
  std::vector<std::string> approved_ids;
  approved_ids.reserve(healthy_ids.size());

  for (const std::string& breakpoint_id : healthy_ids) {
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
      if (bridge_->ApproveBreakpointCanary(breakpoint_id)) {
        approved_ids.push_back(breakpoint_id);
        break;
      }
    }
  }

  // Complete unhealthy breakpoints.
  for (const auto& entry : unhealthy_ids) {
    entry.second.fn_complete(StatusMessageBuilder()
        .set_error()
        .set_format(CanaryBreakpointUnhealthy)
        .build());
  }

  absl::MutexLock lock(&mu_);

  // Remove the approved breakpoints from the canary list.
  for (const std::string& breakpoint_id : approved_ids) {
    canary_breakpoints_.erase(breakpoint_id);
  }

  // We may have some unhealthy breakpoints. They have are now completed, and
  // "JvmBreakpointsManager" will call "BreakpointCompleted", so we don't
  // really have to erase them from "canary_breakpoints_". We still do it
  // just to be on the safe side.
  for (const auto& entry : unhealthy_ids) {
    canary_breakpoints_.erase(entry.first);
  }
}

}  // namespace cdbg
}  // namespace devtools
