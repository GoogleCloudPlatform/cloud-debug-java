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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_MODEL_UTIL_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_MODEL_UTIL_H_

#include <cstdint>
#include <memory>
#include <numeric>

#include "jni_utils.h"
#include "messages.h"
#include "model.h"
#include "type_util.h"

namespace devtools {
namespace cdbg {

static constexpr TimestampModel kUnspecifiedTimestamp {};

// Helper classes to build Breakpoint related structure in code.

class TimestampBuilder {
 public:
  static TimestampModel Build(time_t seconds) {
    TimestampModel timestamp;
    timestamp.seconds = seconds;
    timestamp.nanos = 0;

    return timestamp;
  }

  static TimestampModel Build(int64_t seconds, int32_t nanos) {
    TimestampModel timestamp;
    timestamp.seconds = seconds;
    timestamp.nanos = nanos;

    return timestamp;
  }
};

class DurationBuilder {
 public:
  static DurationModel Build(time_t seconds) {
    DurationModel duration;
    duration.seconds = seconds;
    duration.nanos = 0;

    return duration;
  }

  static DurationModel Build(int64_t seconds, int32_t nanos) {
    DurationModel duration;
    duration.seconds = seconds;
    duration.nanos = nanos;

    return duration;
  }
};

class StatusMessageBuilder {
 public:
  typedef StatusMessageModel ResultType;

  StatusMessageBuilder() { }

  explicit StatusMessageBuilder(const StatusMessageModel& source) {
    data_->is_error = source.is_error;
    data_->refers_to = source.refers_to;
    data_->description.format = source.description.format;
    data_->description.parameters = source.description.parameters;
  }

  StatusMessageBuilder& set_error() {
    data_->is_error = true;
    return *this;
  }

  StatusMessageBuilder& set_info() {
    data_->is_error = false;
    return *this;
  }

  StatusMessageBuilder& set_refers_to(StatusMessageModel::Context refers_to) {
    data_->refers_to = refers_to;
    return *this;
  }

  StatusMessageBuilder& set_description(
      const FormatMessageModel& description) {
    data_->description = description;
    return *this;
  }

  StatusMessageBuilder& set_format(std::string format) {
    data_->description.format = std::move(format);
    return *this;
  }

  StatusMessageBuilder& set_parameters(std::vector<std::string> parameters) {
    data_->description.parameters.swap(parameters);
    return *this;
  }

  StatusMessageBuilder& clear_parameters() {
    data_->description.parameters.clear();
    return *this;
  }

  StatusMessageBuilder& add_parameter(std::string parameter) {
    data_->description.parameters.push_back(std::move(parameter));
    return *this;
  }

  std::unique_ptr<StatusMessageModel> build() {
    return std::move(data_);
  }

 private:
  std::unique_ptr<StatusMessageModel> data_ { new StatusMessageModel };

  DISALLOW_COPY_AND_ASSIGN(StatusMessageBuilder);
};

class SourceLocationBuilder {
 public:
  typedef SourceLocationModel ResultType;

  SourceLocationBuilder() { }

  SourceLocationBuilder(std::string path, int32_t line) {
    set_path(std::move(path));
    set_line(line);
  }

  explicit SourceLocationBuilder(const SourceLocationModel& source) {
    set_path(source.path);
    set_line(source.line);
  }

  SourceLocationBuilder& set_path(std::string path) {
    data_->path = std::move(path);
    return *this;
  }

  SourceLocationBuilder& set_line(int32_t line) {
    data_->line = line;
    return *this;
  }

  std::unique_ptr<SourceLocationModel> build() {
    return std::move(data_);
  }

 private:
  std::unique_ptr<SourceLocationModel> data_ { new SourceLocationModel };

  DISALLOW_COPY_AND_ASSIGN(SourceLocationBuilder);
};


class VariableBuilder {
 public:
  typedef VariableModel ResultType;

  VariableBuilder() { }

  explicit VariableBuilder(const VariableModel& source) {
    data_->name = source.name;
    data_->value = source.value;
    data_->type = source.type;
    data_->var_table_index = source.var_table_index;

    data_->members.reserve(source.members.size());
    for (const auto& member : source.members) {
      add_member(VariableBuilder(*member).build());
    }

    if (source.status != nullptr) {
      set_status(StatusMessageBuilder(*source.status).build());
    }
  }

  static std::unique_ptr<VariableModel> build_capture_buffer_full_variable() {
    return VariableBuilder()
        .set_status(StatusMessageBuilder()
            .set_error()
            .set_refers_to(StatusMessageModel::Context::VARIABLE_VALUE)
            .set_format(OutOfBufferSpace))
        .build();
  }

  VariableBuilder& set_name(std::string name) {
    data_->name = std::move(name);
    return *this;
  }

  VariableBuilder& set_value(std::string value) {
    data_->value = std::move(value);
    return *this;
  }

  VariableBuilder& clear_value() {
    data_->value.clear();
    return *this;
  }

  VariableBuilder& set_type(std::string type) {
    data_->type = std::move(type);
    return *this;
  }

  VariableBuilder& clear_type() {
    data_->type.clear();
    return *this;
  }

  VariableBuilder& set_var_table_index(int var_table_index) {
    data_->var_table_index = var_table_index;
    return *this;
  }

  VariableBuilder& clear_var_table_index() {
    data_->var_table_index.clear();
    return *this;
  }

  VariableBuilder& clear_members() {
    data_->members.clear();
    return *this;
  }

  VariableBuilder& add_member(VariableBuilder& member) {  // NOLINT
    return add_member(member.build());
  }

  VariableBuilder& add_member(
      std::unique_ptr<VariableModel> member) {
    data_->members.push_back(std::move(member));
    return *this;
  }

  VariableBuilder& clear_status() {
    return set_status(nullptr);
  }

  VariableBuilder& set_status(std::unique_ptr<StatusMessageModel> status) {
    data_->status = std::move(status);
    return *this;
  }

  VariableBuilder& set_status(StatusMessageBuilder& status) {  // NOLINT
    return set_status(status.build());
  }

  std::unique_ptr<VariableModel> build() {
    return std::move(data_);
  }

 private:
  std::unique_ptr<VariableModel> data_ { new VariableModel };

  DISALLOW_COPY_AND_ASSIGN(VariableBuilder);
};


class StackFrameBuilder {
 public:
  typedef StackFrameModel ResultType;

  StackFrameBuilder() { }

  explicit StackFrameBuilder(const StackFrameModel& source) {
    set_function(source.function);

    if (source.location != nullptr) {
      set_location(SourceLocationBuilder(*source.location).build());
    }

    data_->arguments.reserve(source.arguments.size());
    for (const auto& argument : source.arguments) {
      add_argument(VariableBuilder(*argument).build());
    }

    data_->locals.reserve(source.locals.size());
    for (const auto& local : source.locals) {
      add_local(VariableBuilder(*local).build());
    }
  }

  StackFrameBuilder& set_function(std::string function) {
    data_->function = std::move(function);
    return *this;
  }

  StackFrameBuilder& set_location(
      std::unique_ptr<SourceLocationModel> location) {
    data_->location = std::move(location);
    return *this;
  }

  StackFrameBuilder& set_location(std::string path, int32_t line) {
    return set_location(SourceLocationBuilder(path, line).build());
  }

  StackFrameBuilder& clear_location() {
    data_->location = nullptr;
    return *this;
  }

  StackFrameBuilder& clear_arguments() {
    data_->arguments.clear();
    return *this;
  }

  StackFrameBuilder& add_argument(VariableBuilder& argument) {  // NOLINT
    return add_argument(argument.build());
  }

  StackFrameBuilder& add_argument(
      std::unique_ptr<VariableModel> argument) {
    data_->arguments.push_back(std::move(argument));
    return *this;
  }

  StackFrameBuilder& clear_locals() {
    data_->locals.clear();
    return *this;
  }

  StackFrameBuilder& add_local(VariableBuilder& local) {  // NOLINT
    return add_local(local.build());
  }

  StackFrameBuilder& add_local(
      std::unique_ptr<VariableModel> local) {
    data_->locals.push_back(std::move(local));
    return *this;
  }

  std::unique_ptr<StackFrameModel> build() {
    return std::move(data_);
  }

 private:
  std::unique_ptr<StackFrameModel> data_ { new StackFrameModel };

  DISALLOW_COPY_AND_ASSIGN(StackFrameBuilder);
};


class UserIdBuilder {
 public:
  typedef UserIdModel ResultType;

  UserIdBuilder() { }

  explicit UserIdBuilder(const UserIdModel& source) {
    data_->kind = source.kind;
    data_->id = source.id;
  }

  UserIdBuilder& set_kind(std::string kind) {
    data_->kind = std::move(kind);
    return *this;
  }

  UserIdBuilder& set_id(std::string id) {
    data_->id = std::move(id);
    return *this;
  }

  std::unique_ptr<UserIdModel> build() {
    return std::move(data_);
  }

 private:
  std::unique_ptr<UserIdModel> data_ { new UserIdModel };

  DISALLOW_COPY_AND_ASSIGN(UserIdBuilder);
};


class BreakpointBuilder {
 public:
  typedef BreakpointModel ResultType;

  BreakpointBuilder() { }

  explicit BreakpointBuilder(const BreakpointModel& source) {
    set_id(source.id);
    set_is_canary(source.is_canary);
    set_action(source.action);

    if (source.location != nullptr) {
      set_location(SourceLocationBuilder(*source.location).build());
    }

    set_condition(source.condition);
    set_expressions(source.expressions);

    set_log_message_format(source.log_message_format);
    set_log_level(source.log_level);

    set_is_final_state(source.is_final_state);

    set_create_time(source.create_time);

    if (source.status != nullptr) {
      set_status(StatusMessageBuilder(*source.status).build());
    }

    data_->stack.reserve(source.stack.size());
    for (const auto& stack_frame : source.stack) {
      add_stack_frame(StackFrameBuilder(*stack_frame).build());
    }

    data_->evaluated_expressions.reserve(source.evaluated_expressions.size());
    for (const auto& evaluated_expression : source.evaluated_expressions) {
      add_evaluated_expression(VariableBuilder(*evaluated_expression).build());
    }

    data_->variable_table.reserve(source.variable_table.size());
    for (const auto& variable_table_item : source.variable_table) {
      add_variable_table_item(VariableBuilder(*variable_table_item).build());
    }

    set_labels(source.labels);
  }

  BreakpointBuilder& set_id(std::string id) {
    data_->id = std::move(id);
    return *this;
  }

  BreakpointBuilder& set_is_canary(bool is_canary) {
    data_->is_canary = is_canary;
    return *this;
  }

  BreakpointBuilder& set_action(BreakpointModel::Action action) {
    data_->action = action;
    return *this;
  }

  BreakpointBuilder& set_location(
      std::unique_ptr<SourceLocationModel> location) {
    data_->location = std::move(location);
    return *this;
  }

  BreakpointBuilder& set_location(std::string path, int32_t line) {
    return set_location(SourceLocationBuilder(path, line).build());
  }

  BreakpointBuilder& set_condition(std::string condition) {
    data_->condition = std::move(condition);
    return *this;
  }

  BreakpointBuilder& add_expression(std::string expression) {
    data_->expressions.push_back(std::move(expression));
    return *this;
  }

  BreakpointBuilder& set_expressions(std::vector<std::string> expressions) {
    data_->expressions.swap(expressions);
    return *this;
  }

  BreakpointBuilder& set_log_message_format(std::string log_message_format) {
    data_->log_message_format = std::move(log_message_format);
    return *this;
  }

  BreakpointBuilder& set_log_level(BreakpointModel::LogLevel log_level) {
    data_->log_level = log_level;
    return *this;
  }

  BreakpointBuilder& set_is_final_state(bool is_final_state) {
    data_->is_final_state = is_final_state;
    return *this;
  }

  BreakpointBuilder& set_create_time(TimestampModel timestamp) {
    data_->create_time = timestamp;
    return *this;
  }

  BreakpointBuilder& clear_status() {
    return set_status(nullptr);
  }

  BreakpointBuilder& set_status(std::unique_ptr<StatusMessageModel> status) {
    data_->status = std::move(status);
    return *this;
  }

  BreakpointBuilder& set_status(StatusMessageBuilder& status) {  // NOLINT
    return set_status(status.build());
  }

  BreakpointBuilder& clear_stack() {
    data_->stack.clear();
    return *this;
  }

  BreakpointBuilder& add_stack_frame(StackFrameBuilder& stack_frame) {  // NOLINT
    return add_stack_frame(stack_frame.build());
  }

  BreakpointBuilder& add_stack_frame(
      std::unique_ptr<StackFrameModel> stack_frame) {
    data_->stack.push_back(std::move(stack_frame));
    return *this;
  }

  BreakpointBuilder& clear_evaluated_expressions() {
    data_->evaluated_expressions.clear();
    return *this;
  }

  BreakpointBuilder& add_evaluated_expression(
      VariableBuilder& evaluated_expression) {  // NOLINT
    return add_evaluated_expression(evaluated_expression.build());
  }

  BreakpointBuilder& add_evaluated_expression(
      std::unique_ptr<VariableModel> evaluated_expression) {
    data_->evaluated_expressions.push_back(std::move(evaluated_expression));
    return *this;
  }

  BreakpointBuilder& clear_variable_table() {
    data_->variable_table.clear();
    return *this;
  }

  BreakpointBuilder& add_variable_table_item(VariableBuilder& item) {  // NOLINT
    return add_variable_table_item(item.build());
  }

  BreakpointBuilder& add_variable_table_item(
      std::unique_ptr<VariableModel> item) {
    data_->variable_table.push_back(std::move(item));
    return *this;
  }

  BreakpointBuilder& add_capture_buffer_full_variable_table_item() {
    return add_variable_table_item(
        VariableBuilder::build_capture_buffer_full_variable());
  }

  BreakpointBuilder& clear_labels() {
    data_->labels.clear();
    return *this;
  }

  BreakpointBuilder& set_labels(std::map<std::string, std::string> labels) {
    data_->labels = std::move(labels);
    return *this;
  }

  BreakpointBuilder& add_label(const std::string& key,
                               const std::string& value) {
    data_->labels[key] = value;
    return *this;
  }

  BreakpointBuilder& set_evaluated_user_id(
      std::unique_ptr<UserIdModel> evaluated_user_id) {
    data_->evaluated_user_id = std::move(evaluated_user_id);
    return *this;
  }

  BreakpointBuilder& set_evaluated_user_id(
      UserIdBuilder& evaluated_user_id) {  // NOLINT
    return set_evaluated_user_id(evaluated_user_id.build());
  }

  BreakpointBuilder& set_expires_in(DurationModel expires_in) {
    data_->expires_in.reset(new DurationModel);
    *data_->expires_in = expires_in;
    return *this;
  }

  std::unique_ptr<BreakpointModel> build() {
    return std::move(data_);
  }

 private:
  std::unique_ptr<BreakpointModel> data_ { new BreakpointModel };

  DISALLOW_COPY_AND_ASSIGN(BreakpointBuilder);
};


// Stores either data or an error message in case of an error.
template <typename T>
class ErrorOr {
 public:
  // Default constructor to initialize to default value with no error.
  ErrorOr() : is_error_(false) {
  }

  // Deliberately implicit constructor.
  ErrorOr(T value)  // NOLINT
      : is_error_(false),
        value_(std::move(value)) {
  }

  // Deliberately implicit constructor.
  ErrorOr(FormatMessageModel error_message)  // NOLINT
      : is_error_(true),
        error_message_(std::move(error_message)) {
  }

  static T detach_value(ErrorOr data) {
    return std::move(data.value_);
  }

  bool is_error() const { return is_error_; }

  const T& value() const {
    DCHECK(!is_error_);
    return value_;
  }

  T& value() {
    DCHECK(!is_error_);
    return value_;
  }

  const FormatMessageModel& error_message() const {
    return error_message_;
  }

 private:
  // Selects between "value_" and "error_message_".
  bool is_error_;

  // Stored data in case there is no error.
  T value_;

  // Error message if "is_error_ == true".
  FormatMessageModel error_message_;
};


inline bool operator== (const TimestampModel& t1, const TimestampModel& t2) {
  return (t1.seconds == t2.seconds) && (t1.nanos == t2.nanos);
}


inline bool operator!= (const TimestampModel& t1, const TimestampModel& t2) {
  return !(t1 == t2);
}

inline bool operator== (const DurationModel& d1, const DurationModel& d2) {
  return (d1.seconds == d2.seconds) && (d1.nanos == d2.nanos);
}


inline bool operator!= (const DurationModel& d1, const DurationModel& d2) {
  return !(d1 == d2);
}

// Debug printer for error messages.
inline std::ostream& operator<< (
    std::ostream& os,
    const FormatMessageModel& message) {
  // Template.
  os << "(\"" << message.format << '"';

  // Parameters (if any).
  for (const std::string& parameter : message.parameters) {
    os << ", \"" << parameter << '"';
  }

  os << ')';

  return os;
}


// Debug printer for status messages.
inline std::ostream& operator<< (
    std::ostream& os,
    const StatusMessageModel& message) {
  if (message.is_error) {
    os << "error";
  } else {
    os << "info";
  }

  os << '(' << static_cast<int>(message.refers_to) << ") "
     << message.description;

  return os;
}

inline std::ostream& operator<< (
    std::ostream& os,
    const std::unique_ptr<StatusMessageModel>& message) {
  if (message == nullptr) {
    os << "null";
    return os;
  }

  os << *message;
  return os;
}


// Debug printer for user id.
inline std::ostream& operator<< (
    std::ostream& os,
    const UserIdModel& message) {
  os << message.kind << ":" << message.id;
  return os;
}


inline std::ostream& operator<< (
    std::ostream& os,
    const std::unique_ptr<UserIdModel>& message) {
  if (message == nullptr) {
    os << "null";
    return os;
  }

  os << *message;
  return os;
}

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_MODEL_UTIL_H_


