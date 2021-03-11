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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_STOPWATCH_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_STOPWATCH_H_

#include <time.h>

#include <cstdint>
#include <functional>

#include "common.h"

namespace devtools {
namespace cdbg {

// Measures elapsed time.
class Stopwatch {
 public:
  // Starts counting time.
  Stopwatch() {
    fn_gettime_ = MonotonicClock;
    Reset();
  }

  // Starts counting time.
  explicit Stopwatch(
      std::function<int(struct timespec*)> fn_gettime) {
    fn_gettime_ = fn_gettime;
    Reset();
  }

  static int MonotonicClock(struct timespec* tp) {
    return clock_gettime(CLOCK_MONOTONIC, tp);
  }

  static int ThreadClock(struct timespec* tp) {
    return clock_gettime(CLOCK_THREAD_CPUTIME_ID, tp);
  }

  // Gets elapsed time in nanoseconds.
  int64_t GetElapsedNanos() const {
    timespec end_time;
    fn_gettime_(&end_time);

    return static_cast<int64_t>(end_time.tv_sec - start_time_.tv_sec) *
               1000000000 +
           static_cast<int64_t>(end_time.tv_nsec - start_time_.tv_nsec);
  }

  // Gets elapsed time in microseconds.
  int64_t GetElapsedMicros() const { return (GetElapsedNanos() + 500) / 1000; }

  // Gets elapsed time in milliseconds.
  int64_t GetElapsedMillis() const {
    return (GetElapsedNanos() + 500000) / 1000000;
  }

  // Reset the stopwatch, restart counting time.
  void Reset() {
    fn_gettime_(&start_time_);
  }

 private:
  std::function<int(struct timespec*)> fn_gettime_;

  timespec start_time_;

  DISALLOW_COPY_AND_ASSIGN(Stopwatch);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_STOPWATCH_H_


