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

#include "jvm_breakpoint.h"

#include <cstdint>

#include "breakpoints_manager.h"
#include "capture_data_collector.h"
#include "class_indexer.h"
#include "class_path_lookup.h"
#include "dynamic_logger.h"
#include "expression_evaluator.h"
#include "expression_util.h"
#include "format_queue.h"
#include "jvm_evaluators.h"
#include "jvm_readers_factory.h"
#include "jvmti_buffer.h"
#include "log_data_collector.h"
#include "messages.h"
#include "model.h"
#include "model_util.h"
#include "resolved_source_location.h"
#include "statistician.h"
#include "type_util.h"

ABSL_FLAG(int32, breakpoint_expiration_sec,
          60 * 60 * 24,  // 24 hours
          "breakpoint expiration time in seconds");

ABSL_FLAG(int32, dynamic_log_quota_recovery_ms,
          500,  // ms
          "time to pause dynamic logs after it runs out of quota");

namespace devtools {
namespace cdbg {

constexpr char kLogpointPrefix[] = "LOGPOINT: ";

// Resolves method line in a loaded and prepared Java class.
static bool FindMethodLine(jclass cls, const std::string& method_name,
                           const std::string& method_signature, int line_number,
                           jmethodID* method, jlocation* location) {
  *method = nullptr;
  *location = 0;

  jvmtiError err = JVMTI_ERROR_NONE;

  // Iterate over all methods in the class.
  jint methods_count = 0;
  JvmtiBuffer<jmethodID> methods_buf;
  err = jvmti()->GetClassMethods(cls, &methods_count, methods_buf.ref());
  if (err != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "GetClassMethods failed, error: " << err;
    return false;
  }

  int matched_name_count = 0;
  for (int method_index = 0; method_index < methods_count; ++method_index) {
    jmethodID cur_method = methods_buf.get()[method_index];

    // Ignore the method unless it's the one we are looking for.
    JvmtiBuffer<char> name_buf;
    JvmtiBuffer<char> sig_buf;
    err = jvmti()->GetMethodName(cur_method, name_buf.ref(), sig_buf.ref(),
                                 nullptr);
    if (err != JVMTI_ERROR_NONE) {
      LOG(ERROR) << "GetMethodName failed, error: " << err << ", ignoring...";
      continue;
    }

    if ((name_buf.get() == nullptr) || (method_name != name_buf.get())) {
      continue;
    }

    if ((sig_buf.get() == nullptr) || (method_signature != sig_buf.get())) {
      continue;
    }

    // Get the line numbers corresponding to the code statements of the method.
    jint line_entries_count = 0;
    JvmtiBuffer<jvmtiLineNumberEntry> line_entries;
    err = jvmti()->GetLineNumberTable(cur_method, &line_entries_count,
                                      line_entries.ref());

    if (err == JVMTI_ERROR_ABSENT_INFORMATION) {
      LOG(ERROR) << "Class doesn't have line number debugging information";
      return false;
    }

    if (err != JVMTI_ERROR_NONE) {
      LOG(ERROR) << "GetLineNumberTable failed, error: " << err;
      return false;
    }

    // Match the line. The "line_number" parameter is by now adjusted to
    // the start location of a statement.
    for (int line_index = 0; line_index < line_entries_count; ++line_index) {
      const jvmtiLineNumberEntry& line_entry = line_entries.get()[line_index];

      if (line_entry.line_number == line_number) {
        *method = cur_method;
        *location = line_entry.start_location;

        LOG(INFO) << "Line " << line_number << " in method " << method_name
                  << " resolved to method ID: " << *method
                  << ", location: " << *location;

        return true;
      }
    }

    // We may still find an overloaded method with the matching line.
    ++matched_name_count;
  }

  if (matched_name_count > 0) {
    LOG(ERROR) << "No statement at line " << line_number << " found in method "
               << method_name << " (" << matched_name_count
               << " methods matched)";
  } else {
    LOG(ERROR) << "Method " << method_name << " not found in the class";
  }

  return false;
}

// Check if a compiled expression has an error due to ClassNotLoaded on an
// available class signature.
static inline bool IsClassNotLoadedError(const CompiledExpression& expression) {
  return !expression.expression.empty() && expression.evaluator == nullptr &&
         expression.error_message.format == ClassNotLoaded &&
         expression.error_message.parameters.size() > 1 &&
         expression.error_message.parameters[1] != kJavaSignatureNotAvailable;
}

CompiledBreakpoint::CompiledBreakpoint(jclass cls, const jmethodID method,
                                       const jlocation location,
                                       CompiledExpression condition,
                                       std::vector<CompiledExpression> watches)
    : method_(method),
      location_(location),
      condition_(std::move(condition)),
      watches_(std::move(watches)) {
  cls_.Assign(cls);
}

CompiledBreakpoint::~CompiledBreakpoint() {}

bool CompiledBreakpoint::HasBadWatchedExpression() const {
  for (const auto& watch : watches_) {
    if (watch.evaluator == nullptr) {
      return true;
    }
  }

  return false;
}

JvmBreakpoint::JvmBreakpoint(
    Scheduler<>* scheduler, JvmEvaluators* evaluators,
    FormatQueue* format_queue, DynamicLogger* dynamic_logger,
    BreakpointsManager* breakpoints_manager,
    std::unique_ptr<StatusMessageModel> setup_error,
    std::unique_ptr<BreakpointModel> breakpoint_definition)
    : scheduler_(scheduler),
      evaluators_(evaluators),
      format_queue_(format_queue),
      dynamic_logger_(dynamic_logger),
      breakpoints_manager_(breakpoints_manager),
      setup_error_(std::move(setup_error)),
      definition_(std::move(breakpoint_definition)),
      jvmti_breakpoint_(breakpoints_manager) {
  breakpoint_condition_cost_limiter_ =
      CreatePerBreakpointCostLimiter(CostLimitType::BreakpointCondition);

  if (definition_->action == BreakpointModel::Action::LOG) {
    breakpoint_dynamic_log_limiter_ =
        CreatePerBreakpointCostLimiter(CostLimitType::DynamicLog);

    breakpoint_dynamic_log_bytes_limiter_ =
        CreatePerBreakpointCostLimiter(CostLimitType::DynamicLogBytes);
  }
}

JvmBreakpoint::~JvmBreakpoint() { scheduler_->Cancel(scheduler_id_); }

void JvmBreakpoint::Initialize() {
  DCHECK(resolved_location_ == nullptr);
  DCHECK(compiled_breakpoint_ == nullptr);
  DCHECK(scheduler_id_ == Scheduler<>::NullId);

  // Schedule breakpoint cancellation.
  time_t expiration_time_base;
  if (definition_->create_time == kUnspecifiedTimestamp) {
    // It really shouldn't happen, but if it does start computing expiration
    // time from this moment.
    expiration_time_base = scheduler_->CurrentTime();
  } else {
    expiration_time_base = definition_->create_time.seconds;
  }

  int32_t expiration_seconds = absl::GetFlag(FLAGS_breakpoint_expiration_sec);
  if (definition_->expires_in != nullptr) {
    // Truncate if per-breakpoint expiration exceeds the agent maximum limit.
    // Ignore the nanos field, we don't need that precision.
    expiration_seconds =
        std::min<int64_t>(definition_->expires_in->seconds, expiration_seconds);
  }

  scheduler_id_ =
      scheduler_->Schedule(expiration_time_base + expiration_seconds,
                           std::weak_ptr<JvmBreakpoint>(shared_from_this()),
                           &JvmBreakpoint::OnBreakpointExpired);

  // If a preemtive status exists, immediately complete the breakpoint with
  // this status.
  if (setup_error_ != nullptr) {
    DCHECK(setup_error_->is_error);
    CompleteBreakpointWithStatus(std::move(setup_error_));
    return;
  }

  std::shared_ptr<ResolvedSourceLocation> rsl(new ResolvedSourceLocation);

  // Find the statement in Java code corresponding to breakpoint location.
  // "mu_data_" must not be locked during call to "ResolveSourceLocation".
  // Otherwise a deadlock is guaranteed.
  if (definition_->location == nullptr) {
    LOG(ERROR) << "\"location\" field not set in breakpoint definition message";
    rsl->error_message = INTERNAL_ERROR_MESSAGE;
  } else {
    evaluators_->class_path_lookup->ResolveSourceLocation(
        definition_->location->path, definition_->location->line, rsl.get());
  }

  if (!rsl->error_message.format.empty()) {
    // The breakpoint location could not be resolved, send final breakpoint
    // update and complete the breakpoint.
    CompleteBreakpointWithStatus(
        StatusMessageBuilder()
            .set_error()
            .set_refers_to(
                StatusMessageModel::Context::BREAKPOINT_SOURCE_LOCATION)
            .set_description(rsl->error_message)
            .build());
    return;
  }

  LOG(INFO) << "Breakpoint " << id()
            << " initialized to pending state, path: "
            << definition_->location->path
            << ", line number: " << definition_->location->line
            << ", resolved class signature: " << rsl->class_signature
            << ", resolved method name: " << rsl->method_name
            << ", adjusted line number: " << rsl->adjusted_line_number;

  resolved_location_ = rsl;

  // At this point we know whether the source line was adjusted or not. If it
  // was, we could send an interim breakpoint update saying that the line has
  // moved. This way the user will see this clue right away even if the
  // underlying Java class hasn't been loaded and the breakpoint is pending.
  // We are not sending an interim update here because multiple debuglets of
  // the same debuggee might be sending multiple updates for line adjustment
  // and watched expressions update simultaneously. Without proper handling on
  // the backend, this can cause flicking of watched expressions warning.
  // Instead we only send interim updates from one place (after watched
  // expressions have been resolved).

  // Compile the breakpoint from pending state to active state. It might fail
  // and the breakpoint will stay pending until better times.
  TryActivatePendingBreakpoint();
}

void JvmBreakpoint::ResetToPending() {
  // We assume here that a class will not be reloaded while a method
  // is being unloaded. If that happens, we may ignore "OnClassLoaded" event.

  jvmti_breakpoint_.Clear(shared_from_this());
  compiled_breakpoint_ = nullptr;
}

void JvmBreakpoint::OnClassPrepared(const std::string& type_name,
                                    const std::string& class_signature) {
  if (compiled_breakpoint_ != nullptr) {
    return;  // The breakpoint is already active.
  }

  std::shared_ptr<ResolvedSourceLocation> location = resolved_location_;
  if (location == nullptr) {
    return;  // The breakpoint is still uninitialized
  }

  if (location->class_signature == class_signature ||
      class_dependency_signature_ == class_signature) {
    LOG(INFO) << "Class " << type_name << " loaded (" << class_signature
              << "), trying to activate pending breakpoint " << id();

    TryActivatePendingBreakpoint();
  }
}

void JvmBreakpoint::OnJvmBreakpointHit(jthread thread, jmethodID method,
                                       jlocation location) {
  Stopwatch stopwatch(Stopwatch::ThreadClock);

  std::shared_ptr<CompiledBreakpoint> state = compiled_breakpoint_;
  if (state == nullptr) {
    // The breakpoint is already pending. This is possible if some other thread
    // just completed this breakpoint (while the callback was being routed).
    LOG(INFO) << "Breakpoint " << id()
              << " is in pending state, ignoring breakpoint hit";
    return;
  }

  DCHECK((method == state->method()) && (location == state->location()));

  // Evaluate breakpoint condition (if defined).
  if (state->condition().evaluator != nullptr) {
    bool condition_result = EvaluateCondition(state->condition(), thread);
    int64_t current_condition_nanos = stopwatch.GetElapsedNanos();
    condition_cost_ns_.Add(current_condition_nanos);

    statConditionEvaluationTime->add(current_condition_nanos / 1000);

    if (!condition_result) {
      // Skip quota if breakpoint got completed.
      if (compiled_breakpoint_ != nullptr) {
        ApplyConditionQuota();
      }

      return;
    }
  }

  switch (definition_->action) {
    case BreakpointModel::Action::CAPTURE: {
      DoCaptureAction(thread, state.get());

      statCaptureTime->add(stopwatch.GetElapsedMicros());
      break;
    }

    case BreakpointModel::Action::LOG: {
      DoLogAction(thread, state.get());

      statDynamicLogTime->add(stopwatch.GetElapsedMicros());
      break;
    }
  }
}

void JvmBreakpoint::DoCaptureAction(jthread thread, CompiledBreakpoint* state) {
  // It will now take a few milliseconds to capture all the data. Then the
  // breakpoint will be done. We don't want other threads to waste their time
  // on this breakpoint while capturing data, so we clear it here.
  breakpoints_manager_->CompleteBreakpoint(id());

  // Capture the data at a breakpoint hit and prepare it for formatting. The
  // formatting will happen in a worker thread at a later time.
  std::unique_ptr<CaptureDataCollector> collector(
      new CaptureDataCollector(evaluators_));
  collector->Collect(state->watches(), thread);

  // Enqueue the breakpoint result and deactivate the breakpoint.
  BreakpointBuilder builder(*definition_);
  CompleteBreakpoint(&builder, std::move(collector));
}

void JvmBreakpoint::DoLogAction(jthread thread, CompiledBreakpoint* state) {
  if (!dynamic_logger_->IsAvailable()) {
    CompleteBreakpointWithStatus(StatusMessageBuilder()
                                     .set_error()
                                     .set_format(DynamicLoggerNotAvailable)
                                     .build());

    return;
  }

  if (dynamic_log_pause_.IsPaused()) {
    return;
  }

  std::shared_ptr<ResolvedSourceLocation> rsl = resolved_location_;
  if (rsl == nullptr) {
    // The breakpoint is being deactivated.
    LOG(WARNING) << "Source location is not available";
    return;
  }

  // Note: It is important to apply quota before doing any potentially-expensive
  // work.
  if (!ApplyDynamicLogsCallQuota(*rsl)) {
    return;
  }

  std::unique_ptr<MethodCaller> method_caller =
      evaluators_->method_caller_factory(Config::DYNAMIC_LOG);

  LogDataCollector collector;

  collector.Collect(method_caller.get(), evaluators_->object_evaluator,
                    state->watches(), thread);

  std::string log_message =
      std::string(kLogpointPrefix) + collector.Format(*definition_);

  // TODO: Discuss if Log() below should be included in quota
  // accounting.  If so, this function can be changed to void.
  if (!ApplyDynamicLogsByteQuota(*rsl, log_message.length())) {
    return;
  }

  dynamic_logger_->Log(definition_->log_level, *rsl, log_message);
}

bool JvmBreakpoint::EvaluateCondition(const CompiledExpression& condition,
                                      jthread thread) {
  std::unique_ptr<MethodCaller> method_caller =
      evaluators_->method_caller_factory(Config::EXPRESSION_EVALUATION);

  EvaluationContext evaluation_context;
  evaluation_context.frame_depth = 0;  // Topmost call frame.
  evaluation_context.thread = thread;
  evaluation_context.method_caller = method_caller.get();

  ErrorOr<JVariant> condition_result =
      condition.evaluator->Evaluate(evaluation_context);
  if (condition_result.is_error()) {
    LOG(WARNING) << "Evaluation of breakpoint condition failed, breakpoint ID: "
                 << id() << ", evaluation error message: "
                 << condition_result.error_message();

    CompleteBreakpointWithStatus(
        StatusMessageBuilder()
            .set_error()
            .set_refers_to(StatusMessageModel::Context::BREAKPOINT_CONDITION)
            .set_description(condition_result.error_message())
            .build());

    return false;
  }

  jboolean condition_result_value = false;
  if (!condition_result.value().get<jboolean>(&condition_result_value)) {
    LOG(WARNING) << "Breakpoint condition result is not boolean, "
                    "breakpoint ID = "
                 << id();
    return false;
  }

  return condition_result_value;
}

void JvmBreakpoint::ApplyConditionQuota() {
  // Only start to apply the cost limit after we evaluated the condition a few
  // times. Otherwise if garbage collection kicks in the first time we evaluate
  // the condition, the cost limit will disable a totally innocent breakpoint.
  // Since the breakpoint will need to hit thousands of time to exceed the
  // quota, ignoring the first 32 samples is not a big deal.
  if (!condition_cost_ns_.IsFilled()) {
    return;
  }

  const int64_t tokens = condition_cost_ns_.Average();

  // Apply per-breakpoint cost limit.
  if (!breakpoint_condition_cost_limiter_->RequestTokens(tokens)) {
    LOG(WARNING) << "Cost of condition evaluations exceeded per-breakpoint "
                    "limit, breakpoint ID: "
                 << id();

    CompleteBreakpointWithStatus(
        StatusMessageBuilder()
            .set_error()
            .set_refers_to(StatusMessageModel::Context::BREAKPOINT_CONDITION)
            .set_format(ConditionEvaluationCostExceededPerBreakpointLimit)
            .build());
    return;
  }

  // Apply global cost limit.
  auto* global_condition_cost_limiter =
      breakpoints_manager_->GetGlobalConditionCostLimiter();
  if (!global_condition_cost_limiter->RequestTokens(tokens)) {
    LOG(WARNING) << "Cost of condition evaluations exceeded global "
                    "limit, breakpoint ID: "
                 << id();

    CompleteBreakpointWithStatus(
        StatusMessageBuilder()
            .set_error()
            .set_refers_to(StatusMessageModel::Context::BREAKPOINT_CONDITION)
            .set_format(ConditionEvaluationCostExceededGlobalLimit)
            .build());
    return;
  }
}

bool JvmBreakpoint::DynamicLogPause::IsPaused() {
  absl::MutexLock lock(&mu_);
  if (!is_paused_) {
    return false;
  }

  if (cooldown_stopwatch_.GetElapsedMillis() >
      absl::GetFlag(FLAGS_dynamic_log_quota_recovery_ms)) {
    is_paused_ = false;
    return false;
  }

  return true;
}

void JvmBreakpoint::DynamicLogPause::OutOfQuota(
    DynamicLogger* logger, BreakpointModel::LogLevel log_level,
    const std::string& message, const ResolvedSourceLocation& source_location) {
  // We are out of quota. Log warning when we transition from "normal" state
  // to "out of quota" state. Stick to the "out of quota" state for some time.
  bool log_out_of_quota_message = false;
  {
    absl::MutexLock lock(&mu_);
    if (!is_paused_) {
      cooldown_stopwatch_.Reset();
      is_paused_ = true;
      log_out_of_quota_message = true;
    }
  }

  if (log_out_of_quota_message) {
    logger->Log(log_level, source_location,
                std::string(kLogpointPrefix) + message);
  }
}

bool JvmBreakpoint::ApplyDynamicLogsCallQuota(
    const ResolvedSourceLocation& source_location) {
  LeakyBucket* global_dynamic_log_limiter =
      breakpoints_manager_->GetGlobalDynamicLogLimiter();

  // Note logic below should be || so that both quotas are charged, even
  // if the first bucket is empty
  if (!breakpoint_dynamic_log_limiter_->RequestTokens(1) ||
      !global_dynamic_log_limiter->RequestTokens(1)) {
    dynamic_log_pause_.OutOfQuota(dynamic_logger_, definition_->log_level,
                                  DynamicLogOutOfCallQuota, source_location);
    return false;
  }

  return true;
}

bool JvmBreakpoint::ApplyDynamicLogsByteQuota(
    const ResolvedSourceLocation& source_location, int log_bytes) {
  LeakyBucket* global_dynamic_log_bytes_limiter =
      breakpoints_manager_->GetGlobalDynamicLogBytesLimiter();

  // Note logic below should be || so that both quotas are charged, even
  // if the first bucket is empty
  if (!breakpoint_dynamic_log_bytes_limiter_->RequestTokens(log_bytes) ||
      !global_dynamic_log_bytes_limiter->RequestTokens(log_bytes)) {
    dynamic_log_pause_.OutOfQuota(dynamic_logger_, definition_->log_level,
                                  DynamicLogOutOfBytesQuota, source_location);
    return false;
  }

  return true;
}

bool JvmBreakpoint::IsSourceLineAdjusted() const {
  std::shared_ptr<ResolvedSourceLocation> location = resolved_location_;
  if (location == nullptr) {
    return false;  // The breakpoint is still uninitialized
  }

  return definition_->location->line != location->adjusted_line_number;
}

void JvmBreakpoint::TryActivatePendingBreakpoint() {
  if (compiled_breakpoint_ != nullptr) {
    return;  // The breakpoint is already active.
  }

  std::shared_ptr<ResolvedSourceLocation> rsl = resolved_location_;
  if (rsl == nullptr) {
    return;  // The breakpoint is still uninitialized
  }

  // Find the class in which we are going to set the breakpoint. It is
  // possible that the class still hasn't been loaded. In this case the
  // breakpoint will remain pending.
  JniLocalRef cls_local_ref =
      evaluators_->class_indexer->FindClassBySignature(rsl->class_signature);
  if (cls_local_ref == nullptr) {
    LOG(INFO) << "Class signature is valid, but class is not loaded yet, "
                 "leaving it as pending, breakpoint ID: "
              << id() << ", path: " << definition_->location->path
              << ", line: " << definition_->location->line;

    return;
  }

  // We are now holding reference to Java class. This guarantees that at least
  // until this function exits, the Java method will not get unloaded.

  // At this point we have the Java class object and we know the method and the
  // line number to set the breakpoint.
  jmethodID method = nullptr;
  jlocation location = 0;
  if (!FindMethodLine(static_cast<jclass>(cls_local_ref.get()),
                      rsl->method_name, rsl->method_signature,
                      rsl->adjusted_line_number, &method, &location)) {
    // This should not normally happen. If we hit this condition, it means
    // some disagreement between "ClassPathLookup.resolveSourceLocation" that
    // told us that "resolved_location_" is a valid source location, but
    // "FindMethodLine" could not find it.
    LOG(ERROR) << "Resolved source location not found"
                  ", class signature: "
               << rsl->class_signature << ", method: " << rsl->method_name
               << ", adjusted line: " << rsl->adjusted_line_number;

    CompleteBreakpointWithStatus(
        StatusMessageBuilder()
            .set_error()
            .set_refers_to(
                StatusMessageModel::Context::BREAKPOINT_SOURCE_LOCATION)
            .set_description(INTERNAL_ERROR_MESSAGE)
            .build());

    return;
  }

  std::shared_ptr<CompiledBreakpoint> new_state = CompileBreakpointExpressions(
      static_cast<jclass>(cls_local_ref.get()), method, location);

  // The only fatal failure of "CompileBreakpointExpression" is compilation
  // of breakpoint condition. The rest of it does not fail the breakpoint.
  // Even in this case, we leave the breakpoint in pending state if
  // the reason for condition compilation failure is a class not being loaded.
  // we record this dependency and listen for OnClassPrepare event in case it
  // is loaded later.
  if (!definition_->condition.empty() &&
      IsClassNotLoadedError(new_state->condition())) {
    LOG(WARNING) << "Failed to set breakpoint " << id()
                 << " because breakpoint condition uses class "
                 << new_state->condition().error_message.parameters[0]
                 << " which has not been loaded yet. Leaving breakpoint"
                    " in pending state.";

    // The not-loaded class signature is the second parameter
    // in 'error_message'
    class_dependency_signature_ =
        new_state->condition().error_message.parameters[1];

    return;
  }

  // Fail the breakpoint if the condition has compilation error other than
  // ClassNotLoaded.
  if (!definition_->condition.empty() &&
      new_state->condition().evaluator == nullptr) {
    LOG(WARNING) << "Failed to set breakpoint " << id()
                 << " because breakpoint condition could not be compiled";

    CompleteBreakpointWithStatus(
        StatusMessageBuilder()
            .set_error()
            .set_refers_to(StatusMessageModel::Context::BREAKPOINT_CONDITION)
            .set_description(new_state->condition().error_message)
            .build());

    return;
  }

  // For each expression, apply the ClassNotLoaded check similar to above that
  // if there is any expression compilation failure due to ClassNotLoaded, the
  // breakpoint would subscribe to the corresponding class and remain pending.
  // However, if there is any failure due to non-ClassNotLoaded reason, the
  // breakpoint continues to proceed so that the expression(s) other than the
  // failure one(s) will still be evaluated.
  for (const auto& expression : new_state->watches()) {
    if (IsClassNotLoadedError(expression)) {
      LOG(WARNING) << "Failed to set breakpoint " << id()
                   << " because breakpoint expression uses class "
                   << expression.error_message.parameters[0]
                   << " which has not been loaded yet. Leaving breakpoint"
                      " in pending state.";
      class_dependency_signature_ = expression.error_message.parameters[1];

      return;
    }
  }

  const bool is_source_line_adjusted = IsSourceLineAdjusted();

  if (is_source_line_adjusted) {
    LOG(INFO) << "Breakpoint " << id() << " location adjusted from line "
              << definition_->location->line << " to line "
              << rsl->adjusted_line_number;

    // This is the only case when we change the supposedly immutable
    // "definition_". Since we are only changing integer, it will not
    // cause any race conditions.
    // TODO: make "definition_" truly immutable and change line
    // number when enqueuing breakpoint update.
    definition_->location->line = rsl->adjusted_line_number;
  }

  // Send interim update if some watched expressions could not be compiled or
  // the breakpoint was set on different line than the one requested.
  if (is_source_line_adjusted || new_state->HasBadWatchedExpression()) {
    SendInterimBreakpointUpdate(*new_state);
  }

  LOG(INFO) << "Activating breakpoint " << id()
            << ", path: " << definition_->location->path
            << ", line: " << definition_->location->line;

  // Set the actual JVMTI breakpoints.
  if (!jvmti_breakpoint_.Set(new_state->method(), new_state->location(),
                             shared_from_this())) {
    LOG(ERROR) << "Failed to set JVMTI breakpoint " << id()
               << ", method = " << new_state->method()
               << ", location = " << std::hex << std::showbase << location;

    BreakpointBuilder builder(*definition_);
    builder.set_status(StatusMessageBuilder().set_error().set_description(
        INTERNAL_ERROR_MESSAGE));

    CompleteBreakpoint(&builder, nullptr);

    return;
  }

  compiled_breakpoint_ = new_state;
}

std::shared_ptr<CompiledBreakpoint> JvmBreakpoint::CompileBreakpointExpressions(
    jclass cls, jmethodID method, jlocation location) const {
  JvmReadersFactory readers_factory(evaluators_, method, location);

  // Compile breakpoint condition (if present).
  CompiledExpression condition = CompileCondition(&readers_factory);

  // Compile watched expressions.
  std::vector<CompiledExpression> watches;
  for (const std::string& watch : definition_->expressions) {
    watches.push_back(CompileExpression(watch, &readers_factory));
  }

  return std::make_shared<CompiledBreakpoint>(
      cls, method, location, std::move(condition), std::move(watches));
}

CompiledExpression JvmBreakpoint::CompileCondition(
    ReadersFactory* readers_factory) const {
  if (definition_->condition.empty()) {
    return CompiledExpression();
  }

  CompiledExpression condition =
      CompileExpression(definition_->condition, readers_factory);

  if (condition.evaluator == nullptr) {
    LOG(WARNING) << "Breakpoint condition could not be compiled, "
                    "condition: "
                 << definition_->condition
                 << ", error message: " << condition.error_message;
    return condition;
  }

  const JType return_type = condition.evaluator->GetStaticType().type;
  if (return_type != JType::Boolean) {
    LOG(WARNING) << "Breakpoint condition does not evaluate to boolean, "
                    "return type: "
                 << static_cast<int>(return_type);

    CompiledExpression result;
    result.evaluator = nullptr;
    result.error_message = {
        ConditionNotBoolean,  // error_message.format
        {                     // error_message.parameters
         TypeNameFromSignature(condition.evaluator->GetStaticType())}};
    result.expression = definition_->condition;

    return result;
  }

  return condition;
}

void JvmBreakpoint::CompleteBreakpointWithStatus(
    std::unique_ptr<StatusMessageModel> status) {
  BreakpointBuilder builder(*definition_);
  builder.set_status(std::move(status));

  CompleteBreakpoint(&builder, nullptr);
}

void JvmBreakpoint::CompleteBreakpoint(
    BreakpointBuilder* builder,
    std::unique_ptr<CaptureDataCollector> collector) {
  builder->set_is_final_state(true);
  std::unique_ptr<BreakpointModel> model = builder->build();
  format_queue_->Enqueue(std::move(model), std::move(collector));

  breakpoints_manager_->CompleteBreakpoint(id());

  ResetToPending();
}

void JvmBreakpoint::SendInterimBreakpointUpdate(
    const CompiledBreakpoint& state) {
  // Prepare breakpoint update.
  BreakpointBuilder breakpoint_builder(*definition_);

  // If we already tried to compiled watched expressions, we want
  // to include any error messages that this process produced.

  breakpoint_builder.clear_evaluated_expressions();

  for (const CompiledExpression& watch : state.watches()) {
    VariableBuilder variable_builder;

    // Set the name of the variable to the original watch expression string.
    variable_builder.set_name(watch.expression);

    if (watch.evaluator == nullptr) {
      variable_builder.set_status(
          StatusMessageBuilder()
              .set_error()
              .set_refers_to(StatusMessageModel::Context::VARIABLE_NAME)
              .set_description(watch.error_message)
              .build());
    } else {
      variable_builder.set_value(std::string());
    }

    breakpoint_builder.add_evaluated_expression(variable_builder);
  }

  format_queue_->Enqueue(breakpoint_builder.build(), nullptr);
}

void JvmBreakpoint::OnBreakpointExpired() {
  // Keep this instance alive at least until this function exits.
  std::shared_ptr<Breakpoint> instance_holder = shared_from_this();

  LOG(INFO) << "Completing expired breakpoint " << id();

  ResetToPending();

  CompleteBreakpointWithStatus(
      StatusMessageBuilder()
          .set_error()
          .set_refers_to(StatusMessageModel::Context::BREAKPOINT_AGE)
          .set_format(definition_->action == BreakpointModel::Action::LOG
                          ? LogpointExpired
                          : SnapshotExpired)
          .build());
}

}  // namespace cdbg
}  // namespace devtools
