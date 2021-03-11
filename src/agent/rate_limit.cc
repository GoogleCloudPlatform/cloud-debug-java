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

#include "rate_limit.h"

#include <cstdint>
#include <thread>  // NOLINT

#include "leaky_bucket.h"

//
// See comment in rate_limit.h explaining the meaning of these flags and how
// they are used.
//

ABSL_FLAG(
    double, max_condition_cost,
    0.01,  // 1% of CPU time
    "maximum cost in percentage of CPU consumption of condition evaluation");

// This constant defines the fill rate for the leaky bucket for logs per second
// limit. The capacity is computed as
//   "FLAGS_max_dynamic_log_rate * kDynamicLogCapacityFactor".
ABSL_FLAG(
    double, max_dynamic_log_rate,
    12,  // maximum of 12 log entries per second on average
    "maximum rate of dynamic log entries in this process; short bursts are "
    "allowed to exceed this limit");

// This constant defines the fill rate for the leaky bucket for log bytes per
// second. The capacity is computed as
//   "FLAGS_max_dynamic_log_rate_bytes * kDynamicLogBytesCapacityFactor".
ABSL_FLAG(double, max_dynamic_log_bytes_rate,
          20480,  // maximum of 20K bytes per second on average
          "maximum rate of dynamic log bytes in this process; short bursts are "
          "allowed to exceed this limit");

namespace devtools {
namespace cdbg {

//
// 1 leaky bucket token = 1 nanosecond (of a single CPU).
//

// Defines capacity of leaky bucket. The capacity is calculated as:
//    capacity = fill_rate * capacity_factor.
//
// The capacity is conceptually unrelated to fill rate, but we don't want to
// expose this knob to the developers. Defining it as a factor of a fill rate
// is a convinient heuristics.
//
// Smaller values of factor ensure that a burst of breakpoints will not impact
// the service throughput. Longer values will allow the burst, and will only
// block high rate of condition checks over long period of time.
//
// For example if max_condition_cost is 0.01, the fill rate per CPU is going
// to be 10^7 nanoseconds per second. With this factor equal to 0.1, the
// capacity will be 10^6 nanoseconds, which is 1 ms. Therefore we will allow
// burst that consume 100% CPU for 1 ms, but no more.

constexpr double kConditionCostCapacityFactor = 0.1;
constexpr double kDynamicLogCapacityFactor = 2;  // allow short bursts.
constexpr double kDynamicLogBytesCapacityFactor = 2;  // allow very short burst.

void MovingAverage::Add(int64_t value) {
  absl::MutexLock lock(&mu_);

  if (window_.size() >= max_size_) {   // make room
    sum_ -= window_.front();
    window_.pop();
  }

  window_.push(value);
  sum_ += value;
}

int64_t MovingAverage::Average() const {
  absl::MutexLock lock(&mu_);

  if (window_.empty()) {
    return 0;
  }

  return sum_ / window_.size();
}

int MovingAverage::IsFilled() const {
  absl::MutexLock lock(&mu_);
  return window_.size() == max_size_;
}


void MovingAverage::Reset() {
  absl::MutexLock lock(&mu_);

  window_ = std::queue<int64_t>();
  sum_ = 0.;
}


// Gets the number of CPUs that this process can use.
static int GetCpuCount() {
  static int cpu_count_cache = -1;

  if (cpu_count_cache == -1) {
#ifdef __APPLE__
    // OS X does not export interfaces for thread placement, so there's no need
    // to check affinity.
    int cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
#else
    cpu_set_t cpus_allowed_mask;
    memset(&cpus_allowed_mask, 0, sizeof(cpus_allowed_mask));

    sched_getaffinity(0, sizeof(cpus_allowed_mask), &cpus_allowed_mask);

    int cpu_count = 0;
    for (int i = 0; i < CPU_SETSIZE; ++i) {
      if (CPU_ISSET(i, &cpus_allowed_mask)) {
        ++cpu_count;
      }
    }
#endif

    LOG(INFO) << "CPU count: " << cpu_count;

    // The cpu_count is the schedulable CPUs and does not reflect available
    // CPU capacity.  For example, you may have 100 cpus on which your threads
    // may be scheduled, but be limited to 1 cpu second/second.
    // Set the cpu_count to 1 to avoid overinflating the global limit.

    cpu_count = 1;
    LOG(INFO) << "Adjusted CPU count: " << cpu_count;

    // If CPU count is not available, assume single CPU.
    cpu_count_cache = std::max(1, cpu_count);
  }

  return cpu_count_cache;
}

static int64_t GetBaseFillRate(CostLimitType type) {
  switch (type) {
    case CostLimitType::BreakpointCondition:
      return absl::GetFlag(FLAGS_max_condition_cost) * 1000000000L;

    case CostLimitType::DynamicLog:
      return absl::GetFlag(FLAGS_max_dynamic_log_rate);

    case CostLimitType::DynamicLogBytes:
      return absl::GetFlag(FLAGS_max_dynamic_log_bytes_rate);
  }

  return 0;
}

static int64_t GetBaseCapacity(CostLimitType type) {
  switch (type) {
    case CostLimitType::BreakpointCondition:
      return GetBaseFillRate(type) * kConditionCostCapacityFactor;

    case CostLimitType::DynamicLog:
      return GetBaseFillRate(type) * kDynamicLogCapacityFactor;

    case CostLimitType::DynamicLogBytes:
      return GetBaseFillRate(type) * kDynamicLogBytesCapacityFactor;
  }

  return 0;
}

std::unique_ptr<LeakyBucket> CreateGlobalCostLimiter(CostLimitType type) {
  // Logs are I/O bound, not CPU bound.
  int cpu_factor =
      ((type == CostLimitType::BreakpointCondition) ? GetCpuCount() : 1);

  int64_t capacity = GetBaseCapacity(type) * cpu_factor;
  int64_t fill_rate = GetBaseFillRate(type) * cpu_factor;
  return std::unique_ptr<LeakyBucket>(new LeakyBucket(capacity, fill_rate));
}


std::unique_ptr<LeakyBucket> CreatePerBreakpointCostLimiter(
    CostLimitType type) {
  int64_t capacity = GetBaseCapacity(type);
  int64_t fill_rate = GetBaseFillRate(type) / 2;
  return std::unique_ptr<LeakyBucket>(new LeakyBucket(capacity, fill_rate));
}

}  // namespace cdbg
}  // namespace devtools
