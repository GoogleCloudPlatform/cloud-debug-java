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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_SCHEDULER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_SCHEDULER_H_

#include <functional>
#include <map>
#include <memory>

#include "common.h"
#include "mutex.h"

namespace devtools {
namespace cdbg {

// Schedules callbacks to be invoked some time in the future. The precision
// of timing depends on the frequency that "Process" method is called.
// This class is thread safe.
template <class... Args>
class Scheduler {
 public:
  // Type for callback to be invoked.
  typedef std::function<void(Args...)> Callback;

  // Primary data structure used by this class. Maps absolute time when the
  // callback is scheduled to be invoked to the callback functor. Usage of
  // "multimap" ensures we can schedule multiple callbacks for the same time
  // (either same callback or different callback).
  typedef typename std::multimap<
      time_t,
      std::pair<int, Callback>> Events;

  // Cancellation token type. The key is timestamp (for efficient lookup in
  // "events_" and unique schedule ID.
  typedef std::pair<time_t, int> Id;

  // Invalid value of scheduled item "Id". Useful for initialization value of
  // an "Id" variable.
  static const Id NullId;

  // Default clock function to be used everywhere except of unit tests.
  static time_t DefaultClock() {
    return time(nullptr);
  }

  explicit Scheduler(std::function<time_t()> clock)
      : clock_(clock) {
  }

  ~Scheduler() {
  }

  // Gets the current time (in seconds) according to the clock specified in
  // constructor.
  time_t CurrentTime() const { return clock_(); }

  // Schedules the callback to be executed at the specified time. The scheduler
  // will hold a weak reference to the target object. The callback is not
  // invoked if the last reference to the object is released by the time
  // the call is scheduled.
  template <typename T>
  Id Schedule(
      time_t time,
      std::weak_ptr<T> target,
      void (T::*fn)(Args...)) {
    Callback callback = [target, fn](Args... args) {
      auto locked_target = target.lock();
      if (locked_target == nullptr) {
        // The target object has already expired.
        return;
      }

      T& t = *locked_target;
      (t.*fn)(args...);
    };

    absl::MutexLock lock(&mu_);

    int cancellation_id = ++last_id_;

    events_.insert(
        std::make_pair(
            time,
            std::make_pair(cancellation_id, std::move(callback))));

    return { time, cancellation_id };
  }

  // Cancels the scheduled callback or does nothing if the specified item
  // has already been completed or being executed. Returns true if the item has
  // been cancelled. Calling "Cancel" with "NullId" has no effect.
  bool Cancel(Id id) {
    absl::MutexLock lock(&mu_);

    auto it = events_.lower_bound(id.first);
    while ((it != events_.end()) && (it->first == id.first)) {
      if (it->second.first == id.second) {
        events_.erase(it);
        return true;
      }

      ++it;
    }

    return false;
  }

  // Invokes all the callbacks scheduled from up to the current time. Completed
  // callbacks are removed from the list.
  void Process(Args... args) {
    // Gather all the callbacks we need to invoke first.
    std::vector<Callback> current_callbacks;
    const time_t time = CurrentTime();

    {
      absl::MutexLock lock(&mu_);
      for (auto it = events_.begin(); it != events_.end(); ) {
        // "events_" is sorted by the scheduled time. Once we saw the first item
        // scheduled in the future, we can stop looking any further.
        if (it->first > time) {
          break;
        }

        current_callbacks.push_back(std::move(it->second.second));
        it = events_.erase(it);
      }
    }

    // Now invoke these callbacks without lock. This allows the callback to
    // call Cancel. Each callback holds a weak reference. Therefore if an
    // object has just been deleted, the callback will do nothing.
    for (auto& callback : current_callbacks) {
      callback(args...);
    }
  }

 private:
  // Global counter to assign unique cancellation IDs.
  int last_id_ { 0 };

  // Clock function. Used to override in unit tests.
  const std::function<time_t()> clock_;

  // Locks access to "handlers_" and "last_id_" list in this class.
  mutable absl::Mutex mu_;

  // List of scheduled items.
  Events events_;

  DISALLOW_COPY_AND_ASSIGN(Scheduler);
};


template <class... Args>
const typename Scheduler<Args...>::Id Scheduler<Args...>::NullId;

}  // namespace cdbg
}  // namespace devtools


#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_SCHEDULER_H_
