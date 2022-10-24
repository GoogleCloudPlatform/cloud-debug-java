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

#include "src/agent/model_util.h"

#include "gmock/gmock.h"

#include "jni_proxy_api_client_datetime.h"
#include "src/agent/test_util/json_eq_matcher.h"
#include "src/agent/test_util/mock_jvmti_env.h"
#include "src/agent/test_util/fake_jni.h"

namespace devtools {
namespace cdbg {

class ModelUtilTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(jniproxy::BindDateTime());
  }

  void TearDown() override {
    jniproxy::CleanupDateTime();
  }
};


static void CheckBuilder(
    const char* expected_json_string,
    BreakpointBuilder& builder) {  // NOLINT
  auto model = builder.build();

  // Check object that the builder emits.
  ExpectJsonEq(
      expected_json_string,
      BreakpointToPrettyJson(*model).data);

  // Check clone functionality of all the builders.
  ExpectJsonEq(
      expected_json_string,
      BreakpointToPrettyJson(*BreakpointBuilder(*model).build()).data);
}


TEST_F(ModelUtilTest, TimestampComparison) {
  EXPECT_TRUE(
      TimestampBuilder::Build(12345678987654321L, 12121212) ==
      TimestampBuilder::Build(12345678987654321L, 12121212));

  EXPECT_FALSE(
      TimestampBuilder::Build(12345678987654321L, 12121212) !=
      TimestampBuilder::Build(12345678987654321L, 12121212));

  EXPECT_FALSE(
      TimestampBuilder::Build(12345678987654321L, 12121213) ==
      TimestampBuilder::Build(12345678987654321L, 12121212));

  EXPECT_FALSE(
      TimestampBuilder::Build(12345678987654322L, 12121212) ==
      TimestampBuilder::Build(12345678987654321L, 12121212));
}


TEST_F(ModelUtilTest, Empty) {
  CheckBuilder(
      "{ 'id': 'abcdefgh' }",
      BreakpointBuilder()
          .set_id("abcdefgh"));
}


TEST_F(ModelUtilTest, BreakpointLocation) {
  CheckBuilder(
      "{ 'id': 'A',"
      "  'location': { 'path': 'ln', 'line': 23 } }",
      BreakpointBuilder()
          .set_id("A")
          .set_location("ln", 23));
}


TEST_F(ModelUtilTest, Variable) {
  auto some_var = VariableBuilder()
      .set_name("dog")
      .set_value("labrador")
      .set_type("dog")
      .set_var_table_index(43)
      .add_member(VariableBuilder()
          .set_name("cat1")
          .set_value("maine coon")
          .set_type("cat"))
      .add_member(VariableBuilder()
          .set_name("cat2")
          .set_value("ragdoll"))
      .build();

  CheckBuilder(
      "{ 'id': 'A',"
      "  'evaluatedExpressions': ["
      "    { 'name': 'dog',"
      "      'value': 'labrador',"
      "      'type': 'dog',"
      "      'varTableIndex': 43,"
      "      'members' : [  "
      "        { 'name': 'cat1', 'value': 'maine coon', 'type': 'cat' },  "
      "        { 'name': 'cat2', 'value': 'ragdoll' }  "
      "      ]"
      "    }"
      "]  }",
      BreakpointBuilder()
          .set_id("A")
          .add_evaluated_expression(
              VariableBuilder(*some_var).build()));

  CheckBuilder(
      "{ 'id': 'A',"
      "  'evaluatedExpressions': ["
      "    { 'name': 'dog'"
      "    }"
      "]  }",
      BreakpointBuilder()
          .set_id("A")
          .add_evaluated_expression(
              VariableBuilder(*some_var)
                  .clear_value()
                  .clear_type()
                  .clear_var_table_index()
                  .clear_members()
                  .build()));

  CheckBuilder(
      "{ 'id': 'A',"
      "  'evaluatedExpressions': ["
      "    { 'name': 'dog',"
      "      'value': 'labrador',"
      "      'type': 'dog',"
      "      'varTableIndex': 43,"
      "      'members' : [  "
      "        { 'name': 'cat1', 'value': 'maine coon', 'type': 'cat' },  "
      "        { 'name': 'cat2', 'value': 'ragdoll' }  "
      "      ],"
      "      'status' : {"
      "        'description' : {"
      "          'format' : '$0 is not $1',"
      "          'parameters' : [ 'apple', 'orange' ]"
      "        },"
      "        'isError' : true,"
      "        'refersTo' : 'VARIABLE_VALUE'"
      "      }"
      "    }"
      "]  }",
      BreakpointBuilder()
          .set_id("A")
          .add_evaluated_expression(VariableBuilder(*some_var)
              .set_status(StatusMessageBuilder()
                  .set_error()
                  .set_refers_to(StatusMessageModel::Context::VARIABLE_VALUE)
                  .set_format("$0 is not $1")
                  .set_parameters({ "apple", "orange" })
                  .build())
              .build()));
}


TEST_F(ModelUtilTest, StackFrame) {
  auto some_frame = StackFrameBuilder()
      .set_function("foo")
      .set_location("myfile", 221)
      .add_local(VariableBuilder().set_name("a").build())
      .add_local(VariableBuilder().set_name("b").build())
      .add_argument(VariableBuilder().set_name("c").build())
      .build();

  CheckBuilder(
      "{ 'id': 'A',"
      "  'stackFrames': ["
      "    { 'function': 'foo',"
      "      'location': { 'path': 'myfile', 'line': 221 },"
      "      'locals' : [  "
      "        { 'name': 'a' },  "
      "        { 'name': 'b' }  "
      "      ],"
      "      'arguments' : [  "
      "        { 'name': 'c' }  "
      "      ]"
      "    }"
      "]  }",
      BreakpointBuilder()
          .set_id("A")
          .add_stack_frame(
              StackFrameBuilder(*some_frame).build()));

  CheckBuilder(
      "{ 'id': 'A',"
      "  'stackFrames': ["
      "    { 'function': 'foo'"
      "    }"
      "]  }",
      BreakpointBuilder()
          .set_id("A")
          .add_stack_frame(
              StackFrameBuilder(*some_frame)
                  .clear_location()
                  .clear_locals()
                  .clear_arguments()
                  .build()));
}


TEST_F(ModelUtilTest, Breakpoint) {
  auto some_breakpoint = BreakpointBuilder()
      .set_id("A")
      .set_location("my file", 633)
      .set_condition("a>b")
      .set_expressions({ "i", "j", "k" })
      .set_is_final_state(true)
      .add_stack_frame(StackFrameBuilder()
          .set_function("f1"))
      .add_stack_frame(StackFrameBuilder()
          .set_function("f2"))
      .add_stack_frame(StackFrameBuilder()
          .set_function("f3"))
      .add_evaluated_expression(VariableBuilder()
          .set_name("w1"))
      .add_evaluated_expression(VariableBuilder()
          .set_name("w2"))
      .add_capture_buffer_full_variable_table_item()
      .add_variable_table_item(VariableBuilder()
          .set_name("v1"))
      .add_variable_table_item(VariableBuilder()
          .set_name("v2"))
      .build();

  CheckBuilder(
      "{"
      "   'condition' : 'a>b',"
      "   'evaluatedExpressions' : ["
      "      {"
      "         'name' : 'w1'"
      "      },"
      "      {"
      "         'name' : 'w2'"
      "      }"
      "   ],"
      "   'expressions' : [ 'i', 'j', 'k' ],"
      "   'id' : 'A',"
      "   'isFinalState' : true,"
      "   'location' : {"
      "      'line' : 633,"
      "      'path' : 'my file'"
      "   },"
      "   'stackFrames' : ["
      "      {"
      "         'function' : 'f1'"
      "      },"
      "      {"
      "         'function' : 'f2'"
      "      },"
      "      {"
      "         'function' : 'f3'"
      "      }"
      "   ],"
      "   'variableTable' : ["
      "      {"
      "         'status' : {"
      "               'description' : { 'format' : 'Buffer full. Use an "
      "expression to see more data' },"
      "               'isError' : true,"
      "               'refersTo' : 'VARIABLE_VALUE'"
      "         }"
      "      },"
      "      {"
      "         'name' : 'v1'"
      "      },"
      "      {"
      "         'name' : 'v2'"
      "      }"
      "   ]"
      "}",
      BreakpointBuilder(*some_breakpoint).set_id("A"));

  CheckBuilder(
      "{"
      "   'id' : 'A',"
      "   'isFinalState' : true,"
      "   'location' : {"
      "      'line' : 633,"
      "      'path' : 'my file'"
      "   }"
      "}",
      BreakpointBuilder(*some_breakpoint)
          .set_condition(std::string())
          .set_expressions({})
          .clear_stack()
          .clear_evaluated_expressions()
          .clear_variable_table());
}


TEST_F(ModelUtilTest, BreakpointMessage) {
  auto base_status = StatusMessageBuilder()
      .set_error()
      .set_refers_to(StatusMessageModel::Context::VARIABLE_NAME)
      .set_format("$0 > $1")
      .set_parameters({ "elephant", "mouse" })
      .build();

  auto some_breakpoint = BreakpointBuilder()
      .set_id("A")
      .set_status(StatusMessageBuilder(*base_status)
          .set_format("$0 is much bigger than $1")
          .build())
      .build();

  CheckBuilder(
      "{"
      "   'id' : 'B',"
      "   'status' : {"
      "     'isError' : true,"
      "     'refersTo' : 'VARIABLE_NAME',"
      "     'description' : {"
      "       'format' : '$0 is much bigger than $1',"
      "       'parameters' : [ 'elephant', 'mouse' ]"
      "     }"
      "   }"
      "}",
      BreakpointBuilder(*some_breakpoint)
          .set_id("B"));

  CheckBuilder(
      "{"
      "   'id' : 'A'"
      "}",
      BreakpointBuilder(*some_breakpoint)
          .clear_status());
}


TEST_F(ModelUtilTest, BreakpointAction) {
  EXPECT_EQ(
      BreakpointModel::Action::CAPTURE,
      BreakpointBuilder().build()->action);

  EXPECT_EQ(
      BreakpointModel::Action::CAPTURE,
      BreakpointBuilder()
          .set_action(BreakpointModel::Action::CAPTURE)
          .build()->action);

  BreakpointBuilder capture_breakpoint;
  capture_breakpoint.set_id("A").set_action(BreakpointModel::Action::CAPTURE);

  BreakpointBuilder log_breakpoint;
  log_breakpoint.set_id("A").set_action(BreakpointModel::Action::LOG);

  CheckBuilder(
      "{"
      "   'id' : 'A'"
      "}",
      capture_breakpoint);

  CheckBuilder(
      "{"
      "   'id' : 'A',"
      "   'action' : 'LOG'"
      "}",
      log_breakpoint);
}


TEST_F(ModelUtilTest, BreakpointLogMessageFormat) {
  CheckBuilder(
      "{"
      "   'id' : 'A',"
      "   'logMessageFormat' : 'a=$0'"
      "}",
      BreakpointBuilder().set_id("A").set_log_message_format("a=$0"));
}


TEST_F(ModelUtilTest, BreakpointLogLevel) {
  CheckBuilder(
      "{"
      "   'id' : 'A'"
      "}",
      BreakpointBuilder()
          .set_id("A")
          .set_log_level(BreakpointModel::LogLevel::INFO));

  CheckBuilder(
      "{"
      "   'id' : 'A',"
      "   'logLevel' : 'WARNING'"
      "}",
      BreakpointBuilder()
          .set_id("A")
          .set_log_level(BreakpointModel::LogLevel::WARNING));

  CheckBuilder(
      "{"
      "   'id' : 'A',"
      "   'logLevel' : 'ERROR'"
      "}",
      BreakpointBuilder()
          .set_id("A")
          .set_log_level(BreakpointModel::LogLevel::ERROR));
}


TEST_F(ModelUtilTest, BreakpointCreateTime) {
  TimestampModel test_timestamp;
  test_timestamp.seconds = 1444163838L;
  test_timestamp.nanos = 123456789;  // Internal precision is milliseconds.
                                     // Will be truncated to '123'.
  CheckBuilder(
      "{"
      "   'id' : 'A'"
      "}",
      BreakpointBuilder().set_id("A").set_create_time(kUnspecifiedTimestamp));

  CheckBuilder(
      "{"
      "   'id' : 'A',"
      "   'createTime' : '2015-10-06T20:37:18.123Z'"
      "}",
      BreakpointBuilder().set_id("A").set_create_time(test_timestamp));
}


TEST_F(ModelUtilTest, SetBreakpointLabels) {
  std::map<std::string, std::string> labels;
  labels["key1"] = "value1";
  labels["key2"] = "value2";

  std::unique_ptr<BreakpointModel> breakpoint = BreakpointBuilder()
      .set_labels(labels)
      .build();

  EXPECT_EQ(labels, breakpoint->labels);
}


TEST_F(ModelUtilTest, AddBreakpointLabels) {
  std::unique_ptr<BreakpointModel> breakpoint = BreakpointBuilder()
      .add_label("key1", "value1")
      .add_label("key2", "value2")
      .build();

  std::map<std::string, std::string> expected_labels;
  expected_labels["key1"] = "value1";
  expected_labels["key2"] = "value2";

  EXPECT_EQ(expected_labels, breakpoint->labels);
}


TEST_F(ModelUtilTest, ClearBreakpointLabels) {
  std::unique_ptr<BreakpointModel> breakpoint = BreakpointBuilder()
      .add_label("key", "value")
      .clear_labels()
      .build();

  EXPECT_TRUE(breakpoint->labels.empty());
}


TEST_F(ModelUtilTest, SetUserId) {
  std::vector<std::string> evaluated_user_id = {"mdb_user", "noogler"};

  std::unique_ptr<BreakpointModel> breakpoint = BreakpointBuilder()
      .set_evaluated_user_id(
          UserIdBuilder()
              .set_kind("mdb_user")
              .set_id("noogler"))
      .build();

  ASSERT_NE(breakpoint->evaluated_user_id, nullptr);
  EXPECT_EQ("mdb_user", breakpoint->evaluated_user_id->kind);
  EXPECT_EQ("noogler", breakpoint->evaluated_user_id->id);
}

}  // namespace cdbg
}  // namespace devtools

