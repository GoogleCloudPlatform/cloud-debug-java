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

#include "log_data_collector.h"

#include "jni_proxy_object.h"
#include "expression_evaluator.h"
#include "messages.h"
#include "readers_factory.h"
#include "value_formatter.h"

namespace devtools {
namespace cdbg {

// Substitutes parameter placeholders $0, $1, etc. with the parameter value
// as evaluated by "parameters".
static std::string SubstitutePlaceholders(
    const std::string& format, std::function<std::string(int)> parameters) {
  std::string result;
  result.reserve(format.size());

  for (std::string::const_iterator it = format.begin(); it != format.end();) {
    if ((*it != '$') || (it + 1 == format.end())) {
      result.push_back(*it++);
      continue;
    }

    std::string::const_iterator p = it + 1;

    // "$$" is an escaped form of "$"
    if (*p == '$') {
      result.push_back('$');
      it += 2;
      continue;
    }

    int parameter_index = 0;
    while (isdigit(*p) && (p != format.end())) {
      parameter_index = parameter_index * 10 + (*p - '0');
      ++p;
    }

    if (p == it + 1) {
      result.push_back(*it++);
      continue;
    }

    result.append(parameters(parameter_index));
    it = p;
  }

  return result;
}

// Formats structured message into a string.
// Note that we are losing the ability to localize the message that goes into
// the log.
// TODO: retain the message as is once we have structured log messages.
static std::string FormatMessage(const FormatMessageModel& message) {
  return SubstitutePlaceholders(
      message.format, [&message](int parameter_index) -> std::string {
        if ((parameter_index < 0) ||
            (parameter_index >= message.parameters.size())) {
          DCHECK(false) << "Bad parameter index " << parameter_index
                        << ", format: " << message.format;
          return std::string();
        }

        return message.parameters[parameter_index];
      });
}

// Prints out the value of JVariant or status message if present.
static std::string FormatValue(const NamedJVariant& result, bool quote_string) {
  if (result.value.type() == JType::Void) {
    return FormatMessage(result.status.description);
  }

  ValueFormatter::Options format_options;
  format_options.quote_string = quote_string;

  std::string formatted_value;
  ValueFormatter::Format(result, format_options, &formatted_value, nullptr);

  return formatted_value;
}

// Prints out all the members of an object in a yaml like format. The output
// is supposed to be human readable rather than a protocol format.
static std::string FormatMembers(const std::vector<NamedJVariant>& members) {
  if ((members.size() == 1) &&
      members[0].name.empty() &&
      members[0].status.description.format.empty()) {
    // Special case for Java strings: format single unnamed member as
    // variable value rather than as a member.
    return FormatValue(members[0], false);
  }

  std::string result;
  result += "{ ";

  for (const NamedJVariant& member : members) {
    if (result.size() > 2) {
      result += ", ";
    }

    result += member.name;
    result += ": ";
    result += FormatValue(member, true);
  }

  result += " }";

  return result;
}

// Checks if the object class has a non-default version of "toString()".
static bool HasCustomToString(const JVariant& item) {
  jobject obj = nullptr;
  if (!item.get<jobject>(&obj) || (obj == nullptr)) {
    return false;
  }

  JniLocalRef cls = GetObjectClass(obj);
  if (cls == nullptr) {
    return false;
  }

  jmethodID method_id = jni()->GetMethodID(
      static_cast<jclass>(cls.get()),
      "toString",
      "()Ljava/lang/String;");
  if (!JniCheckNoException("GetMethodID(toString)")) {
    return false;
  }

  return !jni()->IsSameObject(
      GetMethodDeclaringClass(method_id).get(),
      jniproxy::Object()->GetClass());
}


void LogDataCollector::Collect(
    MethodCaller* method_caller,
    ObjectEvaluator* object_evaluator,
    const std::vector<CompiledExpression>& watches,
    jthread thread) {
  const ClassMetadataReader::Method to_string_method =
      InstanceMethod("Ljava/lang/Object;", "toString", "()Ljava/lang/String;");

  DCHECK(watch_results_.empty())
      << "LogDataCollector::Collect is only expected to be called once";

  for (const CompiledExpression& watch : watches) {
    NamedJVariant result =
        EvaluateWatchedExpression(method_caller, watch, thread);

    if (ValueFormatter::IsValue(result)) {
      watch_results_.push_back(FormatValue(result, false));
      continue;
    }

    // If the expression evaluates to an object, there is no point in leaving
    // the object as is. It will print out as "<object>", which is not very
    // useful. Instead we get a string representation of an object.

    // Try to call "toString()" unless it's a default "Object.toString", which
    // is not too helpful.
    if (HasCustomToString(result.value)) {
      ErrorOr<JVariant> to_string =
          method_caller->Invoke(to_string_method, result.value, {});
      if (!to_string.is_error() &&
          to_string.value().has_non_null_object()) {
        result.value = ErrorOr<JVariant>::detach_value(std::move(to_string));
        result.well_known_jclass = WellKnownJClass::String;
        watch_results_.push_back(FormatValue(result, false));
        continue;
      }
    }

    // Calling "toString()" didn't work. Print all the object fields.
    jobject obj = nullptr;
    result.value.get<jobject>(&obj);

    std::vector<NamedJVariant> members;
    object_evaluator->Evaluate(method_caller, obj, false, &members);

    watch_results_.push_back(FormatMembers(members));
  }
}


NamedJVariant LogDataCollector::EvaluateWatchedExpression(
    MethodCaller* method_caller,
    const CompiledExpression& watch,
    jthread thread) const {
  if (watch.evaluator == nullptr) {
    LOG_IF(WARNING, watch.error_message.format.empty())
        << "Unavailable error message for "
           "watched expression that failed to compile";

    NamedJVariant result;
    result.status.is_error = true;
    result.status.refers_to = StatusMessageModel::Context::VARIABLE_NAME;
    result.status.description = watch.error_message;

    return result;
  }

  EvaluationContext evaluation_context;
  evaluation_context.thread = thread;
  evaluation_context.frame_depth = 0;
  evaluation_context.method_caller = method_caller;

  ErrorOr<JVariant> evaluation_result =
      watch.evaluator->Evaluate(evaluation_context);
  if (evaluation_result.is_error()) {
    NamedJVariant result;
    result.status.is_error = true;
    result.status.refers_to = StatusMessageModel::Context::VARIABLE_VALUE;
    result.status.description = evaluation_result.error_message();

    return result;
  }

  NamedJVariant result;
  result.value = ErrorOr<JVariant>::detach_value(std::move(evaluation_result));
  result.well_known_jclass =
      WellKnownJClassFromSignature(watch.evaluator->GetStaticType());

  result.value.change_ref_type(JVariant::ReferenceKind::Global);

  return result;
}

std::string LogDataCollector::Format(const BreakpointModel& breakpoint) const {
  return SubstitutePlaceholders(
      breakpoint.log_message_format, [this](int watch_index) -> std::string {
        if ((watch_index < 0) || (watch_index >= watch_results_.size())) {
          return FormatMessage({
            InvalidParameterIndex,
            { std::to_string(watch_index) }
          });
        }

        return watch_results_[watch_index];
      });
}

}  // namespace cdbg
}  // namespace devtools


