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

#ifndef DEVTOOLS_CDBG_COMMON_CALLBACKS_MONITOR_H_
#define DEVTOOLS_CDBG_COMMON_CALLBACKS_MONITOR_H_

#include <cstdint>
#include <functional>
#include <list>
#include <mutex>  // NOLINT

#include "common.h"

namespace devtools {
namespace cdbg {

// Keeps track of all the ongoing and past calls to allow detection of
// stuck ones or those that just take too much time. Once a stuck callback is
// detected, the caller should declare the agent as unhealthy.
//
// The class is optimized for performance of registering/completing new calls.
class CallbacksMonitor {
 public:
  struct OngoingCall {
    int64_t start_time_ms;
    const char* tag;
  };

  typedef std::list<OngoingCall>::iterator Id;

  explicit CallbacksMonitor(
      int max_call_duration_ms,
      std::function<int64_t()> fn_gettime = MonotonicClockMillis)
      : max_call_duration_ms_(max_call_duration_ms),
        fn_gettime_(fn_gettime),
        last_unhealthy_time_ms_(-1) {}

  ~CallbacksMonitor() {
    if (!ongoing_calls_.empty()) {
      // As a part of the debugger's exit routine, we disable JVMTI event
      // delivery. However, SetEventNotificationMode(JVMTI_DISABLE) does not
      // wait for the pending events, and therefore, we might end up with some
      // ongoing calls while destructing this class. This race condition rarely
      // happens, and we use a simple sleep-based optimistic workaround rather
      // than a more complex synchronization-based solution.
      LOG(WARNING) << "Waiting for 10 seconds for ongoing calls to finish";
      usleep(10*1000);
    }
  }

  // One time initialization of the global instance.
  static void InitializeSingleton(int max_interval_ms);

  // One time cleanup of the global instance.
  static void CleanupSingleton();

  // Gets the global instance of this class.
  static CallbacksMonitor* GetInstance();

  // Gets the current time.
  int64_t GetCurrentTimeMillis() const { return fn_gettime_(); }

  // Notifies start of an operation to monitor. The "tag" is a human readable
  // name of this callback. It is only used for logging purposes.
  Id RegisterCall(const char* tag);

  // Notifies completion of an operation started with "RegisterCall".
  void CompleteCall(Id id);

  // Returns true if there are no ongoing calls that already take more than
  // "max_interval_ms_" and that no completed call took more than
  // "max_interval_ms_" after "timestamp" time.
  bool IsHealthy(int64_t timestamp) const;

  // Returns the current time in milliseconds.
  static int64_t MonotonicClockMillis();

 private:
  // Maximum allowed duration of the healthy callback.
  const int max_call_duration_ms_;

  // Function to get the current time. Defined explicitly for unit tests.
  std::function<int64_t()> fn_gettime_;

  // Protects access to "ongoing_calls_" and "last_unhealthy_timestamp_".
  mutable std::mutex mu_;

  // Linked list of currently active calls to monitor.
  std::list<OngoingCall> ongoing_calls_;

  // Timestamp of the completion of last callback that lasted more than
  // "max_interval_ms_".
  int64_t last_unhealthy_time_ms_;

  DISALLOW_COPY_AND_ASSIGN(CallbacksMonitor);
};


// Automatically calls "RegisterCall", "CompleteCall" on entry and scope exit.
class ScopedMonitoredCall {
 public:
  explicit ScopedMonitoredCall(const char* tag)
      : id_(CallbacksMonitor::GetInstance()->RegisterCall(tag)) {
  }

  ~ScopedMonitoredCall() {
    CallbacksMonitor::GetInstance()->CompleteCall(id_);
  }

 private:
  const CallbacksMonitor::Id id_;

  DISALLOW_COPY_AND_ASSIGN(ScopedMonitoredCall);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_COMMON_CALLBACKS_MONITOR_H_
