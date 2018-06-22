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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_STATISTICIAN_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_STATISTICIAN_H_

#include "common.h"
#include "mutex.h"
#include "stopwatch.h"

namespace devtools {
namespace cdbg {

// Computes statistics (like minimum, maximum and average) over a stream of
// samples.
// This class is thread safe.
class Statistician {
 public:
  explicit Statistician(const char* name) : name_(name) { }

  // Adds a new sample to the statistics.
  void add(double sample);

  // Gets the name of the collected statistics.
  const char* name() const { return name_; }

  // Gets the number of samples added.
  int count() const { return count_; }

  // Gets the minimal sample value encountered.
  double min() const { return min_; }

  // Gets the maximal sample value encountered.
  double max() const { return max_; }

  // Gets the mean value of all the samples encountered.
  double mean() const;

  // Gets the standard deviation of the samples.
  double stdev() const;

 private:
  // Synchronize updates.
  mutable absl::Mutex mu_;

  // Name of the collector for logging purposes.
  const char* name_;

  // Number of samples.
  int count_ { 0 };

  // Sum of all the sample values.
  double sum_ { 0 };

  // Sum of squares of values.
  double sum2_ { 0 };

  // Minimal sample value.
  double min_ { -1 };

  // Maximal sample value.
  double max_ { -1 };

  // Counts time since the collected statistics were last written to the log.
  Stopwatch report_timer_;

  DISALLOW_COPY_AND_ASSIGN(Statistician);
};


// Adds a new timer stat to the provided Statistician when ScopedStat
// goes out-of-scope.
class ScopedStat {
 public:
  // fn_gettime typically takes:
  //   Stopwatch::MonotonicClock - wall time
  //   Stopwatch::TheadClock - thread cpu time
  explicit ScopedStat(
      Statistician* stat,
      std::function<int(struct timespec*)> fn_gettime =
          Stopwatch::MonotonicClock) :
    stat_(stat),
    timer_(std::move(fn_gettime)) { }

  ~ScopedStat() {
    stat_->add(timer_.GetElapsedMicros());
  }

 private:
  Statistician* stat_;
  Stopwatch timer_;
};


// Global instances of all the metrics collected in the debuglet.
extern Statistician* statCaptureTime;
extern Statistician* statDynamicLogTime;
extern Statistician* statConditionEvaluationTime;
extern Statistician* statFormattingTime;
extern Statistician* statClassPrepareTime;
extern Statistician* statBreakpointsUpdateTime;
extern Statistician* statSafeClassSize;
extern Statistician* statSafeClassTransformTime;

// Initialize global statistician instances. This function is only expected to
// be called exactly once during initialization.
void InitializeStatisticians();

// Global statistician instances cleanup.
void CleanupStatisticians();

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_STATISTICIAN_H_
