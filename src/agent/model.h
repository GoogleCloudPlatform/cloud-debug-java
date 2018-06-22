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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_MODEL_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_MODEL_H_

#include <vector>
#include <map>
#include <memory>
#include "nullable.h"
#include "common.h"

namespace devtools {
namespace cdbg {

// See google/protobuf/timestamp.proto for explanation of this structure.
struct TimestampModel {
  int64 seconds = 0;
  int32 nanos = 0;
};

// See google/protobuf/duration.proto for explanation of this structure.
struct DurationModel {
  int64 seconds = 0;
  int32 nanos = 0;
};

struct FormatMessageModel {
  string format;
  std::vector<string> parameters;

  bool operator== (const FormatMessageModel& other) const {
    return (format == other.format) &&
           (parameters == other.parameters);
  }
};

struct StatusMessageModel {
  enum class Context {
    UNSPECIFIED = 0,
    BREAKPOINT_SOURCE_LOCATION = 3,
    BREAKPOINT_CONDITION = 4,
    BREAKPOINT_EXPRESSION = 7,
    BREAKPOINT_AGE = 8,
    VARIABLE_NAME = 5,
    VARIABLE_VALUE = 6
  };

  bool is_error = false;
  Context refers_to = Context::UNSPECIFIED;
  FormatMessageModel description;

  bool operator== (const StatusMessageModel& other) const {
    return (is_error == other.is_error) &&
           (refers_to == other.refers_to) &&
           (description == other.description);
  }

  string RefersToToString() const {
    switch (refers_to) {
      case Context::UNSPECIFIED : return "UNSPECIFIED";
      case Context::BREAKPOINT_SOURCE_LOCATION :
        return "BREAKPOINT_SOURCE_LOCATION";
      case Context::BREAKPOINT_CONDITION : return "BREAKPOINT_CONDITION";
      case Context::BREAKPOINT_EXPRESSION : return "BREAKPOINT_EXPRESSION";
      case Context::BREAKPOINT_AGE : return "BREAKPOINT_AGE";
      case Context::VARIABLE_NAME : return "VARIABLE_NAME";
      case Context::VARIABLE_VALUE : return "VARIABLE_VALUE";
      default : return "UNKNOWN";
    }
  }
};

struct SourceLocationModel {
  string path;
  int32 line = -1;
};

struct VariableModel {
  string name;
  Nullable<string> value;
  string type;
  Nullable<uint64> var_table_index;
  std::vector<std::unique_ptr<VariableModel>> members;
  std::unique_ptr<StatusMessageModel> status;
};

struct StackFrameModel {
  string function;
  std::unique_ptr<SourceLocationModel> location;
  std::vector<std::unique_ptr<VariableModel>> arguments;
  std::vector<std::unique_ptr<VariableModel>> locals;
};

struct UserIdModel {
  string kind;
  string id;
};

struct BreakpointModel {
  enum class Action {
    CAPTURE = 0,
    LOG = 1
  };

  enum class LogLevel {
    INFO = 0,  // The serialization code assumes default log level is INFO.
    WARNING = 1,
    ERROR = 2
  };

  string ActionToString() const {
    switch (action) {
      case Action::CAPTURE : return "CAPTURE";
      case Action::LOG : return "LOG";
      default : return "UNKNOWN";
    }
  }

  string id;
  bool is_canary = false;
  Action action = Action::CAPTURE;
  std::unique_ptr<SourceLocationModel> location;
  string condition;
  std::vector<string> expressions;
  string log_message_format;
  LogLevel log_level = LogLevel::INFO;
  bool is_final_state = false;
  TimestampModel create_time;
  std::unique_ptr<StatusMessageModel> status;
  std::vector<std::unique_ptr<StackFrameModel>> stack;
  std::vector<std::unique_ptr<VariableModel>> evaluated_expressions;
  std::vector<std::unique_ptr<VariableModel>> variable_table;
  std::map<string, string> labels;
  std::unique_ptr<UserIdModel> evaluated_user_id;
  std::unique_ptr<DurationModel> expires_in;
};

// BreakpointModel in serialized form that we send to the Java code. The
// format is either ProtoBuf or JSON depending on the build and configuration.
struct SerializedBreakpoint {
  // Either "v2proto" or "json".
  string format;

  // Breakpoint ID is encoded somewhere in "data", but it is hard to get. We
  // pass it around so that we don't need to deserialize the entire breakpoint
  // to get it.
  string id;

  // If format is "v2proto", data is protobuf message serialized to byte
  // array.
  // If format is "json", data is UTF-8 encoded JSON string.
  string data;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_MODEL_H_


