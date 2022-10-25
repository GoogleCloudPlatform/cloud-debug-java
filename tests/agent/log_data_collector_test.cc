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

#include "src/agent/log_data_collector.h"

#include "gtest/gtest.h"
#include "mock_object.h"
#include "src/agent/expression_evaluator.h"
#include "src/agent/model_json.h"
#include "src/agent/model_util.h"
#include "tests/agent/fake_jni.h"
#include "tests/agent/mock_jvmti_env.h"
#include "tests/agent/mock_method_caller.h"
#include "tests/agent/mock_object_evaluator.h"
#include "tests/agent/mock_readers_factory.h"

using testing::_;
using testing::DoAll;
using testing::IsNull;
using testing::NiceMock;
using testing::NotNull;
using testing::Return;
using testing::ReturnRef;
using testing::Invoke;
using testing::InvokeArgument;
using testing::SetArgPointee;

namespace devtools {
namespace cdbg {

static const jthread kThread = reinterpret_cast<jthread>(0x67125374);

class LogDataCollectorTest : public ::testing::Test {
 protected:
  LogDataCollectorTest() : global_jvm_(fake_jni_.jvmti(), fake_jni_.jni()) {
  }

  void SetUp() override {
    EXPECT_CALL(java_lang_object_, GetClass())
        .WillRepeatedly(Return(
            fake_jni_.GetStockClass(FakeJni::StockClass::Object)));

    jniproxy::InjectObject(&java_lang_object_);
    readers_factory_.SetUpDefault();
  }

  void TearDown() override {
    jniproxy::InjectObject(nullptr);
  }

  std::string Process(const BreakpointModel& breakpoint) {
    // Compile watched expressions.
    std::vector<CompiledExpression> watches;
    for (const std::string& expression : breakpoint.expressions) {
      watches.push_back(CompileExpression(expression, &readers_factory_));
    }

    LogDataCollector collector;
    collector.Collect(
        &method_caller_,
        &object_evaluator_,
        watches,
        kThread);

    return collector.Format(breakpoint);
  }

  void TestCommon(std::string expected_log_message,
                  std::unique_ptr<BreakpointModel> breakpoint) {
    EXPECT_EQ(expected_log_message, Process(*breakpoint))
        << "Breakpoint: " << std::endl
        << BreakpointToPrettyJson(*breakpoint).data;
  }

 protected:
  FakeJni fake_jni_;
  GlobalJvmEnv global_jvm_;
  MockReadersFactory readers_factory_;
  MockObjectEvaluator object_evaluator_;
  MockMethodCaller method_caller_;
  jniproxy::MockObject java_lang_object_;
};


TEST_F(LogDataCollectorTest, EmptyMessage) {
  TestCommon(
    "",
    BreakpointBuilder()
      .set_id("BPID")
      .build());
}


TEST_F(LogDataCollectorTest, StaticMessage) {
  TestCommon(
    "Hello world!",
    BreakpointBuilder()
      .set_id("BPID")
      .set_log_message_format("Hello world!")
      .build());
}


TEST_F(LogDataCollectorTest, Escaping) {
  TestCommon(
    "$abc$def$",
    BreakpointBuilder()
      .set_id("BPID")
      .set_log_message_format("$$abc$$def$$")
      .build());

  TestCommon(
    "$a$ --$",
    BreakpointBuilder()
      .set_id("BPID")
      .set_log_message_format("$a$ --$")
      .build());

  TestCommon(
    "$",
    BreakpointBuilder()
      .set_id("BPID")
      .set_log_message_format("$")
      .build());
}


TEST_F(LogDataCollectorTest, Substitution) {
  TestCommon(
    "firstsecondabcthird",
    BreakpointBuilder()
      .set_id("BPID")
      .set_log_message_format("$0$1abc$2")
      .add_expression("\"first\"")
      .add_expression("\"second\"")
      .add_expression("\"third\"")
      .build());
}


TEST_F(LogDataCollectorTest, ErrorMessage) {
  readers_factory_.AddFakeLocal<jint>("myint", 31);

  TestCommon(
    "wish=The primitive type int does not have a field wish",
    BreakpointBuilder()
      .set_id("BPID")
      .set_log_message_format("wish=$0")
      .add_expression("myint.wish")
      .build());
}


TEST_F(LogDataCollectorTest, BadParameterIndex) {
  TestCommon(
    "a=Invalid parameter $1",
    BreakpointBuilder()
      .set_id("BPID")
      .set_log_message_format("a=$1")
      .add_expression("123")
      .build());
}


TEST_F(LogDataCollectorTest, ManyParameters) {
  TestCommon(
    "8121517",
    BreakpointBuilder()
      .set_id("BPID")
      .set_log_message_format("$8$12$15$17")
      .add_expression("0")
      .add_expression("1")
      .add_expression("2")
      .add_expression("3")
      .add_expression("4")
      .add_expression("5")
      .add_expression("6")
      .add_expression("7")
      .add_expression("8")
      .add_expression("9")
      .add_expression("10")
      .add_expression("11")
      .add_expression("12")
      .add_expression("13")
      .add_expression("14")
      .add_expression("15")
      .add_expression("16")
      .add_expression("17")
      .build());
}


TEST_F(LogDataCollectorTest, ToStringSuccess) {
  JVariant my_obj = JVariant::LocalRef(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));
  readers_factory_.AddFakeLocal("myObj", "LSourceObj;", my_obj);

  auto& metadata =
      fake_jni_.MutableStockClassMetadata(FakeJni::StockClass::MyClass1);
  metadata.methods.push_back(
      {
        reinterpret_cast<jmethodID>(0x123123),
        InstanceMethod("LSourceObj;", "toString", "()Ljava/lang/String;")
      });

  JVariant return_value = JVariant::LocalRef(
      fake_jni_.CreateNewJavaString("I am a string returned by toString"));

  EXPECT_CALL(method_caller_, Invoke(
      "class = Ljava/lang/Object;, method name = toString, "
      "method signature = ()Ljava/lang/String;, "
      "source = <Object>, arguments = ()"))
      .WillRepeatedly(Return(&return_value));

  TestCommon(
    "I am a string returned by toString",
    BreakpointBuilder()
      .set_id("BPID")
      .set_log_message_format("$0")
      .add_expression("myObj")
      .build());
}


TEST_F(LogDataCollectorTest, ToStringFailure) {
  JVariant my_obj = JVariant::LocalRef(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));
  readers_factory_.AddFakeLocal("myObj", "LSourceObj;", my_obj);

  EXPECT_CALL(method_caller_, Invoke(_))
      .WillRepeatedly(Return(FormatMessageModel { "some error" }));

  EXPECT_CALL(object_evaluator_, Evaluate(NotNull(), _, _, NotNull()))
      .WillRepeatedly(Invoke([this] (
          MethodCaller* method_caller,
          jobject obj,
          bool is_watch_expression,
          std::vector<NamedJVariant>* members) {
        NamedJVariant var1;
        var1.name = "myInt";
        var1.value = JVariant::Int(42);
        members->push_back(var1);

        NamedJVariant var2;
        var2.name = "myString";
        var2.value = JVariant::LocalRef(
            fake_jni_.CreateNewJavaString("hello"));
        var2.well_known_jclass = WellKnownJClass::String;
        members->push_back(var2);

        NamedJVariant var3;
        var3.name = "myOtherObj";
        var3.value = JVariant::LocalRef(
            fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass2));
        members->push_back(var3);
      }));

  TestCommon(
    "{ myInt: 42, myString: \"hello\", myOtherObj: <Object> }",
    BreakpointBuilder()
      .set_id("BPID")
      .set_log_message_format("$0")
      .add_expression("myObj")
      .build());
}


}  // namespace cdbg
}  // namespace devtools


