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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_RATE_LIMIT_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_RATE_LIMIT_H_

#include <cstdint>
#include <memory>
#include <queue>

#include "common.h"
#include "mutex.h"

namespace devtools {
namespace cdbg {

class LeakyBucket;

//
// Each rate limit is defined as the maximum amount of time in nanoseconds to
// spend on particular processing per second per CPU. These rate are enforced
// as following:
// 1. If a single breakpoint contributes to half the maximum rate, that
//    breakpoint will be deactivated.
// 2. If all breakpoints combined hit the maximum rate, any breakpoint to
//    exceed the limit gets disabled.
//
// The global limit is for all CPUs combined (we assume that multiple
// breakpoints will hit different CPUs). We don't make this assumption
// for per-breakpoint limit.
//
// The first rule ensures that in vast majority of scenarios expensive
// breakpoints will get deactivated. The second rule guarantees that in edge
// case scenarios the total amount of time spent in condition evaluation will
// not exceed the allotted limit.
//
// The simplest way to measure the time of each event is to use "clock_gettime"
// function. However in garbage collection environments like Java this can
// yield false positives if JVM triggers garbage collection while the debuglet
// is evaluating the condition or writing a log statement. To alleviate the
// effect of garbage collector, we apply moving average filter to time
// measurements. It would be better to use median, but there are no efficient
// algorithms to compute a running median.
//

// Types of cost limits we have in the debuglet.
enum class CostLimitType {
  BreakpointCondition,
  DynamicLog,
  DynamicLogBytes,
};

// Creates instance of "LeakyBucket" to enforce global cost.
std::unique_ptr<LeakyBucket> CreateGlobalCostLimiter(CostLimitType type);

// Creates instance of "LeakyBucket" to enforce per breakpoint cost.
std::unique_ptr<LeakyBucket> CreatePerBreakpointCostLimiter(CostLimitType type);

// Thread safe moving average computation.
class MovingAverage {
 public:
  void Add(int64_t value);

  int64_t Average() const;

  int IsFilled() const;

  void Reset();

 private:
  // We compute the k-term moving average. The choice of 32 is arbitrary.
  // TODO: adjust it to optimal value.
  const int max_size_ { 32 };

  // Lock access to "window_" and "sum_".
  mutable absl::Mutex mu_;

  // Last k values.
  std::queue<int64_t> window_;

  // Total of last k values.
  int64_t sum_{0};
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_RATE_LIMIT_H_
