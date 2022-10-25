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

#include "src/agent/model_json.h"

#include "gtest/gtest.h"
#include "jni_proxy_api_client_datetime.h"
#include "src/agent/model_util.h"
#include "tests/agent/fake_jni.h"
#include "tests/agent/mock_jvmti_env.h"

namespace devtools {
namespace cdbg {

class ModelJsonTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(jniproxy::BindDateTime());
  }

  void TearDown() override {
    jniproxy::CleanupDateTime();
  }
};


template <typename T>
static bool IsEqual(
    const std::vector<std::unique_ptr<T>>& arr1,
    const std::vector<std::unique_ptr<T>>& arr2) {
  if (arr1.size() != arr2.size()) {
    return false;
  }

  for (int i = 0; i < arr1.size(); ++i) {
    if (!IsEqual(arr1[i], arr2[i])) {
      return false;
    }
  }

  return true;
}


template <typename T>
static bool IsEqual(
    const std::unique_ptr<T>& model1,
    const std::unique_ptr<T>& model2) {
  if ((model1 == nullptr) != (model2 == nullptr)) {
    return false;
  }

  if (model1 != nullptr) {
    if (!IsEqual(*model1, *model2)) {
      return false;
    }
  }

  return true;
}


static bool IsEqual(
    const FormatMessageModel& model1,
    const FormatMessageModel& model2) {
  return (model1.format == model2.format) &&
         (model1.parameters == model2.parameters);
}


static bool IsEqual(
    const StatusMessageModel& model1,
    const StatusMessageModel& model2) {
  return (model1.is_error == model2.is_error) &&
         (model1.refers_to == model2.refers_to) &&
         IsEqual(model1.description, model2.description);
}


static bool IsEqual(
    const SourceLocationModel& model1,
    const SourceLocationModel& model2) {
  return (model1.path == model2.path) &&
         (model1.line == model2.line);
}


static bool IsEqual(
    const VariableModel& model1,
    const VariableModel& model2) {
  return (model1.name == model2.name) &&
         (model1.value == model2.value) &&
         (model1.type == model2.type) &&
         (model1.var_table_index == model2.var_table_index) &&
         IsEqual(model1.members, model2.members) &&
         IsEqual(model1.status, model2.status);
}


static bool IsEqual(
    const StackFrameModel& model1,
    const StackFrameModel& model2) {
  return (model1.function == model2.function) &&
         IsEqual(model1.location, model2.location) &&
         IsEqual(model1.arguments, model2.arguments) &&
         IsEqual(model1.locals, model2.locals);
}


static bool IsEqual(
    const BreakpointModel& model1,
    const BreakpointModel& model2) {
  // Note that output-only evaluated_user_id field is not used when comparing
  // the two objects.
  // TODO: Either ignore all output-only fields, or include
  // evaluated_user_id in the comparison.
  return (model1.id == model2.id) &&
         (model1.action == model2.action) &&
         IsEqual(model1.location, model2.location) &&
         (model1.condition == model2.condition) &&
         (model1.expressions == model2.expressions) &&
         (model1.log_message_format == model2.log_message_format) &&
         (model1.log_level == model2.log_level) &&
         (model1.is_final_state == model2.is_final_state) &&
         (model1.create_time == model2.create_time) &&
         IsEqual(model1.status, model2.status) &&
         IsEqual(model1.stack, model2.stack) &&
         IsEqual(model1.evaluated_expressions, model2.evaluated_expressions) &&
         IsEqual(model1.variable_table, model2.variable_table) &&
         (model1.labels == model2.labels);
}

static std::unique_ptr<SourceLocationModel> CreateSourceLocation(
    std::string path, int line) {
  std::unique_ptr<SourceLocationModel> model(new SourceLocationModel);
  model->path = std::move(path);
  model->line = line;

  return model;
}

static std::unique_ptr<BreakpointModel> CreateFullBreakpoint() {
  std::unique_ptr<BreakpointModel> breakpoint(new BreakpointModel);
  breakpoint->id = "id";
  breakpoint->location = CreateSourceLocation("this/is/a/path.java", 34957834);
  breakpoint->condition = "condition";
  breakpoint->expressions = { "expr1", "expr2", "expr3" };
  breakpoint->is_final_state = true;

  breakpoint->stack.push_back(
      std::unique_ptr<StackFrameModel>(new StackFrameModel {
          "func1",
          CreateSourceLocation("func1.java", 564345),
          { },
          { }
      }));
  breakpoint->stack.push_back(
      std::unique_ptr<StackFrameModel>(new StackFrameModel {
          "func2",
          CreateSourceLocation("func2.java", 903487),
          { },
          { }
      }));

  breakpoint->variable_table.push_back(
      std::unique_ptr<VariableModel>(new VariableModel));
  breakpoint->variable_table[0]->name = "named";

  breakpoint->variable_table.push_back(
      std::unique_ptr<VariableModel>(new VariableModel));
  breakpoint->variable_table[1]->value = "valued";
  breakpoint->variable_table[1]->type = "typed";

  breakpoint->variable_table.push_back(
      std::unique_ptr<VariableModel>(new VariableModel));
  breakpoint->variable_table[2]->var_table_index = 4345;

  breakpoint->variable_table.push_back(
      std::unique_ptr<VariableModel>(new VariableModel));
  breakpoint->variable_table[3]->members.push_back(
      std::unique_ptr<VariableModel>(new VariableModel));
  breakpoint->variable_table[3]->members[0]->name = "myname";

  breakpoint->variable_table.push_back(
      std::unique_ptr<VariableModel>(new VariableModel));

  breakpoint->labels["first"] = "one";
  breakpoint->labels["second"] = "two";
  breakpoint->labels["third"] = "three";

  return breakpoint;
}


static void SerializationLoop(const BreakpointModel& breakpoint) {
  EXPECT_TRUE(IsEqual(breakpoint, breakpoint));  // Self-test.

  std::string pretty_json = BreakpointToPrettyJson(breakpoint).data;
  std::unique_ptr<BreakpointModel> pretty_breakpoint =
      BreakpointFromJsonString(pretty_json);
  ASSERT_TRUE(pretty_breakpoint != nullptr);
  EXPECT_TRUE(IsEqual(breakpoint, *pretty_breakpoint))
      << "Pretty JSON:\n" << pretty_json;

  std::string fast_json = BreakpointToJson(breakpoint).data;
  std::unique_ptr<BreakpointModel> fast_breakpoint =
      BreakpointFromJsonString(fast_json);
  ASSERT_TRUE(pretty_breakpoint != nullptr);
  EXPECT_TRUE(IsEqual(breakpoint, *fast_breakpoint))
      << "Fast JSON:\n" << fast_json;
}


TEST_F(ModelJsonTest, format) {
  EXPECT_EQ("json", BreakpointToJson(BreakpointModel()).format);
}


TEST_F(ModelJsonTest, FullBreakpoint) {
  SerializationLoop(*CreateFullBreakpoint());
}


TEST_F(ModelJsonTest, EmptyStack) {
  std::unique_ptr<BreakpointModel> breakpoint = CreateFullBreakpoint();
  breakpoint->stack.clear();

  SerializationLoop(*breakpoint);
}


TEST_F(ModelJsonTest, EmptyId) {
  std::unique_ptr<BreakpointModel> breakpoint = CreateFullBreakpoint();
  breakpoint->id.clear();

  std::string pretty_json = BreakpointToPrettyJson(*breakpoint).data;
  EXPECT_TRUE(BreakpointFromJsonString(pretty_json) == nullptr);

  std::string fast_json = BreakpointToJson(*breakpoint).data;
  EXPECT_TRUE(BreakpointFromJsonString(pretty_json) == nullptr);
}


TEST_F(ModelJsonTest, EmptyExpressions) {
  std::unique_ptr<BreakpointModel> breakpoint = CreateFullBreakpoint();
  breakpoint->expressions.clear();

  SerializationLoop(*breakpoint);
}


TEST_F(ModelJsonTest, BreakpointLabels) {
  std::unique_ptr<BreakpointModel> breakpoint = BreakpointBuilder()
      .set_id("id")
      .add_label("key1", "value1")
      .add_label("key2", "value2")
      .build();

  EXPECT_EQ(
      "{\"id\":\"id\",\"labels\":{\"key1\":\"value1\",\"key2\":\"value2\"}}\n",
      BreakpointToJson(*breakpoint).data);
}


TEST_F(ModelJsonTest, EmptyBreakpointLabels) {
  std::unique_ptr<BreakpointModel> breakpoint = CreateFullBreakpoint();
  breakpoint->labels.clear();

  SerializationLoop(*breakpoint);
}


TEST_F(ModelJsonTest, StatusMessage_Empty) {
  std::unique_ptr<BreakpointModel> breakpoint = CreateFullBreakpoint();
  breakpoint->status.reset(new StatusMessageModel());

  SerializationLoop(*breakpoint);
}


TEST_F(ModelJsonTest, StatusMessage_NoParameters) {
  const StatusMessageModel::Context contexts[] = {
    StatusMessageModel::Context::UNSPECIFIED,
    StatusMessageModel::Context::BREAKPOINT_SOURCE_LOCATION,
    StatusMessageModel::Context::BREAKPOINT_CONDITION,
    StatusMessageModel::Context::BREAKPOINT_EXPRESSION,
    StatusMessageModel::Context::VARIABLE_NAME,
    StatusMessageModel::Context::VARIABLE_VALUE
  };

  for (StatusMessageModel::Context context : contexts) {
    std::unique_ptr<BreakpointModel> breakpoint = CreateFullBreakpoint();

    breakpoint->status.reset(new StatusMessageModel());
    breakpoint->status->is_error = true;
    breakpoint->status->refers_to = context;
    breakpoint->status->description.format = "bad condition";

    SerializationLoop(*breakpoint);
  }
}


TEST_F(ModelJsonTest, StatusMessage_Full) {
  std::unique_ptr<BreakpointModel> breakpoint = CreateFullBreakpoint();

  breakpoint->status.reset(new StatusMessageModel());
  breakpoint->status->is_error = false;
  breakpoint->status->refers_to = StatusMessageModel::Context::VARIABLE_NAME;
  breakpoint->status->description.format =
      "$0 is a bad variable because $1 and $2";
  breakpoint->status->description.parameters =
      { "fish", "some reason", "just excuse" };

  SerializationLoop(*breakpoint);
}


TEST_F(ModelJsonTest, VariableStatusMessage) {
  std::unique_ptr<StatusMessageModel> status(new StatusMessageModel());
  status->is_error = true;
  status->description.format = "variable doesn't work";

  std::unique_ptr<BreakpointModel> breakpoint = CreateFullBreakpoint();
  breakpoint->variable_table[4]->status = std::move(status);

  SerializationLoop(*breakpoint);
}


TEST_F(ModelJsonTest, BreakpointAction) {
  std::unique_ptr<BreakpointModel> breakpoint = CreateFullBreakpoint();

  breakpoint->action = BreakpointModel::Action::CAPTURE;
  SerializationLoop(*breakpoint);

  breakpoint->action = BreakpointModel::Action::LOG;
  SerializationLoop(*breakpoint);
}


TEST_F(ModelJsonTest, BreakpointLogMessageFormat) {
  std::unique_ptr<BreakpointModel> breakpoint = CreateFullBreakpoint();

  breakpoint->log_message_format = "a = $0, b = $1";
  SerializationLoop(*breakpoint);
}


TEST_F(ModelJsonTest, BreakpointLogLevel) {
  std::unique_ptr<BreakpointModel> breakpoint = CreateFullBreakpoint();

  breakpoint->log_level = BreakpointModel::LogLevel::INFO;
  SerializationLoop(*breakpoint);

  breakpoint->log_level = BreakpointModel::LogLevel::WARNING;
  SerializationLoop(*breakpoint);

  breakpoint->log_level = BreakpointModel::LogLevel::ERROR;
  SerializationLoop(*breakpoint);
}


TEST_F(ModelJsonTest, BreakpointCreateTime) {
  std::unique_ptr<BreakpointModel> breakpoint = CreateFullBreakpoint();

  breakpoint->create_time.seconds = 1444163838L;
  breakpoint->create_time.nanos = 893000000;  // Rfc3339 timestamp parser only
                                              // supports millisecond precision.
  SerializationLoop(*breakpoint);

  breakpoint->create_time.seconds = 3489578;
  breakpoint->create_time.nanos = TimestampModel().nanos;
  SerializationLoop(*breakpoint);
}


TEST_F(ModelJsonTest, UserId_Empty) {
  std::unique_ptr<BreakpointModel> breakpoint = CreateFullBreakpoint();
  breakpoint->evaluated_user_id.reset(new UserIdModel());

  SerializationLoop(*breakpoint);
}


TEST_F(ModelJsonTest, UserId_Full) {
  std::unique_ptr<BreakpointModel> breakpoint = CreateFullBreakpoint();

  breakpoint->evaluated_user_id.reset(new UserIdModel());
  breakpoint->evaluated_user_id->kind = "test_user";
  breakpoint->evaluated_user_id->id = "12345";

  SerializationLoop(*breakpoint);

  // Also verify that evaluated_user_id field is not serialized to JSON.
  std::string json_breakpoint = BreakpointToJson(*breakpoint).data;
  EXPECT_EQ(std::string::npos, json_breakpoint.find("evaluated_user_id"))
      << "Unexpected JSON data: " << json_breakpoint;
}

}  // namespace cdbg
}  // namespace devtools
