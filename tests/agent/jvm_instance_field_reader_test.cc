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

#include "src/agent/jvm_instance_field_reader.h"

#include "gtest/gtest.h"
#include "tests/agent/mock_jni_env.h"
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

static const jfieldID kFieldId = reinterpret_cast<jfieldID>(123);
static const jobject kSourceObject = reinterpret_cast<jobject>(0x83467524);

class JvmInstanceFieldReaderTest : public ::testing::Test {
 protected:
  JvmInstanceFieldReaderTest() : global_jvm_(&jvmti_, &jni_) {}

  void TestReadValue(JSignature signature, const std::string& expected_value) {
    JvmInstanceFieldReader reader(
        "myvar",
        kFieldId,
        signature,
        false,
        read_error_);

    JVariant jvariant_value;
    FormatMessageModel error;
    EXPECT_TRUE(reader.ReadValue(kSourceObject, &jvariant_value, &error));

    EXPECT_EQ(expected_value, jvariant_value.ToString(false));
  }

 protected:
  MockJvmtiEnv jvmti_;
  MockJNIEnv jni_;
  GlobalJvmEnv global_jvm_;
  FormatMessageModel read_error_;
};


TEST_F(JvmInstanceFieldReaderTest, ReadBoolean) {
  EXPECT_CALL(jni_, GetBooleanField(kSourceObject, kFieldId))
      .WillRepeatedly(Return(true));

  TestReadValue({ JType::Boolean }, "<boolean>true");
}


TEST_F(JvmInstanceFieldReaderTest, ReadByte) {
  EXPECT_CALL(jni_, GetByteField(kSourceObject, kFieldId))
      .WillRepeatedly(Return(-31));

  TestReadValue({ JType::Byte }, "<byte>-31");
}


TEST_F(JvmInstanceFieldReaderTest, ReadChar) {
  EXPECT_CALL(jni_, GetCharField(kSourceObject, kFieldId))
      .WillRepeatedly(Return('A'));

  TestReadValue({ JType::Char }, "<char>65");
}


TEST_F(JvmInstanceFieldReaderTest, ReadShort) {
  EXPECT_CALL(jni_, GetShortField(kSourceObject, kFieldId))
      .WillRepeatedly(Return(27123));

  TestReadValue({ JType::Short }, "<short>27123");
}


TEST_F(JvmInstanceFieldReaderTest, ReadInt) {
  EXPECT_CALL(jni_, GetIntField(kSourceObject, kFieldId))
      .WillRepeatedly(Return(427));

  TestReadValue({ JType::Int }, "<int>427");
}


TEST_F(JvmInstanceFieldReaderTest, ReadLong) {
  EXPECT_CALL(jni_, GetLongField(kSourceObject, kFieldId))
      .WillRepeatedly(Return(783496836454378L));

  TestReadValue({ JType::Long }, "<long>783496836454378");
}


TEST_F(JvmInstanceFieldReaderTest, ReadFloat) {
  EXPECT_CALL(jni_, GetFloatField(kSourceObject, kFieldId))
      .WillRepeatedly(Return(23.4564f));

  TestReadValue({ JType::Float }, "<float>23.4564");
}


TEST_F(JvmInstanceFieldReaderTest, ReadDouble) {
  EXPECT_CALL(jni_, GetDoubleField(kSourceObject, kFieldId))
      .WillRepeatedly(Return(879.345));

  TestReadValue({ JType::Double }, "<double>879.345");
}


TEST_F(JvmInstanceFieldReaderTest, ReadNullObject) {
  EXPECT_CALL(jni_, GetObjectField(kSourceObject, kFieldId))
      .WillRepeatedly(Return(nullptr));

  TestReadValue({ JType::Object, kJavaStringClassSignature }, "null");
}


TEST_F(JvmInstanceFieldReaderTest, ReadObject) {
  const std::string kObjectSignature = "Ljava/lang/Thread;";
  const jobject kObjectValue = reinterpret_cast<jobject>(0x87324648234L);

  EXPECT_CALL(jni_, GetObjectRefType(_))
      .WillRepeatedly(Return(JNILocalRefType));

  EXPECT_CALL(jni_, GetObjectField(kSourceObject, kFieldId))
      .WillRepeatedly(Return(kObjectValue));

  JvmInstanceFieldReader reader(
      "myvar",
      kFieldId,
      { JType::Object, kObjectSignature },
      false,
      read_error_);

  JVariant jvariant_value;
  FormatMessageModel error;
  EXPECT_TRUE(reader.ReadValue(kSourceObject, &jvariant_value, &error));

  EXPECT_EQ(JType::Object, jvariant_value.type());

  jobject actual_object_value = nullptr;
  EXPECT_TRUE(jvariant_value.get<jobject>(&actual_object_value));
  EXPECT_EQ(kObjectValue, actual_object_value);

  EXPECT_CALL(jni_, DeleteLocalRef(kObjectValue))
      .WillOnce(Return());
}


TEST_F(JvmInstanceFieldReaderTest, Signature) {
  JvmInstanceFieldReader reader(
      "myvar",
      kFieldId,
      { JType::Object, "Ljava/lang/Thread;" },
      false,
      read_error_);

  EXPECT_EQ("myvar", reader.GetName());
  EXPECT_EQ(JType::Object, reader.GetStaticType().type);
  EXPECT_EQ("Ljava/lang/Thread;", reader.GetStaticType().object_signature);
}


TEST_F(JvmInstanceFieldReaderTest, SignatureVoidType) {
  JvmInstanceFieldReader reader(
      "myvar",
      kFieldId,
      { JType::Void, "Ljava/lang/Thread;" },
      false,
      read_error_);

  JVariant jvariant_value;
  FormatMessageModel error;
  EXPECT_FALSE(reader.ReadValue(kSourceObject, &jvariant_value, &error));
  EXPECT_GT(error.format.length(), 0);
}


TEST_F(JvmInstanceFieldReaderTest, SignatureWithReadError) {
  read_error_.format = "read error";
  JvmInstanceFieldReader reader(
      "myvar",
      kFieldId,
      { JType::Object, "Ljava/lang/Thread;" },
      true,
      read_error_);

  JVariant jvariant_value;
  FormatMessageModel error;
  EXPECT_FALSE(reader.ReadValue(kSourceObject, &jvariant_value, &error));
  EXPECT_EQ(read_error_, error);
}

}  // namespace cdbg
}  // namespace devtools
