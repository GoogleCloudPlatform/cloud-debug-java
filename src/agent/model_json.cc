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

#include "model_json.h"

#include <cstdint>

#include "jni_proxy_api_client_datetime.h"
#include "jsoncpp_util.h"
#include "model_util.h"
#include "json/json.h"

namespace devtools {
namespace cdbg {

// Maps enum to strings.
#define ENUM_CODE_MAP(enum_type, code) \
  { \
    enum_type::code, \
    #code \
  }

static const struct StatusContextCode {
  StatusMessageModel::Context enum_code;
  const char* enum_string;
} status_context_codes_map[] = {
  ENUM_CODE_MAP(StatusMessageModel::Context, UNSPECIFIED),
  ENUM_CODE_MAP(StatusMessageModel::Context, BREAKPOINT_SOURCE_LOCATION),
  ENUM_CODE_MAP(StatusMessageModel::Context, BREAKPOINT_CONDITION),
  ENUM_CODE_MAP(StatusMessageModel::Context, BREAKPOINT_EXPRESSION),
  ENUM_CODE_MAP(StatusMessageModel::Context, BREAKPOINT_AGE),
  ENUM_CODE_MAP(StatusMessageModel::Context, VARIABLE_NAME),
  ENUM_CODE_MAP(StatusMessageModel::Context, VARIABLE_VALUE)
};


// Maps breakpoint action to enum strings.
static const struct BreakpointActionCode {
  BreakpointModel::Action enum_code;
  const char* enum_string;
} breakpoint_action_codes_map[] = {
  ENUM_CODE_MAP(BreakpointModel::Action, CAPTURE),
  ENUM_CODE_MAP(BreakpointModel::Action, LOG)
};


// Maps log level to enum strings.
static const struct BreakpointLogLevelCode {
  BreakpointModel::LogLevel enum_code;
  const char* enum_string;
} breakpoint_log_level_codes_map[] = {
  ENUM_CODE_MAP(BreakpointModel::LogLevel, INFO),
  ENUM_CODE_MAP(BreakpointModel::LogLevel, WARNING),
  ENUM_CODE_MAP(BreakpointModel::LogLevel, ERROR)
};


// All DeserializeModel(root) methods are implemented through template
// specialization.
template <typename TModel>
static std::unique_ptr<TModel> DeserializeModel(const Json::Value& root);

template <typename TElement>
static void SerializeModel(const std::vector<std::unique_ptr<TElement>>& model,
                           const std::string& array_name, Json::Value* root) {
  if (model.empty()) {
    return;
  }

  Json::Value array(Json::arrayValue);
  for (const std::unique_ptr<TElement>& element : model) {
    Json::Value& element_value = array.append(Json::Value());
    SerializeModel(*element, &element_value);
  }

  (*root)[array_name].swap(array);
}

static void SerializeModel(const std::map<std::string, std::string>& model,
                           Json::Value* root) {
  for (const auto& element : model) {
    (*root)[element.first] = Json::Value(element.second);
  }
}

template <typename TElement>
static bool DeserializeModel(
    const Json::Value& root,
    std::vector<std::unique_ptr<TElement>>* model) {
  model->clear();

  for (const Json::Value& element_value : root) {
    std::unique_ptr<TElement> element_model =
        DeserializeModel<TElement>(element_value);
    if (element_model == nullptr) {
      return false;
    }

    model->push_back(std::move(element_model));
  }

  return true;
}

static bool DeserializeModel(const Json::Value& root,
                             std::map<std::string, std::string>* model) {
  model->clear();

  if (root.isNull()) {
    return true;
  }

  if (!root.isObject()) {
    LOG(WARNING) << "Bad map type";
    return false;
  }

  for (const std::string& key : root.getMemberNames()) {
    std::string value = JsonCppGetString(root, key.c_str());
    if (value.empty()) {
      LOG(WARNING) << "Bad map entry for " << key;
      return false;
    }

    (*model)[key] = value;
  }

  return true;
}

template <typename TElement>
static void SerializeModel(const std::vector<TElement>& model,
                           const std::string& array_name, Json::Value* root) {
  if (model.empty()) {
    return;
  }

  Json::Value& array = (*root)[array_name];
  for (const TElement& element : model) {
    array.append(Json::Value(element));
  }
}

template <typename TElement>
static bool DeserializeModel(
    const Json::Value& root,
    std::vector<TElement>* model) {
  model->clear();

  for (const Json::Value& element_value : root) {
    if (!element_value.isString()) {
      return false;
    }

    model->push_back(element_value.asString());
  }

  return true;
}


static void SerializeModel(
    const FormatMessageModel& model,
    Json::Value* root) {
  (*root)["format"] = Json::Value(model.format);
  SerializeModel(model.parameters, "parameters", root);
}


static void SerializeRefersTo(
    StatusMessageModel::Context refers_to,
    Json::Value* root) {
  // No need to set the default values.
  if (refers_to == StatusMessageModel::Context::UNSPECIFIED) {
    return;
  }

  for (const auto& entry : status_context_codes_map) {
    if (refers_to == entry.enum_code) {
      (*root)["refersTo"] = Json::Value(entry.enum_string);
      return;
    }
  }

  LOG(ERROR) << "Invalid 'refers_to' value: " << static_cast<int>(refers_to);
}


static void SerializeBreakpointAction(
    BreakpointModel::Action action,
    Json::Value* root) {
  // No need to set the default values.
  if (action == BreakpointModel::Action::CAPTURE) {
    return;
  }

  for (const auto& entry : breakpoint_action_codes_map) {
    if (action == entry.enum_code) {
      (*root)["action"] = Json::Value(entry.enum_string);
      return;
    }
  }

  LOG(ERROR) << "Invalid 'action' value: " << static_cast<int>(action);
}


static void SerializeLogLevel(
    BreakpointModel::LogLevel log_level,
    Json::Value* root) {
  // No need to set the default values.
  if (log_level == BreakpointModel::LogLevel::INFO) {
    return;
  }

  for (const auto& entry : breakpoint_log_level_codes_map) {
    if (log_level == entry.enum_code) {
      (*root)["logLevel"] = Json::Value(entry.enum_string);
      return;
    }
  }

  LOG(ERROR) << "Invalid 'log_level' value: " << static_cast<int>(log_level);
}


// Returns seconds and milliseconds formatted as an RFC3339 timestamp string.
// Returns empty string in case of error.
static std::string FormatTime(int64_t seconds, int32_t millis) {
  int64_t total_millis = seconds * 1000 + millis;

  // Enforce formatting in UTC (e.g., 2015-10-06T20:37:19.212Z).
  int32_t timeZoneShift = 0;

  JniLocalRef datetime =
      jniproxy::DateTime()->NewObject(total_millis, timeZoneShift)
      .Release(ExceptionAction::LOG_AND_IGNORE);
  if (datetime == nullptr) {
    return std::string();
  }

  return jniproxy::DateTime()
      ->toStringRfc3339(datetime.get())
      .Release(ExceptionAction::LOG_AND_IGNORE);
}

static void SerializeTimestamp(
    const TimestampModel& model,
    Json::Value* root) {
  std::string value = FormatTime(model.seconds, model.nanos / (1000 * 1000));
  if (value.empty()) {
    LOG(ERROR) << "Failed to format timestamp value: "
               << " seconds=" << model.seconds
               << ", nanos=" << model.nanos;
  }

  (*root) = Json::Value(value);
}


static void SerializeModel(
    const StatusMessageModel& model,
    Json::Value* root) {
  (*root)["isError"] = Json::Value(model.is_error);
  SerializeRefersTo(model.refers_to, root);

  SerializeModel(model.description, &(*root)["description"]);
}


static void SerializeModel(
    const SourceLocationModel& model,
    Json::Value* root) {
  (*root)["path"] = Json::Value(model.path);
  (*root)["line"] = Json::Value(model.line);
}


StatusMessageModel::Context DeserializeRefersTo(const Json::Value& root) {
  std::string refers_to = JsonCppGetString(root, "refersTo");
  if (refers_to.empty()) {
    return StatusMessageModel::Context::UNSPECIFIED;  // default
  }

  for (const auto& entry : status_context_codes_map) {
    if (refers_to == entry.enum_string) {
      return entry.enum_code;
    }
  }

  LOG(ERROR) << "Invalid 'refers_to' value: " << refers_to;
  return StatusMessageModel::Context::UNSPECIFIED;
}


BreakpointModel::Action DeserializeBreakpointAction(const Json::Value& root) {
  const std::string& action = JsonCppGetString(root, "action");
  if (action.empty()) {
    return BreakpointModel::Action::CAPTURE;  // default
  }

  for (const auto& entry : breakpoint_action_codes_map) {
    if (action == entry.enum_string) {
      return entry.enum_code;
    }
  }

  LOG(ERROR) << "Invalid 'action' value: " << action;
  return BreakpointModel::Action::CAPTURE;
}


BreakpointModel::LogLevel DeserializeLogLevel(const Json::Value& root) {
  const std::string& log_level = JsonCppGetString(root, "logLevel");
  if (log_level.empty()) {
    return BreakpointModel::LogLevel::INFO;  // default
  }

  for (const auto& entry : breakpoint_log_level_codes_map) {
    if (log_level == entry.enum_string) {
      return entry.enum_code;
    }
  }

  LOG(ERROR) << "Invalid 'log_level' value: " << log_level;
  return BreakpointModel::LogLevel::INFO;
}


// Parses RFC3339 timestamp string and convert it into the number of
// milliseconds passed since Unix epoch. Returns 0 in case of error.
static int64_t ParseTime(const std::string& input) {
  JniLocalRef datetime = jniproxy::DateTime()->parseRfc3339(input).Release(
      ExceptionAction::LOG_AND_IGNORE);
  if (datetime == nullptr) {
    return 0;
  }

  // DateTime only supports millisecond granularity for Rfc3339.
  return jniproxy::DateTime()
      ->getValue(datetime.get())
      .Release(ExceptionAction::LOG_AND_IGNORE);
}

TimestampModel DeserializeTimestamp(const Json::Value& root) {
  if (!root.isString()) {
    return kUnspecifiedTimestamp;
  }

  std::string value = root.asString();

  int64_t total_millis = ParseTime(value);
  if (total_millis == 0) {
    return kUnspecifiedTimestamp;
  }

  TimestampModel timestamp;
  timestamp.seconds = total_millis / 1000;
  timestamp.nanos = (total_millis % 1000) * 1000 * 1000;

  return timestamp;
}


template <>
std::unique_ptr<StatusMessageModel> DeserializeModel<StatusMessageModel>(
    const Json::Value& root) {
  std::unique_ptr<StatusMessageModel> model(new StatusMessageModel);

  model->is_error = JsonCppGetBool(root, "isError", false);
  model->refers_to = DeserializeRefersTo(root);

  const Json::Value& description = root["description"];
  model->description.format = JsonCppGetString(description, "format");

  if (!DeserializeModel(
        description["parameters"],
        &model->description.parameters)) {
    return nullptr;
  }

  return model;
}


template <>
std::unique_ptr<SourceLocationModel> DeserializeModel<SourceLocationModel>(
    const Json::Value& root) {
  std::unique_ptr<SourceLocationModel> model(new SourceLocationModel);

  // Path.
  model->path = JsonCppGetString(root, "path");

  // Line.
  model->line = JsonCppGetInt(root, "line", 0);

  return model;
}


static void SerializeModel(
    const VariableModel& model,
    Json::Value* root) {
  if (!model.name.empty()) {
    (*root)["name"] = Json::Value(model.name);
  }

  if (model.value.has_value()) {
    (*root)["value"] = Json::Value(model.value.value());
  }

  if (!model.type.empty()) {
    (*root)["type"] = Json::Value(model.type);
  }

  if (model.var_table_index.has_value()) {
    (*root)["varTableIndex"] =
        Json::Value(static_cast<int>(model.var_table_index.value()));
  }

  SerializeModel(model.members, "members", root);

  if (model.status != nullptr) {
    SerializeModel(*model.status, &(*root)["status"]);
  }
}


template <>
std::unique_ptr<VariableModel> DeserializeModel<VariableModel>(
    const Json::Value& root) {
  std::unique_ptr<VariableModel> model(new VariableModel);

  // Function.
  model->name = JsonCppGetString(root, "name");

  // Value.
  if (root.isMember("value")) {
    model->value = JsonCppGetString(root, "value");
  } else {
    model->value.clear();
  }

  // Type.
  if (root.isMember("type")) {
    model->type = JsonCppGetString(root, "type");
  } else {
    model->type.clear();
  }

  // Reference to object in "variable_table".
  if (root.isMember("varTableIndex")) {
    model->var_table_index = JsonCppGetInt(root, "varTableIndex", -1);
  } else {
    model->var_table_index.clear();
  }

  // Members.
  if (!DeserializeModel(root["members"], &model->members)) {
    return nullptr;
  }

  // Variable status.
  if (root.isMember("status")) {
    model->status = DeserializeModel<StatusMessageModel>(root["status"]);
  }

  return model;
}


static void SerializeModel(
    const StackFrameModel& model,
    Json::Value* root) {
  (*root)["function"] = Json::Value(model.function);

  if (model.location != nullptr) {
    SerializeModel(*model.location, &(*root)["location"]);
  }

  SerializeModel(model.arguments, "arguments", root);

  SerializeModel(model.locals, "locals", root);
}


template <>
std::unique_ptr<StackFrameModel> DeserializeModel<StackFrameModel>(
    const Json::Value& root) {
  std::unique_ptr<StackFrameModel> model(new StackFrameModel);

  // Function.
  model->function = JsonCppGetString(root, "function");

  // Location.
  model->location = DeserializeModel<SourceLocationModel>(root["location"]);
  if (model->location == nullptr) {
    return nullptr;
  }

  // Function arguments.
  if (!DeserializeModel(root["arguments"], &model->arguments)) {
    return nullptr;
  }

  // Local variables.
  if (!DeserializeModel(root["locals"], &model->locals)) {
    return nullptr;
  }

  return model;
}


static void SerializeModel(
    const BreakpointModel& model,
    Json::Value* root) {
  (*root)["id"] = Json::Value(model.id);

  SerializeBreakpointAction(model.action, root);

  if (model.location != nullptr) {
    SerializeModel(*model.location, &(*root)["location"]);
  }

  if (!model.condition.empty()) {
    (*root)["condition"] = Json::Value(model.condition);
  }

  SerializeModel(model.expressions, "expressions", root);

  if (!model.log_message_format.empty()) {
    (*root)["logMessageFormat"] = Json::Value(model.log_message_format);
  }

  SerializeLogLevel(model.log_level, root);

  // "isFinalState" defaults to false, so we only need to include the
  // element when the value is true.
  if (model.is_final_state) {
    (*root)["isFinalState"] = Json::Value(model.is_final_state);
  }

  if (model.create_time != kUnspecifiedTimestamp) {
    SerializeTimestamp(model.create_time, &(*root)["createTime"]);
  }

  if (model.status != nullptr) {
    SerializeModel(*model.status, &(*root)["status"]);
  }

  SerializeModel(model.stack, "stackFrames", root);

  SerializeModel(model.evaluated_expressions, "evaluatedExpressions", root);

  SerializeModel(model.variable_table, "variableTable", root);

  if (!model.labels.empty()) {
    SerializeModel(model.labels, &(*root)["labels"]);
  }
}


template <>
std::unique_ptr<BreakpointModel> DeserializeModel<BreakpointModel>(
    const Json::Value& root) {
  std::unique_ptr<BreakpointModel> model(new BreakpointModel);

  // Breakpoint ID.
  model->id = JsonCppGetString(root, "id");
  if (model->id.empty()) {
    return nullptr;
  }

  // Breakpoint action.
  model->action = DeserializeBreakpointAction(root);

  // Location.
  model->location = DeserializeModel<SourceLocationModel>(root["location"]);
  if (model->location == nullptr) {
    return nullptr;
  }

  // Condition.
  model->condition = JsonCppGetString(root, "condition");

  // Watched expressions.
  if (!DeserializeModel(root["expressions"], &model->expressions)) {
    return nullptr;
  }

  // Log message format.
  model->log_message_format = JsonCppGetString(root, "logMessageFormat");

  // Log level.
  model->log_level = DeserializeLogLevel(root);

  // Final state flag.
  model->is_final_state = JsonCppGetBool(root, "isFinalState", false);

  // Breakpoint creation time.
  model->create_time = DeserializeTimestamp(root["createTime"]);

  // Breakpoint status.
  if (root.isMember("status")) {
    model->status = DeserializeModel<StatusMessageModel>(root["status"]);
  }

  // Stack frame.
  if (!DeserializeModel(root["stackFrames"], &model->stack)) {
    return nullptr;
  }

  // Results of watched expressions.
  if (!DeserializeModel(
        root["evaluatedExpressions"],
        &model->evaluated_expressions)) {
    return nullptr;
  }

  // Referenced objects dictionary.
  if (!DeserializeModel(
        root["variableTable"],
        &model->variable_table)) {
    return nullptr;
  }

  // Breakpoint labels.
  if (!DeserializeModel(root["labels"], &model->labels)) {
    return nullptr;
  }

  // evaluated_user_id field is output-only, and hence, it does not need
  // translation.
  // TODO: There are various other output-only fields that also
  // don't need translation. We should either remove them too, or add
  // evaluated_user_id here, in order to be symmetric.

  return model;
}


SerializedBreakpoint BreakpointToJson(const BreakpointModel& model) {
  Json::Value root;
  SerializeModel(model, &root);

  Json::FastWriter writer;
  return { "json", model.id, writer.write(root) };
}


SerializedBreakpoint BreakpointToPrettyJson(const BreakpointModel& model) {
  Json::Value root;
  SerializeModel(model, &root);

  Json::StyledWriter writer;
  return { "json", model.id, writer.write(root) };
}


std::unique_ptr<BreakpointModel> BreakpointFromJson(
    const SerializedBreakpoint& serialized_breakpoint) {
  if (serialized_breakpoint.format != "json") {
    DCHECK(false);
    return nullptr;
  }

  return BreakpointFromJsonString(serialized_breakpoint.data);
}

std::unique_ptr<BreakpointModel> BreakpointFromJsonString(
    const std::string& json_string) {
  Json::Value root;
  Json::Reader reader;
  if (!reader.parse(json_string, root)) {
    // TODO: print "reader.getFormattedErrorMessages()" when
    // external build upgrade to the latest version of JsonCpp.
    LOG(ERROR) << "JSON string could not be parsed";
    return nullptr;
  }

  return BreakpointFromJsonValue(root);
}

std::unique_ptr<BreakpointModel> BreakpointFromJsonValue(
    const Json::Value& json_value) {
  auto breakpoint = DeserializeModel<BreakpointModel>(json_value);
  if (breakpoint == nullptr) {
    LOG(ERROR) << "Failed to deserialize breakpoint from JSON string:\n"
               << Json::StyledWriter().write(json_value);
  }

  return breakpoint;
}


}  // namespace cdbg
}  // namespace devtools
