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

#include "src/agent/value_formatter.h"

#include "gtest/gtest.h"
#include "src/agent/model_util.h"
#include "tests/agent/fake_jni.h"
#include "tests/agent/mock_jvmti_env.h"

namespace devtools {
namespace cdbg {

class ValueFormatterTest : public ::testing::Test {
 protected:
  ValueFormatterTest() : global_jvm_(fake_jni_.jvmti(), fake_jni_.jni()) {}

 protected:
  FakeJni fake_jni_;
  GlobalJvmEnv global_jvm_;
};

TEST_F(ValueFormatterTest, DataSize_Incomplete) {
  NamedJVariant v;
  v.name = "bob";  // length = 3
  v.status.description.format = "some message";  // length = 12
  v.status.description.parameters.push_back("23846234");  // length = 8
  v.status.description.parameters.push_back("ekrughjgfsdjhk");  // length = 14

  EXPECT_EQ(
      3 + 12 + 8 + 14,
      ValueFormatter::GetTotalDataSize(v));
}


TEST_F(ValueFormatterTest, DataSize_Primitive) {
  NamedJVariant v;
  v.name = "bob";
  v.value = JVariant::Double(3.1);

  EXPECT_EQ(3 + 8, ValueFormatter::GetTotalDataSize(v));
}


TEST_F(ValueFormatterTest, DataSize_NullObject) {
  NamedJVariant v;
  v.name = "bob";
  v.value.attach_ref(JVariant::ReferenceKind::Local, nullptr);

  EXPECT_EQ(3 + 8, ValueFormatter::GetTotalDataSize(v));
}


TEST_F(ValueFormatterTest, DataSize_NullString) {
  NamedJVariant v;
  v.name = "bob";
  v.value.attach_ref(JVariant::ReferenceKind::Local, nullptr);
  v.well_known_jclass = WellKnownJClass::String;

  EXPECT_EQ(3 + 4, ValueFormatter::GetTotalDataSize(v));
}


TEST_F(ValueFormatterTest, DataSize_ShortString) {
  NamedJVariant v;
  v.name = "bob";
  v.value.attach_ref(JVariant::ReferenceKind::Local,
                     fake_jni_.CreateNewJavaString(
                         std::string(kDefaultMaxStringLength - 1, 'A')));
  v.well_known_jclass = WellKnownJClass::String;

  EXPECT_EQ(
      3 + 2 + kDefaultMaxStringLength - 1,  // "bob" + double quotes + length.
      ValueFormatter::GetTotalDataSize(v));
}


TEST_F(ValueFormatterTest, DataSize_TruncatedString) {
  NamedJVariant v;
  v.name = "bob";
  v.value.attach_ref(
      JVariant::ReferenceKind::Local,
      fake_jni_.CreateNewJavaString("abc\x88 def"));
  v.well_known_jclass = WellKnownJClass::String;

  // The \x88 translates into two bytes.
  EXPECT_EQ(3 + 2 + 8, ValueFormatter::GetTotalDataSize(v));
}


TEST_F(ValueFormatterTest, DataSize_UnicodeString) {
  NamedJVariant v;
  v.name = "bob";
  v.value.attach_ref(JVariant::ReferenceKind::Local,
                     fake_jni_.CreateNewJavaString(
                         std::string(kDefaultMaxStringLength + 1, 'A')));
  v.well_known_jclass = WellKnownJClass::String;

  EXPECT_EQ(
      3 + 2 + kDefaultMaxStringLength,  // "bob" + double quotes + length.
      ValueFormatter::GetTotalDataSize(v));
}


TEST_F(ValueFormatterTest, IsValue_Incomplete) {
  NamedJVariant v;

  EXPECT_TRUE(ValueFormatter::IsValue(v));
}


TEST_F(ValueFormatterTest, IsValue_Primitive) {
  NamedJVariant v;
  v.value = JVariant::Double(3.1);

  EXPECT_TRUE(ValueFormatter::IsValue(v));
}


TEST_F(ValueFormatterTest, IsValue_NullObject) {
  NamedJVariant v;
  v.value.attach_ref(JVariant::ReferenceKind::Local, nullptr);

  EXPECT_TRUE(ValueFormatter::IsValue(v));
}


TEST_F(ValueFormatterTest, IsValue_NullString) {
  NamedJVariant v;
  v.value.attach_ref(JVariant::ReferenceKind::Local, nullptr);
  v.well_known_jclass = WellKnownJClass::String;

  EXPECT_TRUE(ValueFormatter::IsValue(v));
}


TEST_F(ValueFormatterTest, IsValue_String) {
  NamedJVariant v;
  v.value.attach_ref(
      JVariant::ReferenceKind::Local,
      fake_jni_.CreateNewJavaString("hello world"));
  v.well_known_jclass = WellKnownJClass::String;

  EXPECT_TRUE(ValueFormatter::IsValue(v));
}


TEST_F(ValueFormatterTest, IsValue_Object) {
  NamedJVariant v;
  v.value.attach_ref(
      JVariant::ReferenceKind::Local,
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  EXPECT_FALSE(ValueFormatter::IsValue(v));
}


TEST_F(ValueFormatterTest, Format_Primitive) {
  NamedJVariant v;
  v.value = JVariant::Double(3.1);

  std::string formatted_value;
  std::string type;
  ValueFormatter::Format(v, ValueFormatter::Options(), &formatted_value, &type);

  EXPECT_EQ("3.1", formatted_value);
  EXPECT_EQ("double", type);
}


TEST_F(ValueFormatterTest, Format_Primitive_NoType) {
  NamedJVariant v;
  v.value = JVariant::Boolean(true);

  std::string formatted_value;
  ValueFormatter::Format(
      v,
      ValueFormatter::Options(),
      &formatted_value,
      nullptr);

  EXPECT_EQ("true", formatted_value);
}


TEST_F(ValueFormatterTest, Format_NullObject) {
  NamedJVariant v;
  v.value.attach_ref(JVariant::ReferenceKind::Local, nullptr);

  std::string formatted_value;
  std::string type;
  ValueFormatter::Format(v, ValueFormatter::Options(), &formatted_value, &type);

  EXPECT_EQ("null", formatted_value);
  EXPECT_EQ("", type);
}


TEST_F(ValueFormatterTest, Format_NullObject_NoType) {
  NamedJVariant v;
  v.value.attach_ref(JVariant::ReferenceKind::Local, nullptr);

  std::string formatted_value;
  ValueFormatter::Format(
      v,
      ValueFormatter::Options(),
      &formatted_value,
      nullptr);

  EXPECT_EQ("null", formatted_value);
}


TEST_F(ValueFormatterTest, Format_NullString) {
  NamedJVariant v;
  v.value.attach_ref(JVariant::ReferenceKind::Local, nullptr);
  v.well_known_jclass = WellKnownJClass::String;

  std::string formatted_value;
  std::string type;
  ValueFormatter::Format(v, ValueFormatter::Options(), &formatted_value, &type);

  EXPECT_EQ("null", formatted_value);
  EXPECT_EQ("", type);  // We don't print types for nulls.
}


TEST_F(ValueFormatterTest, Format_EmptyString) {
  NamedJVariant v;
  v.value.attach_ref(
      JVariant::ReferenceKind::Local,
      fake_jni_.CreateNewJavaString(""));
  v.well_known_jclass = WellKnownJClass::String;

  std::string formatted_value;
  std::string type;

  ValueFormatter::Options quote;
  ValueFormatter::Format(v, quote, &formatted_value, &type);
  EXPECT_EQ("\"\"", formatted_value);
  EXPECT_EQ("String", type);

  formatted_value.clear();
  ValueFormatter::Options no_quote;
  no_quote.quote_string = false;
  ValueFormatter::Format(v, no_quote, &formatted_value, nullptr);
  EXPECT_EQ("", formatted_value);
}


TEST_F(ValueFormatterTest, Format_ShortString) {
  NamedJVariant v;
  v.value.attach_ref(
      JVariant::ReferenceKind::Local,
      fake_jni_.CreateNewJavaString("hello world"));
  v.well_known_jclass = WellKnownJClass::String;

  std::string formatted_value;
  std::string type;

  ValueFormatter::Options quote;
  ValueFormatter::Format(v, quote, &formatted_value, &type);
  EXPECT_EQ("\"hello world\"", formatted_value);
  EXPECT_EQ("String", type);

  formatted_value.clear();
  ValueFormatter::Options no_quote;
  no_quote.quote_string = false;
  ValueFormatter::Format(v, no_quote, &formatted_value, nullptr);
  EXPECT_EQ("hello world", formatted_value);
}


TEST_F(ValueFormatterTest, Format_TruncatedString) {
  NamedJVariant v;
  v.value.attach_ref(JVariant::ReferenceKind::Local,
                     fake_jni_.CreateNewJavaString(
                         std::string(kDefaultMaxStringLength + 1, 'A')));
  v.well_known_jclass = WellKnownJClass::String;

  std::unique_ptr<StatusMessageModel> expected_status_message =
      StatusMessageBuilder()
      .set_info()
      .set_refers_to(StatusMessageModel::Context::VARIABLE_VALUE)
      .set_description({ FormatTrimmedLocalString,
                       { std::to_string(kDefaultMaxStringLength + 1) }})
      .build();

  std::string formatted_value;
  ValueFormatter::Options quote;
  std::unique_ptr<StatusMessageModel> status_message =
      ValueFormatter::Format(v, quote, &formatted_value, nullptr);
  EXPECT_EQ('"' + std::string(kDefaultMaxStringLength, 'A') + " ...\"",
            formatted_value);
  EXPECT_EQ(*status_message, *expected_status_message);

  formatted_value.clear();
  ValueFormatter::Options no_quote;
  no_quote.quote_string = false;
  status_message =
      ValueFormatter::Format(v, no_quote, &formatted_value, nullptr);
  EXPECT_EQ(std::string(kDefaultMaxStringLength, 'A') + " ...",
            formatted_value);
  EXPECT_EQ(*status_message, *expected_status_message);
}


TEST_F(ValueFormatterTest, Format_UnicodeString) {
  NamedJVariant v;
  v.value.attach_ref(
      JVariant::ReferenceKind::Local,
      fake_jni_.CreateNewJavaString("abc\x88 def"));
  v.well_known_jclass = WellKnownJClass::String;

  std::string formatted_value;
  std::string type;
  ValueFormatter::Format(v, ValueFormatter::Options(), &formatted_value, &type);

  EXPECT_EQ("\"abc\b* def\"", formatted_value);
  EXPECT_EQ("String", type);
}


}  // namespace cdbg
}  // namespace devtools
