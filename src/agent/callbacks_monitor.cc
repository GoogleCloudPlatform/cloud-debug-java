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

#include "callbacks_monitor.h"

#include <time.h>

#include <cstdint>

namespace devtools {
namespace cdbg {

static CallbacksMonitor* g_instance = nullptr;

void CallbacksMonitor::InitializeSingleton(int max_interval_ms) {
  DCHECK(g_instance == nullptr);

  g_instance = new CallbacksMonitor(max_interval_ms);
}


void CallbacksMonitor::CleanupSingleton() {
  delete g_instance;
  g_instance = nullptr;
}


CallbacksMonitor* CallbacksMonitor::GetInstance() {
  DCHECK(g_instance != nullptr);
  return g_instance;
}

int64_t CallbacksMonitor::MonotonicClockMillis() {
  struct timespec tp;
  clock_gettime(CLOCK_MONOTONIC, &tp);
  return tp.tv_sec * 1000L + tp.tv_nsec / 1000000L;
}

CallbacksMonitor::Id CallbacksMonitor::RegisterCall(const char* tag) {
  OngoingCall ongoing_call { GetCurrentTimeMillis(), tag };

  std::lock_guard<std::mutex> lock(mu_);
  return ongoing_calls_.insert(ongoing_calls_.begin(), ongoing_call);
}


void CallbacksMonitor::CompleteCall(CallbacksMonitor::Id id) {
  int64_t current_time_ms = GetCurrentTimeMillis();

  std::lock_guard<std::mutex> lock(mu_);

  if (current_time_ms - id->start_time_ms > max_call_duration_ms_) {
    LOG(INFO) << "Cloud Debugger call \"" << id->tag
              << "\" completed after " << current_time_ms - id->start_time_ms
              << " ms";
    last_unhealthy_time_ms_ = current_time_ms;
  }

  ongoing_calls_.erase(id);
}

bool CallbacksMonitor::IsHealthy(int64_t timestamp) const {
  bool rc = true;
  int64_t current_time_ms = GetCurrentTimeMillis();

  std::lock_guard<std::mutex> lock(mu_);

  if (last_unhealthy_time_ms_ >= timestamp) {
    LOG(WARNING) << "Unhealthy callback completed "
                 << current_time_ms - last_unhealthy_time_ms_ << " ms ago";
    return false;
  }

  for (auto it = ongoing_calls_.begin(); it != ongoing_calls_.end(); ++it) {
    int64_t duration_ms = current_time_ms - it->start_time_ms;
    if (duration_ms > max_call_duration_ms_) {
      LOG(WARNING) << "Cloud Debugger call \"" << it->tag
                   << "\" hasn't completed in " << duration_ms
                   << " ms, possibly stuck";
      rc = false;
    }
  }

  return rc;
}

}  // namespace cdbg
}  // namespace devtools




