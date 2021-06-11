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

#include "statistician.h"

#include <math.h>

#include <cstdint>

ABSL_FLAG(int32, cdbg_log_stats_time_micros,
          15 * 60 * 1000 * 1000,  // 15 minutes
          "How often to log debugger performance stats. "
          "Set to zero to never logs stats.");

namespace devtools {
namespace cdbg {

Statistician* statCaptureTime = nullptr;
Statistician* statDynamicLogTime = nullptr;
Statistician* statConditionEvaluationTime = nullptr;
Statistician* statFormattingTime = nullptr;
Statistician* statClassPrepareTime = nullptr;
Statistician* statBreakpointsUpdateTime = nullptr;
Statistician* statSafeClassSize = nullptr;
Statistician* statSafeClassTransformTime = nullptr;


void InitializeStatisticians() {
  statCaptureTime = new Statistician("capture_time_micros");
  statDynamicLogTime = new Statistician("dynamic_log_time_micros");
  statConditionEvaluationTime =
      new Statistician("condition_evaluation_time_micros");
  statFormattingTime = new Statistician("formatting_time_micros");
  statClassPrepareTime = new Statistician("class_prepare_time_micros");
  statBreakpointsUpdateTime =
      new Statistician("breakpoints_update_time_micros");
  statSafeClassSize = new Statistician("safe_class_size_bytes");
  statSafeClassTransformTime =
      new Statistician("safe_class_transform_time_micros");
}


void CleanupStatisticians() {
  delete statCaptureTime;
  statCaptureTime = nullptr;

  delete statDynamicLogTime;
  statDynamicLogTime = nullptr;

  delete statConditionEvaluationTime;
  statConditionEvaluationTime = nullptr;

  delete statFormattingTime;
  statFormattingTime = nullptr;

  delete statClassPrepareTime;
  statClassPrepareTime = nullptr;

  delete statBreakpointsUpdateTime;
  statBreakpointsUpdateTime = nullptr;

  delete statSafeClassSize;
  statSafeClassSize = nullptr;

  delete statSafeClassTransformTime;
  statSafeClassTransformTime = nullptr;
}


void Statistician::add(double sample) {
  bool log_statistics = false;

  {
    absl::MutexLock lock(&mu_);

    sum_ += sample;
    sum2_ += sample * sample;

    if (count_ > 0) {
      min_ = std::min(min_, sample);
      max_ = std::max(max_, sample);
    } else {
      min_ = sample;
      max_ = sample;
    }

    ++count_;

    const int32_t log_stats_time_micros =
        absl::GetFlag(FLAGS_cdbg_log_stats_time_micros);

    if (log_stats_time_micros > 0 &&
        report_timer_.GetElapsedMicros() > log_stats_time_micros) {
      log_statistics = true;
      report_timer_.Reset();
    }
  }

  if (log_statistics) {
    LOG(INFO)
        << "Statistics of " << name_ << ": "
           "mean = " << mean()
        << ", stdev = " << stdev()
        << ", min = " << min()
        << ", max = " << max()
        << ", samples = " << count();
  }
}


double Statistician::mean() const {
  absl::MutexLock lock(&mu_);

  if (count_ == 0) {
    return -1;
  }

  return sum_ / count_;
}

double Statistician::stdev() const {
  absl::MutexLock lock(&mu_);

  if (count_ == 0) {
    return -1;
  }

  const double mean_value = sum_ / count_;
  return sqrt((sum2_ / count_) - (mean_value * mean_value));
}

}  // namespace cdbg
}  // namespace devtools
