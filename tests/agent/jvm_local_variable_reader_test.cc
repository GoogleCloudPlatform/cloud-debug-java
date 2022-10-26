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

#include "src/agent/jvm_local_variable_reader.h"

#include "gtest/gtest.h"
#include "src/agent/readers_factory.h"
#include "tests/agent/fake_jni.h"
#include "tests/agent/mock_jvmti_env.h"

using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::IsNull;
using testing::NotNull;
using testing::Test;
using testing::Return;
using testing::SetArgPointee;

namespace devtools {
namespace cdbg {

static const jthread thread = reinterpret_cast<jthread>(0x67125374);
static constexpr int frame_depth = 4;


class LocalVariableReaderTest : public ::testing::Test {
 protected:
  LocalVariableReaderTest()
      : fake_jni_(&jvmti_),
        global_jvm_(&jvmti_, fake_jni_.jni()) {
  }

 protected:
  MockJvmtiEnv jvmti_;
  FakeJni fake_jni_;
  GlobalJvmEnv global_jvm_;
  FormatMessageModel read_error_;
};


// Verify correct evaluation of local variables of all types.
TEST_F(LocalVariableReaderTest, Extraction) {
  // These local references are returned directly in "GetLocalObject" mock and
  // it is the responsibility of the LocalVariableReader to release them.
  jobject local_ref_array_of_integers =
      fake_jni_.CreateNewObject(FakeJni::StockClass::IntArray);
  jobject local_ref_string = fake_jni_.CreateNewJavaString("abc");

  jvmtiLocalVariableEntry table[] = {
    {
      100,  // start_location
      0,  // length
      const_cast<char*>("local_boolean"),  // name
      const_cast<char*>("Z"),  // signature
      nullptr,  // generic_signature
      100  // slot
    },
    {
      100,  // start_location
      0,  // length
      const_cast<char*>("local_char"),  // name
      const_cast<char*>("C"),  // signature
      nullptr,  // generic_signature
      101  // slot
    },
    {
      100,  // start_location
      0,  // length
      const_cast<char*>("local_byte"),  // name
      const_cast<char*>("B"),  // signature
      nullptr,  // generic_signature
      102  // slot
    },
    {
      100,  // start_location
      0,  // length
      const_cast<char*>("local_short"),  // name
      const_cast<char*>("S"),  // signature
      nullptr,  // generic_signature
      103  // slot
    },
    {
      100,  // start_location
      0,  // length
      const_cast<char*>("local_int"),  // name
      const_cast<char*>("I"),  // signature
      nullptr,  // generic_signature
      104  // slot
    },
    {
      100,  // start_location
      0,  // length
      const_cast<char*>("local_long"),  // name
      const_cast<char*>("J"),  // signature
      nullptr,  // generic_signature
      105  // slot
    },
    {
      100,  // start_location
      0,  // length
      const_cast<char*>("local_float"),  // name
      const_cast<char*>("F"),  // signature
      nullptr,  // generic_signature
      106  // slot
    },
    {
      100,  // start_location
      0,  // length
      const_cast<char*>("local_double"),  // name
      const_cast<char*>("D"),  // signature
      nullptr,  // generic_signature
      107  // slot
    },
    {
      100,  // start_location
      0,  // length
      const_cast<char*>("local_array_of_integers"),  // name
      const_cast<char*>("[I"),  // signature
      nullptr,  // generic_signature
      108  // slot
    },
    {
      100,  // start_location
      0,  // length
      const_cast<char*>("local_string"),  // name
      const_cast<char*>("Ljava/lang/String"),  // signature
      nullptr,  // generic_signature
      109  // slot
    }
  };

  // local_boolean
  EXPECT_CALL(
      jvmti_,
      GetLocalInt(thread, frame_depth, 100, NotNull()))
      .Times(1)
      .WillRepeatedly(DoAll(SetArgPointee<3>(0), Return(JVMTI_ERROR_NONE)));

  // local_char (2 byte in Java)
  EXPECT_CALL(
      jvmti_,
      GetLocalInt(thread, frame_depth, 101, NotNull()))
      .Times(1)
      .WillRepeatedly(DoAll(SetArgPointee<3>(12345), Return(JVMTI_ERROR_NONE)));

  // local_byte
  EXPECT_CALL(
      jvmti_,
      GetLocalInt(thread, frame_depth, 102, NotNull()))
      .Times(1)
      .WillRepeatedly(DoAll(SetArgPointee<3>(-94), Return(JVMTI_ERROR_NONE)));

  // local_short
  EXPECT_CALL(
      jvmti_,
      GetLocalInt(thread, frame_depth, 103, NotNull()))
      .Times(1)
      .WillRepeatedly(DoAll(
          SetArgPointee<3>(-25231),
          Return(JVMTI_ERROR_NONE)));

  // local_int
  EXPECT_CALL(
      jvmti_,
      GetLocalInt(thread, frame_depth, 104, NotNull()))
      .Times(1)
      .WillRepeatedly(DoAll(
          SetArgPointee<3>(13747862),
          Return(JVMTI_ERROR_NONE)));

  // local_long
  EXPECT_CALL(
      jvmti_,
      GetLocalLong(thread, frame_depth, 105, NotNull()))
      .Times(1)
      .WillRepeatedly(DoAll(
          SetArgPointee<3>(83764287364234L),
          Return(JVMTI_ERROR_NONE)));

  // local_float
  EXPECT_CALL(
      jvmti_,
      GetLocalFloat(thread, frame_depth, 106, NotNull()))
      .Times(1)
      .WillRepeatedly(DoAll(
          SetArgPointee<3>(12.45f),
          Return(JVMTI_ERROR_NONE)));

  // local_double
  EXPECT_CALL(
      jvmti_,
      GetLocalDouble(thread, frame_depth, 107, NotNull()))
      .Times(1)
      .WillRepeatedly(DoAll(
          SetArgPointee<3>(4.1273467235476),
          Return(JVMTI_ERROR_NONE)));

  // local_array_of_integers (arrays are objects in Java)
  EXPECT_CALL(
      jvmti_,
      GetLocalObject(thread, frame_depth, 108, NotNull()))
      .Times(1)
      .WillRepeatedly(DoAll(
          SetArgPointee<3>(local_ref_array_of_integers),
          Return(JVMTI_ERROR_NONE)));

  // local_string (strings are objects in Java)
  EXPECT_CALL(
      jvmti_,
      GetLocalObject(thread, frame_depth, 109, NotNull()))
      .Times(1)
      .WillRepeatedly(DoAll(
          SetArgPointee<3>(local_ref_string),
          Return(JVMTI_ERROR_NONE)));

  std::vector<std::unique_ptr<JVariant>> jvariant_result;

  for (const jvmtiLocalVariableEntry& entry : table) {
    JvmLocalVariableReader local_variable_reader(
        entry,
        true,
        false,
        read_error_);

    std::unique_ptr<LocalVariableReader> reader_copy =
        local_variable_reader.Clone();

    EvaluationContext evaluation_context;
    evaluation_context.thread = thread;
    evaluation_context.frame_depth = frame_depth;

    std::unique_ptr<JVariant> jvariant(new JVariant);
    FormatMessageModel error;
    EXPECT_TRUE(
        reader_copy->ReadValue(evaluation_context, jvariant.get(), &error));

    jvariant_result.push_back(std::move(jvariant));
  }

  ASSERT_EQ(10, jvariant_result.size());

  EXPECT_EQ("<boolean>false", jvariant_result[0]->ToString(false));
  EXPECT_EQ("<char>12345", jvariant_result[1]->ToString(false));
  EXPECT_EQ("<byte>-94", jvariant_result[2]->ToString(false));
  EXPECT_EQ("<short>-25231", jvariant_result[3]->ToString(false));
  EXPECT_EQ("<int>13747862", jvariant_result[4]->ToString(false));
  EXPECT_EQ("<long>83764287364234", jvariant_result[5]->ToString(false));
  EXPECT_EQ("<float>12.45", jvariant_result[6]->ToString(false));
  EXPECT_EQ("<double>4.127346724", jvariant_result[7]->ToString(false));
  EXPECT_EQ("<Object>", jvariant_result[8]->ToString(false));
  EXPECT_EQ("<Object>", jvariant_result[9]->ToString(false));
}


// Verify that local variable outside the scope are not evaluated.
TEST_F(LocalVariableReaderTest, Scope) {
  jvmtiLocalVariableEntry table[2];
  table[0].start_location = 0;
  table[0].length = 100;
  table[1].start_location = 101;
  table[1].length = 1;
  table[0].name = table[1].name = const_cast<char*>("local");
  table[0].signature = table[1].signature = const_cast<char*>("Z");
  table[0].generic_signature = table[1].generic_signature = nullptr;
  table[0].slot = table[1].slot = 0;

  for (const jvmtiLocalVariableEntry& entry : table) {
    JvmLocalVariableReader local_variable_reader(
        entry,
        false,
        false,
        read_error_);
    ASSERT_FALSE(local_variable_reader.IsDefinedAtLocation(100));
  }
}


TEST_F(LocalVariableReaderTest, IsArgument) {
  jvmtiLocalVariableEntry entry;
  entry.start_location = 0;
  entry.length = 100;
  entry.name = const_cast<char*>("local");
  entry.signature = const_cast<char*>("Z");
  entry.generic_signature = nullptr;
  entry.slot = 0;

  JvmLocalVariableReader argument_reader(entry, true, false, read_error_);
  ASSERT_TRUE(argument_reader.IsArgument());

  JvmLocalVariableReader local_variable_reader(
      entry,
      false,
      false,
      read_error_);
  ASSERT_FALSE(local_variable_reader.IsArgument());
}


TEST_F(LocalVariableReaderTest, HasReadError) {
  jvmtiLocalVariableEntry entry;
  entry.start_location = 0;
  entry.length = 100;
  entry.name = const_cast<char*>("local");
  entry.signature = const_cast<char*>("Z");
  entry.generic_signature = nullptr;
  entry.slot = 0;

  read_error_.format = "read error";

  JvmLocalVariableReader reader(entry, true, true, read_error_);
  EvaluationContext evaluation_context;
  JVariant jvariant;
  FormatMessageModel error;
  EXPECT_FALSE(reader.ReadValue(evaluation_context, &jvariant, &error));
  EXPECT_EQ(read_error_, error);
}

}  // namespace cdbg
}  // namespace devtools
