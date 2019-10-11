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

#include "capture_data_collector.h"

#include <algorithm>

#include "encoding_util.h"
#include "eval_call_stack.h"
#include "expression_evaluator.h"
#include "expression_util.h"
#include "local_variable_reader.h"
#include "messages.h"
#include "method_locals.h"
#include "model_util.h"
#include "object_evaluator.h"
#include "value_formatter.h"

ABSL_FLAG(
    bool, cdbg_capture_user_id, true,
    "If true, the agent also captures the end user identity for audit logging");

namespace devtools {
namespace cdbg {

// Local utility functions
namespace {

void MergeLabels(std::map<std::string, std::string>* existing_labels,
                 std::map<std::string, std::string> agent_labels) {
  // This will merge in the agent labels with the pre existing client labels
  // that may already be present. In the event of a duplicate label the insert
  // will favour the pre existing labels an not update the entry.  This
  // generally should not be an issue as the pre existing client label names are
  // chosen with care and there should be no conflicts.
  existing_labels->insert(agent_labels.begin(), agent_labels.end());
}

}  // namespace

CaptureDataCollector::CaptureDataCollector(JvmEvaluators* evaluators)
    : evaluators_(evaluators) {
  // Reserve "var_table_index" 0 for memory objects that we didn't capture
  // because collector ran out of quota.
  explored_memory_objects_.push_back(MemoryObject());
}


CaptureDataCollector::~CaptureDataCollector() {
}


void CaptureDataCollector::Collect(
    const std::vector<CompiledExpression>& watches,
    jthread thread) {
  // Collect information about the local environment, but don't format it yet.
  DCHECK(evaluators_->labels_factory);
  breakpoint_labels_provider_ = evaluators_->labels_factory();
  DCHECK(breakpoint_labels_provider_);
  breakpoint_labels_provider_->Collect();

  // Collect current end user identity, but don't format it yet.
  if (absl::GetFlag(FLAGS_cdbg_capture_user_id)) {
    DCHECK(evaluators_->user_id_provider_factory);
    user_id_provider_ = evaluators_->user_id_provider_factory();
    DCHECK(user_id_provider_);
    user_id_provider_->Collect();
  }

  std::unique_ptr<MethodCaller> pretty_printers_method_caller =
      evaluators_->method_caller_factory(Config::PRETTY_PRINTERS);

  // Get the call stack frames.
  std::vector<EvalCallStack::JvmFrame> jvm_frames;
  evaluators_->eval_call_stack->Read(thread, &jvm_frames);

  // Collect and evaluate watched expressions.
  // We fill our buffer with all watched expression before we process
  // call stack frames, arguments and local variables. The rationale is that we
  // don't trim iterable and array values for expressions (users want them in
  // full since they added them manually). Therefore we use our buffer space
  // for expresssion first, and proceed to frames with rest of it.
  CollectWatchExpressions(watches, thread, jvm_frames);

  // Collect referenced objects of watched expressions in BFS fashion.
  EvaluateEnqueuedObjects(true, pretty_printers_method_caller.get());

  // Walk the call stack.
  CollectCallStack(thread, jvm_frames, pretty_printers_method_caller.get());

  // Collect referenced objects of call stack in BFS fashion.
  EvaluateEnqueuedObjects(false, pretty_printers_method_caller.get());
}


void CaptureDataCollector::CollectWatchExpressions(
    const std::vector<CompiledExpression>& watches,
    jthread thread,
    const std::vector<EvalCallStack::JvmFrame>& jvm_frames) {
  watch_results_ = std::vector<EvaluatedExpression>(watches.size());
  for (int i = 0; i < watches.size(); ++i) {
    // Keep the original expression around so that we can populate variable
    // name.
    watch_results_[i].expression = watches[i].expression;

    if (jvm_frames.size() > 0 &&
        jvm_frames[0].code_location.method == nullptr) {

      watch_results_[i].compile_error_message = {ExpressionSensitiveData, { }};

    } else if (watches[i].evaluator != nullptr) {
      std::unique_ptr<MethodCaller> expression_method_caller =
          evaluators_->method_caller_factory(Config::EXPRESSION_EVALUATION);

      EvaluationContext evaluation_context;
      evaluation_context.thread = thread;
      evaluation_context.frame_depth = 0;
      evaluation_context.method_caller = expression_method_caller.get();

      EvaluateWatchedExpression(
          evaluation_context,
          *watches[i].evaluator,
          &watch_results_[i].evaluation_result);

      PostProcessVariable(watch_results_[i].evaluation_result);
    } else {
      watch_results_[i].compile_error_message = watches[i].error_message;

      LOG_IF(WARNING, watch_results_[i].compile_error_message.format.empty())
          << "Unavailable error message for "
             "watched expression that failed to compile";
    }
  }
}


void CaptureDataCollector::CollectCallStack(
    jthread thread,
    const std::vector<EvalCallStack::JvmFrame>& jvm_frames,
    MethodCaller* pretty_printers_method_caller) {
  const int call_frames_count = jvm_frames.size();
  call_frames_.resize(call_frames_count);
  for (int depth = 0; depth < call_frames_count; ++depth) {
    call_frames_[depth].frame_info_key = jvm_frames[depth].frame_info_key;

    // Collect local variables.
    if ((depth < kMethodLocalsFrames) &&
        (jvm_frames[depth].code_location.method != nullptr)) {
      EvaluationContext evaluation_context;
      evaluation_context.thread = thread;
      evaluation_context.frame_depth = depth;
      evaluation_context.method_caller = pretty_printers_method_caller;

      ReadLocalVariables(
          evaluation_context,
          jvm_frames[depth].code_location.method,
          jvm_frames[depth].code_location.location,
          &call_frames_[depth].arguments,
          &call_frames_[depth].local_variables);

      PostProcessVariables(call_frames_[depth].arguments);
      PostProcessVariables(call_frames_[depth].local_variables);
    }
  }
}


// Collect referenced objects in BFS fashion.
void CaptureDataCollector::EvaluateEnqueuedObjects(
    bool is_watch_expression,
    MethodCaller* pretty_printers_method_caller) {
  while (!unexplored_memory_objects_.empty() &&
         CanCollectMoreMemoryObjects()) {
    // We promote objects from "unexplored_memory_objects_" to
    // "explored_memory_objects_" as long as space permits
    // When our buffer is full and we cannot collect
    // more memory objects, we drop them and have no indexes to those
    // references in "explored_object_index_map_". We later handle this case
    // in FormatVariable() by redirecting not found indexes to special index 0.
    auto pending_object = *unexplored_memory_objects_.begin();
    unexplored_memory_objects_.pop_front();

    evaluators_->object_evaluator->Evaluate(
        pretty_printers_method_caller,
        pending_object.object_ref,
        is_watch_expression,
        &pending_object.members);

    // If members of the current object contain references to other unique
    // memory objects, "unexplored_memory_objects_" will grow inside
    // "PostProcessVariables"
    PostProcessVariables(pending_object.members);

    // Insert the next index of Java object into the map.
    // Since "unexplored_memory_objects_" contains only objects with unique
    // reference, it should not be encountered in "explored_object_index_map_"
    // and "Insert" should always succeed.
    const bool is_new_object = explored_object_index_map_.Insert(
        pending_object.object_ref, explored_memory_objects_.size());
    DCHECK(is_new_object);

    // Now that the index is in the map, create the actual entry in
    // "explored_memory_objects_"
    explored_memory_objects_.push_back(std::move(pending_object));
  }

  // At this point we either moved all pending objects into
  // "explored_memory_objects_", or we run out of quota (buffer full).
  unexplored_memory_objects_.clear();
}

void CaptureDataCollector::FormatByteArray(
    const std::vector<NamedJVariant>& source,
    std::vector<std::unique_ptr<VariableModel>>* target) const {
  std::vector<char> bytes(source.size());
  int bytes_count = 0;
  for (auto it = source.begin(); it != source.end(); ++it) {
    if (it->value.type() == JType::Byte) {
      it->value.get(reinterpret_cast<jbyte*>(&bytes[bytes_count]));
      ++bytes_count;
    }
  }

  if (bytes_count == 0) {
    return;
  }

  int valid_utf8_bytes = ValidateUtf8(bytes.data(), bytes_count);
  // Possibly add $utf8 field. We allow leeway in the case that the array was
  // trimmed in the middle of an extended sequence.
  if (valid_utf8_bytes > 0 && valid_utf8_bytes > bytes_count - 3) {
    std::unique_ptr<VariableModel> utf8(new VariableModel);
    utf8->name = "$utf8";
    utf8->type = "String";
    utf8->value = "\"" + std::string(bytes.data(), valid_utf8_bytes) + "\"";
    target->push_back(std::move(utf8));
  }

  std::unique_ptr<VariableModel> base64(new VariableModel);
  base64->name = "$base64";
  base64->type = "String";
  base64->value = Base64Encode(bytes.data(), bytes_count);
  target->push_back(std::move(base64));
}

void CaptureDataCollector::ReleaseRefs() {
  explored_object_index_map_.RemoveAll();

  unique_objects_.RemoveAll();

  watch_results_.clear();

  call_frames_.clear();

  unexplored_memory_objects_.clear();

  explored_memory_objects_.clear();
}


void CaptureDataCollector::Format(BreakpointModel* breakpoint) const {
  // Format stack trace.
  breakpoint->stack.clear();
  for (int depth = 0; depth < call_frames_.size(); ++depth) {
    std::unique_ptr<StackFrameModel> frame(new StackFrameModel);

    frame->function = GetFunctionName(depth);
    frame->location = GetCallFrameSourceLocation(depth);

    FormatVariablesArray(
        call_frames_[depth].arguments,
        &frame->arguments);

    FormatVariablesArray(
        call_frames_[depth].local_variables,
        &frame->locals);

    breakpoint->stack.push_back(std::move(frame));
  }

  // Format watched expressions.
  FormatWatchedExpressions(&breakpoint->evaluated_expressions);

  // Format referenced memory objects (within the quota).
  breakpoint->variable_table.clear();
  for (const MemoryObject& memory_object : explored_memory_objects_) {
    std::unique_ptr<VariableModel> object_variable;

    if (breakpoint->variable_table.empty()) {
      // First entry in "explored_memory_objects_" has a special meaning.
      object_variable = VariableBuilder::build_capture_buffer_full_variable();
    } else {
      if ((memory_object.members.size() == 1) &&
          memory_object.members[0].name.empty() &&
          memory_object.members[0].status.description.format.empty()) {
        // Special case for Java strings: format single unnamed member as
        // variable value rather than as a member. We don't want to do this
        // collapsing for synthetic member entries like "object has no fields".
        //
        // TODO: it is possible that the string object is referenced by
        // a watched expression. In this case we should pass true in
        // "FormatVariable" to increase the size limit of captured a string
        // string object.
        object_variable = FormatVariable(memory_object.members[0], false);
      } else {
        object_variable.reset(new VariableModel);

        object_variable->type = TypeNameFromSignature({
            JType::Object,
            GetObjectClassSignature(memory_object.object_ref)
        });

        if (!memory_object.status.description.format.empty()) {
          object_variable->status =
              StatusMessageBuilder(memory_object.status).build();
        }

        if (object_variable->type == "byte[]") {
          FormatByteArray(memory_object.members, &object_variable->members);
        }

        FormatVariablesArray(memory_object.members, &object_variable->members);
      }
    }

    breakpoint->variable_table.push_back(std::move(object_variable));
  }

  // Format the breakpoint labels and merge them with the existing client
  // labels.
  MergeLabels(&breakpoint->labels, breakpoint_labels_provider_->Format());

  // Format the end user identity.
  if (absl::GetFlag(FLAGS_cdbg_capture_user_id)) {
    std::string kind;
    std::string id;
    if (user_id_provider_->Format(&kind, &id)) {
      breakpoint->evaluated_user_id.reset(new UserIdModel());
      breakpoint->evaluated_user_id->kind = kind;
      breakpoint->evaluated_user_id->id = id;
    }
  }
}


void CaptureDataCollector::ReadLocalVariables(
    const EvaluationContext& evaluation_context,
    jmethodID method,
    jlocation location,
    std::vector<NamedJVariant>* arguments,
    std::vector<NamedJVariant>* local_variables) {

  if (method == nullptr) {
    return;
  }

  std::shared_ptr<const MethodLocals::Entry> entry =
      evaluators_->method_locals->GetLocalVariables(method);

  // TODO: refactor this function to add locals and arguments to
  // output vectors as we go.

  // Count number of local variables that are defined at "location".
  int arguments_count = 0;
  int local_variables_count = 0;
  for (const auto& reader : entry->locals) {
    if (reader->IsDefinedAtLocation(location)) {
      if (reader->IsArgument()) {
        ++arguments_count;
      } else {
        ++local_variables_count;
      }
    }
  }

  *arguments = std::vector<NamedJVariant>(arguments_count);
  *local_variables = std::vector<NamedJVariant>(local_variables_count);

  int arguments_index = 0;
  int local_variables_index = 0;

  for (const auto& reader : entry->locals) {
    if (!reader->IsDefinedAtLocation(location)) {
      continue;
    }

    NamedJVariant& item = reader->IsArgument()
        ? (*arguments)[arguments_index++]
        : (*local_variables)[local_variables_index++];

    item.name = reader->GetName();
    FormatMessageModel error;
    if (!reader->ReadValue(evaluation_context, &item.value, &error)) {
      item.status.is_error = false;
      item.status.refers_to = StatusMessageModel::Context::VARIABLE_VALUE;
      item.status.description = error;
    } else {
      item.well_known_jclass =
          WellKnownJClassFromSignature(reader->GetStaticType());
    }

    item.value.change_ref_type(JVariant::ReferenceKind::Global);
  }

  DCHECK_EQ(arguments_count, arguments_index);
  DCHECK_EQ(local_variables_count, local_variables_index);
}


void CaptureDataCollector::EvaluateWatchedExpression(
    const EvaluationContext& evaluation_context,
    const ExpressionEvaluator& watch_evaluator,
    NamedJVariant* result) {
  ErrorOr<JVariant> evaluation_result =
    watch_evaluator.Evaluate(evaluation_context);
  if (evaluation_result.is_error()) {
    result->status.is_error = true;
    result->status.refers_to = StatusMessageModel::Context::VARIABLE_VALUE;
    result->status.description = evaluation_result.error_message();
  } else {
    result->value =
        ErrorOr<JVariant>::detach_value(std::move(evaluation_result));
  }

  result->well_known_jclass =
      WellKnownJClassFromSignature(watch_evaluator.GetStaticType());
  result->value.change_ref_type(JVariant::ReferenceKind::Global);
}


void CaptureDataCollector::FormatVariablesArray(
    const std::vector<NamedJVariant>& source,
    std::vector<std::unique_ptr<VariableModel>>* target) const {
  for (const NamedJVariant& item : source) {
    target->push_back(FormatVariable(item, false));
  }
}


void CaptureDataCollector::FormatWatchedExpressions(
    std::vector<std::unique_ptr<VariableModel>>* target) const {
  target->clear();

  for (const EvaluatedExpression& item : watch_results_) {
    if (!item.compile_error_message.format.empty()) {
      target->push_back(VariableBuilder()
          .set_name(item.expression)
          .set_status(StatusMessageBuilder()
              .set_error()
              .set_refers_to(StatusMessageModel::Context::VARIABLE_NAME)
              .set_description(item.compile_error_message))
          .build());
    } else {
      target->push_back(FormatVariable(item.evaluation_result, true));
    }
  }
}


std::unique_ptr<VariableModel> CaptureDataCollector::FormatVariable(
    const NamedJVariant& source,
    bool is_watched_expression) const {
  std::unique_ptr<VariableModel> target(new VariableModel);

  target->name = source.name;

  if (!source.status.description.format.empty()) {
    target->status = StatusMessageBuilder(source.status).build();
  } else {
    if (ValueFormatter::IsValue(source)) {
      ValueFormatter::Options options;
      if (is_watched_expression) {
        options.max_string_length = kExtendedMaxStringLength;
      }

      std::string formatted_value;
      target->status = ValueFormatter::Format(
          source, options, &formatted_value, &target->type);
      target->value = std::move(formatted_value);
    } else {
      jobject ref = nullptr;
      const int* var_table_index = nullptr;
      if (source.value.get<jobject>(&ref)) {
        var_table_index = explored_object_index_map_.Find(ref);
      }

      if (var_table_index == nullptr) {
        // Index not found.
        // Collector ran out of quota before the current object was explored.
        // Set "var_table_index" to 0, which is an empty object (with no
        // fields) and has a special meaning ("buffer full").
        target->var_table_index.set_value(0);
      } else {
        if (*var_table_index < explored_memory_objects_.size()) {
          target->var_table_index = *var_table_index;
        } else {
          // We are not supposed to have index larger than
          // explored_memory_objects_.size() as indexes match objects that we
          // promote from "unexplored_memory_objects_" to
          // "explored_memory_objects_" in EvaluateEnqueuedObjects()
          target->status = StatusMessageBuilder()
              .set_error()
              .set_refers_to(StatusMessageModel::Context::VARIABLE_VALUE)
              .set_description(INTERNAL_ERROR_MESSAGE)
              .build();
        }
      }
    }
  }

  return target;
}

std::string CaptureDataCollector::GetFunctionName(int depth) const {
  const int frame_info_key = call_frames_[depth].frame_info_key;
  const auto& frame_info =
      evaluators_->eval_call_stack->ResolveCallFrameKey(frame_info_key);

  std::string function_name =
      TypeNameFromJObjectSignature(frame_info.class_signature);
  function_name += '.';
  function_name += frame_info.method_name;

  return function_name;
}

std::unique_ptr<SourceLocationModel>
CaptureDataCollector::GetCallFrameSourceLocation(int depth) const {
  const int frame_info_key = call_frames_[depth].frame_info_key;
  const auto& frame_info =
      evaluators_->eval_call_stack->ResolveCallFrameKey(frame_info_key);

  std::unique_ptr<SourceLocationModel> location(new SourceLocationModel);

  location->path = ConstructFilePath(
      frame_info.class_signature.c_str(),
      frame_info.source_file_name.c_str());

  location->line = frame_info.line_number;

  return location;
}


void CaptureDataCollector::PostProcessVariable(
    const NamedJVariant& variable) {
  // Even if due to some error the variable has a zero size, we still want
  // to add a non-zero. This is to avoid any potential endless loops.
  total_variables_size_ +=
      std::max(1, ValueFormatter::GetTotalDataSize(variable));

  EnqueueRef(variable);
}


void CaptureDataCollector::PostProcessVariables(
    const std::vector<NamedJVariant>& variables) {
  for (const NamedJVariant& variable : variables) {
    PostProcessVariable(variable);
  }
}


void CaptureDataCollector::EnqueueRef(const NamedJVariant& var) {
  // Nothing to do if "var" is not a reference.
  if (ValueFormatter::IsValue(var)) {
    return;
  }

  jobject ref = nullptr;
  if (!var.value.get<jobject>(&ref) || (ref == nullptr)) {
    return;
  }

  // Try to insert a ref for the Java object into the map. If this object
  // has already been encountered, it will be in unique_objects_ and "Insert"
  // will return false. In this case no further action is necessary.
  const bool is_new_object = unique_objects_.Insert(ref, 0);
  if (!is_new_object) {
    return;
  }

  // Now create the actual entry in "unexplored_memory_objects_".
  MemoryObject new_memory_object;
  new_memory_object.object_ref = ref;

  unexplored_memory_objects_.push_back(std::move(new_memory_object));
}


bool CaptureDataCollector::CanCollectMoreMemoryObjects() const {
  return total_variables_size_ < kBreakpointMaxCaptureSize;
}


}  // namespace cdbg
}  // namespace devtools
