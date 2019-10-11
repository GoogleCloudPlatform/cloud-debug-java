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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_BREAKPOINT_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_BREAKPOINT_H_

#include <memory>

#include "leaky_bucket.h"
#include "auto_jvmti_breakpoint.h"
#include "breakpoint.h"
#include "common.h"
#include "expression_util.h"
#include "jni_utils.h"
#include "mutex.h"
#include "rate_limit.h"
#include "scheduler.h"
#include "stopwatch.h"

namespace devtools {
namespace cdbg {

class BreakpointBuilder;
class BreakpointsManager;
class CaptureDataCollector;
class DynamicLogger;
class FormatQueue;
struct JvmEvaluators;
struct ResolvedSourceLocation;

// Immutable state of a compiled breakpoint.
//
// "CompiledBreakpoint" keeps a global reference to the Java class containing
// the location where the breakpoint is set. This ensures that JVM will not
// unload the method with the breakpoint. Unloading a method with breakpoint
// presents us with a number of challenges:
// * We need to listen for notification about the method getting loaded back
//   to re-enable breakpoint.
// * There is a possibility of various race conditions when a method is being
//   unloaded and a breakpoint is being set. Protecting against all those
//   is not trivial task.
// * There is no easy way to trigger compiled method unload in integration test.
class CompiledBreakpoint {
 public:
  CompiledBreakpoint(
      jclass cls,
      const jmethodID method,
      const jlocation location,
      CompiledExpression condition,
      std::vector<CompiledExpression> watches);

  ~CompiledBreakpoint();

  jmethodID method() const { return method_; }

  jlocation location() const { return location_; }

  const CompiledExpression& condition() const { return condition_; }

  const std::vector<CompiledExpression>& watches() const { return watches_; }

  // Checks whether "JvmBreakpoint" has any expressions that could not be
  // parsed or compiled.
  bool HasBadWatchedExpression() const;

 private:
  // Java class in which the breakpoint is set.
  JavaClass cls_;

  // Method containing the breakpoint source location.
  const jmethodID method_;

  // Location of a statement within the method.
  const jlocation location_;

  // Compiled breakpoint condition or null if the breakpoint is unconditional.
  CompiledExpression condition_;

  // List of watched expressions to evaluate upon breakpoint hit. Elements
  // corresponding to watched expressions that could not be compiled will be
  // nullptr. They are not skipped to maintain proper indexes (since indexes
  // are used to correlate the evaluated result with the entry in breakpoint
  // definition).
  std::vector<CompiledExpression> watches_;

  DISALLOW_COPY_AND_ASSIGN(CompiledBreakpoint);
};


class JvmBreakpoint : public Breakpoint,
                      public std::enable_shared_from_this<JvmBreakpoint> {
 public:
  // All arguments except of "breakpoint_definition" are not owned by this
  // class and must outlive it.
  JvmBreakpoint(
      Scheduler<>* scheduler,
      JvmEvaluators* evaluators,
      FormatQueue* format_queue,
      DynamicLogger* dynamic_logger,
      BreakpointsManager* breakpoints_manager,
      std::unique_ptr<StatusMessageModel> setup_error,
      std::unique_ptr<BreakpointModel> breakpoint_definition);

  ~JvmBreakpoint() override;

  const std::string& id() const override { return definition_->id; }

  void Initialize() override;

  void ResetToPending() override;

  void OnClassPrepared(const std::string& type_name,
                       const std::string& class_signature) override;

  void OnJvmBreakpointHit(
      jthread thread,
      jmethodID method,
      jlocation location) override;

  void CompleteBreakpointWithStatus(
      std::unique_ptr<StatusMessageModel> status) override;

 private:
  // Checks whether the resolve location points to a different source line
  // than the one specified in the breakpoint. This will happen when a
  // breakpoint is not set to the first line of a multi-line statement.
  bool IsSourceLineAdjusted() const;

  // Resolves pending breakpoint definition. This involves:
  // 1. Mapping source location defined as a file name and source line number
  //    pair to {jmethodID, jlocation} tuple.
  // 2. Compiling breakpoint condition and watched expressions.
  // If the breakpoint is not valid, the function sends final breakpoint update.
  // If the code hasn't been loaded yet, the breakpoint stays pending.
  void TryActivatePendingBreakpoint();

  // Parses and compiles breakpoint expressions (if any) within the context
  // of a breakpoint location. The result is returned as "CompiledBreakpoint".
  std::shared_ptr<CompiledBreakpoint> CompileBreakpointExpressions(
      jclass cls,
      jmethodID method,
      jlocation location) const;

  // Compiles breakpoint condition (if the breakpoint has condition at all) and
  // verifies the proper return type. Returns "CompiledExpression" with error
  // message in case of error.
  CompiledExpression CompileCondition(ReadersFactory* readers_factory) const;

  // Evaluates the breakpoint condition. Returns true if breakpoint condition
  // matched. Completes breakpoint if the conditional expression turns out
  // to be mutable.
  bool EvaluateCondition(
      const CompiledExpression& condition,
      jthread thread);

  // Subtracts the condition evaluation time from the quota and completes the
  // breakpoint if limit was reached. "condition_cost_ns_" stores the last
  // condition evaluation duration (smoothed with sliding average filter).
  void ApplyConditionQuota();

  // Charges one log collection against quota.  Only call this if a collection
  // will occur, otherwise the cost will be artifically high.  Returns false
  // if there is insufficient call quota to collect log data.
  bool ApplyDynamicLogsCallQuota(const ResolvedSourceLocation& source_location);

  // Charges bytes collected against quota.  Returns false if there is
  // insufficient byte quota to log the request.
  bool ApplyDynamicLogsByteQuota(const ResolvedSourceLocation& source_location,
                                 int log_bytes);
  // Captures the application state for data capturing breakpoints on
  // breakpoint hit.
  void DoCaptureAction(jthread thread, CompiledBreakpoint* state);

  // Issues a dynamic log on breakpoint hit.
  void DoLogAction(jthread thread, CompiledBreakpoint* state);

  // Sends a final breakpoint update and completes the breakpoint.
  void CompleteBreakpoint(
      BreakpointBuilder* builder,
      std::unique_ptr<CaptureDataCollector> collector);

  // Sends interim breakpoint update to indicate that some watched expressions
  // could not be parsed or compiled.
  void SendInterimBreakpointUpdate(const CompiledBreakpoint& state);

  // Callback invoked "breakpoint_expiration_sec" seconds after the breakpoint
  // has been created. This callback signals that the breakpoint has expired.
  void OnBreakpointExpired();
 private:
  // Schedules callbacks at a future time. Used to expire breakpoints.
  // Not owned by this class.
  Scheduler<>* scheduler_;

  // Bundles all the evaluation classes together. Evaluators are guaranteed
  // to be valid throughout the lifetime of "JvmBreakpoint". See "Cleaner"
  // class for details on how the lifetime is extended to after all the
  // callbacks end.
  // Not owned by this class.
  JvmEvaluators* const evaluators_;

  // Breakpoint hit results that wait to be reported to the hub.
  // Not owned by this class.
  FormatQueue* const format_queue_;

  // Application logger to inject dynamically generated log statements.
  // Not owned by this class.
  DynamicLogger* const dynamic_logger_;

  // Multiplexer of JVMTI breakpoints.
  // Not owned by this class.
  BreakpointsManager* const breakpoints_manager_;

  // If not-null, this breakpoint will be immediately completed on Initialize()
  // with the given status.
  std::unique_ptr<StatusMessageModel> setup_error_;

  // Breakpoint definition (with no hit results). Since "definition_" doesn't
  // keep the hit results, it is very small and copying it is not a big deal.
  std::unique_ptr<const BreakpointModel> definition_;

  // Manages calls to "SetJvmtiBreakpoint" and "ClearJvmtiBreakpoint"
  AutoJvmtiBreakpoint jvmti_breakpoint_;

  // Cancellation token for scheduled expiration callback.
  Scheduler<>::Id scheduler_id_ { Scheduler<>::NullId };

  // Average cost of evaluating condition in this breakpoint. The averaging
  // eliminates spikes due to garbage collector.
  MovingAverage condition_cost_ns_;

  // Per breakpoint limit of the cost of condition checks.
  std::unique_ptr<LeakyBucket> breakpoint_condition_cost_limiter_;

  // Per breakpoint limit of dynamic logs. Only initialized for dynamic log
  // breakpoints.
  std::unique_ptr<LeakyBucket> breakpoint_dynamic_log_limiter_;

  // Per breakpoint limit of dynamic log bytes. Only initialized for dynamic log
  // breakpoints.
  std::unique_ptr<LeakyBucket> breakpoint_dynamic_log_bytes_limiter_;

  // Manages the pause in logger when quota is exceeded.
  class DynamicLogPause {
   public:
    // Called when quota has been exceeded.  Updates is_paused_ and logs a
    // message.  The message parameter specifies the out-of-quota message to
    // log.
    void OutOfQuota(DynamicLogger* logger, BreakpointModel::LogLevel log_level,
                    const std::string& message,
                    const ResolvedSourceLocation& source_location);

    // Returns true if logging is paused.
    bool IsPaused();

   private:
    // Locks access to members of this struct.
    mutable absl::Mutex mu_;

    // Indicates whether log collection should be paused due to quota
    // restrictions. This flag is used to log a warning that some log entries
    // are omitted when we run out of quota. Obviously we don't want to log
    // this warning every time we omit a log entry.
    bool is_paused_ { false };

    // Time at which the dynamic log was disabled due to quota. Used to enforce
    // the cool down period.
    Stopwatch cooldown_stopwatch_;
  };

  DynamicLogPause dynamic_log_pause_;

  //
  // Breakpoint state machine:
  //   1. Uninitialized: (resolved_location_ == nullptr)
  //   2. Pending: (resolved_location_ != nullptr) &&
  //               (compiled_breakpoint_ is == nullptr)
  //   3. Active: (compiled_breakpoint_ is != nullptr)
  //

  // Breakpoint location mapped to the specific statement in Java code. The
  // method containing this statement may or may not be loaded by JVM.
  std::shared_ptr<ResolvedSourceLocation> resolved_location_;

  // Conditions or expressions can also put a breakpoints into pending state if
  // the class they are called on is not loaded.
  std::string class_dependency_signature_;

  // Immutable state of active breakpoint. The code should assume that this
  // variable can change any time (point to a new instance of immutable
  // "CompiledBreakpoint").
  std::shared_ptr<CompiledBreakpoint> compiled_breakpoint_;

 private:
  DISALLOW_COPY_AND_ASSIGN(JvmBreakpoint);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_BREAKPOINT_H_
