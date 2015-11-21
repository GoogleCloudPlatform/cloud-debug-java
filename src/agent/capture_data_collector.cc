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
#include "eval_call_stack.h"
#include "expression_evaluator.h"
#include "expression_util.h"
#include "local_variable_reader.h"
#include "messages.h"
#include "method_locals.h"
#include "model_util.h"
#include "object_evaluator.h"
#include "safe_method_caller.h"
#include "value_formatter.h"

namespace devtools {
namespace cdbg {

CaptureDataCollector::CaptureDataCollector() {
  // Reserve "var_table_index" 0 for memory objects that we didn't capture
  // because collector ran out of quota.
  memory_objects_.push_back(MemoryObject());
  ++memory_objects_size_;
}


CaptureDataCollector::~CaptureDataCollector() {
}


void CaptureDataCollector::Collect(
    const Config& config,
    ClassFilesCache* class_files_cache,
    EvalCallStack* eval_call_stack,
    ClassIndexer* class_indexer,
    ClassMetadataReader* class_metadata_reader,
    MethodLocals* method_locals,
    ObjectEvaluator* object_evaluator,
    const std::vector<CompiledExpression>& watches,
    jthread thread) {
  DCHECK(eval_call_stack_ == nullptr) << "Collect can only be called once";
  eval_call_stack_ = CHECK_NOTNULL(eval_call_stack);

  // Walk the call stack.
  std::vector<EvalCallStack::JvmFrame> jvm_frames;
  eval_call_stack_->Read(thread, &jvm_frames);

  const int call_frames_count = jvm_frames.size();
  call_frames_.resize(call_frames_count);
  for (int depth = 0; depth < call_frames_count; ++depth) {
    call_frames_[depth].frame_info_key = jvm_frames[depth].frame_info_key;

    // Collect local variables.
    if ((depth < kMethodLocalsFrames) &&
        (jvm_frames[depth].code_location.method != nullptr)) {
      // We don't expect any methods to be called while reading fields.
      SafeMethodCaller method_caller(
          &config,
          Config::MethodCallQuota(),  // Default quota of zero for everything.
          class_indexer,
          class_files_cache);

      EvaluationContext evaluation_context;
      evaluation_context.thread = thread;
      evaluation_context.frame_depth = depth;
      evaluation_context.method_caller = &method_caller;

      ReadLocalVariables(
          evaluation_context,
          class_metadata_reader,
          method_locals,
          jvm_frames[depth].code_location.method,
          jvm_frames[depth].code_location.location,
          &call_frames_[depth].arguments,
          &call_frames_[depth].local_variables);

      PostProcessVariables(call_frames_[depth].arguments);
      PostProcessVariables(call_frames_[depth].local_variables);
    }
  }

  // Evaluate watched expressions.
  watch_results_ = std::vector<EvaluatedExpression>(watches.size());
  for (int i = 0; i < watches.size(); ++i) {
    // Keep the original expression around so that we can populate variable
    // name.
    watch_results_[i].expression = watches[i].expression;

    if (jvm_frames.size() > 0 &&
        jvm_frames[0].code_location.method == nullptr) {

      watch_results_[i].compile_error_message = {ExpressionSensitiveData, { }};

    } else if (watches[i].evaluator != nullptr) {
      SafeMethodCaller method_caller(
          &config,
          config.GetQuota(Config::EXPRESSION_EVALUATION),
          class_indexer,
          class_files_cache);

      EvaluationContext evaluation_context;
      evaluation_context.thread = thread;
      evaluation_context.frame_depth = 0;
      evaluation_context.method_caller = &method_caller;

      EvaluateWatchedExpression(
          evaluation_context,
          class_metadata_reader,
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

  // Collect referenced objects in BFS fashion.
  auto it_pending_object = memory_objects_.begin();
  int captured_variable_table_size = 0;
  DCHECK(it_pending_object != memory_objects_.end());

  ++it_pending_object;  // First entry has a special meaning of "buffer full".
  ++captured_variable_table_size;

  SafeMethodCaller method_caller(
      &config,
      config.GetQuota(Config::PRETTY_PRINTERS),
      class_indexer,
      class_files_cache);

  while ((it_pending_object != memory_objects_.end()) &&
         CanCollectMoreMemoryObjects()) {
    object_evaluator->Evaluate(
        &method_caller,
        it_pending_object->object_ref,
        &it_pending_object->members);

    // If members of the current object contain references to other memory
    // objects, "memory_objects_" will grow inside "PostProcessVariables".
    PostProcessVariables(it_pending_object->members);

    ++it_pending_object;
    ++captured_variable_table_size;
  }

  // Remove all memory objects that were enqueued, but were not explored.
  DCHECK_EQ(memory_objects_size_, memory_objects_.size());

  memory_objects_.erase(it_pending_object, memory_objects_.end());
  memory_objects_size_ = captured_variable_table_size;

  DCHECK_EQ(memory_objects_size_, memory_objects_.size());
}


void CaptureDataCollector::ReleaseRefs() {
  object_index_map_.RemoveAll();

  watch_results_.clear();

  call_frames_.clear();

  memory_objects_.clear();
  memory_objects_size_ = 0;
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
  for (const MemoryObject& memory_object : memory_objects_) {
    std::unique_ptr<VariableModel> object_variable;

    if (breakpoint->variable_table.empty()) {
      // First entry in "memory_objects_" has a special meaning.
      object_variable = VariableBuilder::build_capture_buffer_full_variable();
    } else {
      if ((memory_object.members.size() == 1) &&
          memory_object.members[0].name.empty() &&
          memory_object.members[0].status.description.format.empty()) {
        // Special case for Java strings: format single unnamed member as
        // variable value rather than as a member. We don't want to do this
        // collapsing for synthetic member entries like "object has no fields".
        object_variable = FormatVariable(memory_object.members[0]);
      } else {
        object_variable.reset(new VariableModel);

        if (!memory_object.status.description.format.empty()) {
          object_variable->status =
              StatusMessageBuilder(memory_object.status).build();
        }

        FormatVariablesArray(
            memory_object.members,
            &object_variable->members);
      }
    }

    breakpoint->variable_table.push_back(std::move(object_variable));
  }
}


void CaptureDataCollector::ReadLocalVariables(
    const EvaluationContext& evaluation_context,
    ClassMetadataReader* class_metadata_reader,
    MethodLocals* method_locals,
    jmethodID method,
    jlocation location,
    std::vector<NamedJVariant>* arguments,
    std::vector<NamedJVariant>* local_variables) {

  if (method == nullptr) {
    return;
  }

  std::shared_ptr<const MethodLocals::Entry> entry =
      method_locals->GetLocalVariables(method);

  // TODO(vlif): refactor this function to add locals and arguments to
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
    if (!reader->ReadValue(evaluation_context, &item.value)) {
      item.status.is_error = false;
      item.status.refers_to = StatusMessageModel::Context::VARIABLE_VALUE;
      item.status.description = INTERNAL_ERROR_MESSAGE;
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
    ClassMetadataReader* class_metadata_reader,
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
    target->push_back(FormatVariable(item));
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
      target->push_back(FormatVariable(item.evaluation_result));
    }
  }
}


std::unique_ptr<VariableModel> CaptureDataCollector::FormatVariable(
    const NamedJVariant& source) const {
  std::unique_ptr<VariableModel> target(new VariableModel);

  target->name = source.name;

  if (!source.status.description.format.empty()) {
    target->status = StatusMessageBuilder(source.status).build();
  } else {
    if (ValueFormatter::IsValue(source)) {
      string formatted_value;
      ValueFormatter::Format(
          source,
          ValueFormatter::Options(),
          &formatted_value);
      target->value = formatted_value;
    } else {
      jobject ref = nullptr;
      const int* var_table_index = nullptr;
      if (source.value.get<jobject>(&ref)) {
        var_table_index = object_index_map_.Find(ref);
      }

      if (var_table_index == nullptr) {
        target->status = StatusMessageBuilder()
            .set_error()
            .set_refers_to(StatusMessageModel::Context::VARIABLE_VALUE)
            .set_description(INTERNAL_ERROR_MESSAGE)
            .build();
      } else {
        if (*var_table_index < memory_objects_size_) {
          target->var_table_index = *var_table_index;
        } else {
          // Collector ran out of quota before the current object was explored.
          // Set "var_table_index" to 0, which is an empty object (with no
          // fields) and has a special meaning ("buffer full").
          target->var_table_index.set_value(0);
        }
      }
    }
  }

  return target;
}


string CaptureDataCollector::GetFunctionName(int depth) const {
  const int frame_info_key = call_frames_[depth].frame_info_key;
  const auto& frame_info =
      eval_call_stack_->ResolveCallFrameKey(frame_info_key);

  string function_name =
      TypeNameFromJObjectSignature(frame_info.class_signature);
  function_name += '.';
  function_name += frame_info.method_name;

  return function_name;
}


std::unique_ptr<SourceLocationModel>
CaptureDataCollector::GetCallFrameSourceLocation(int depth) const {
  const int frame_info_key = call_frames_[depth].frame_info_key;
  const auto& frame_info =
      eval_call_stack_->ResolveCallFrameKey(frame_info_key);

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

  // Try to insert the next index of Java object into the map. If this object
  // has already been encountered, it will be in memory_objects_ and "Insert"
  // will return false. In this case no further action is necessary.
  const bool is_new_object =
      object_index_map_.Insert(ref, memory_objects_size_);
  if (!is_new_object) {
    return;
  }

  // Now that the index is in the map, create the actual entry in
  // "memory_objects_".
  MemoryObject new_memory_object;
  new_memory_object.object_ref = ref;

  memory_objects_.push_back(std::move(new_memory_object));
  ++memory_objects_size_;
}


bool CaptureDataCollector::CanCollectMoreMemoryObjects() const {
  return total_variables_size_ < kBreakpointMaxCaptureSize;
}


}  // namespace cdbg
}  // namespace devtools


