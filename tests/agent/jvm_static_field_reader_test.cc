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

#include "src/agent/jvm_static_field_reader.h"

#include "gtest/gtest.h"
#include "tests/agent/fake_jni.h"
#include "tests/agent/mock_jni_env.h"
#include "tests/agent/mock_jvmti_env.h"

using testing::_;
using testing::Return;
using testing::Invoke;

namespace devtools {
namespace cdbg {

static const jfieldID kFieldId = reinterpret_cast<jfieldID>(123);

class JvmStaticFieldReaderTest : public ::testing::Test {
 protected:
  JvmStaticFieldReaderTest()
      : fake_jni_(&jni_),
        global_jvm_(fake_jni_.jvmti(), fake_jni_.jni()) {
  }

  void SetUp() override {
    cls_ = fake_jni_.GetStockClass(FakeJni::StockClass::MyClass1);
  }

  void TestReadValue(JSignature signature, const std::string& expected_value) {
    JvmStaticFieldReader reader(
        cls_,
        "myvar",
        kFieldId,
        signature,
        false,
        read_error_);

    JVariant jvariant_value;
    FormatMessageModel error;
    EXPECT_TRUE(reader.ReadValue(&jvariant_value, &error));

    EXPECT_EQ(expected_value, jvariant_value.ToString(false));

    reader.ReleaseRef();
  }

 protected:
  testing::NiceMock<MockJNIEnv> jni_;
  FakeJni fake_jni_;
  GlobalJvmEnv global_jvm_;
  FormatMessageModel read_error_;

  // Fake Java class whose static fields this unit test is reading.
  jclass cls_ { nullptr };
};


TEST_F(JvmStaticFieldReaderTest, ReadBoolean) {
  EXPECT_CALL(jni_, GetStaticBooleanField(_, kFieldId))
      .WillRepeatedly(Return(true));

  TestReadValue({ JType::Boolean }, "<boolean>true");
}


TEST_F(JvmStaticFieldReaderTest, ReadByte) {
  EXPECT_CALL(jni_, GetStaticByteField(_, kFieldId))
      .WillRepeatedly(Return(-31));

  TestReadValue({ JType::Byte }, "<byte>-31");
}


TEST_F(JvmStaticFieldReaderTest, ReadChar) {
  EXPECT_CALL(jni_, GetStaticCharField(_, kFieldId))
      .WillRepeatedly(Return('A'));

  TestReadValue({ JType::Char }, "<char>65");
}


TEST_F(JvmStaticFieldReaderTest, ReadShort) {
  EXPECT_CALL(jni_, GetStaticShortField(_, kFieldId))
      .WillRepeatedly(Return(27123));

  TestReadValue({ JType::Short }, "<short>27123");
}


TEST_F(JvmStaticFieldReaderTest, ReadInt) {
  EXPECT_CALL(jni_, GetStaticIntField(_, kFieldId))
      .WillRepeatedly(Return(427));

  TestReadValue({ JType::Int }, "<int>427");
}


TEST_F(JvmStaticFieldReaderTest, ReadLong) {
  EXPECT_CALL(jni_, GetStaticLongField(_, kFieldId))
      .WillRepeatedly(Return(783496836454378L));

  TestReadValue({ JType::Long }, "<long>783496836454378");
}


TEST_F(JvmStaticFieldReaderTest, ReadFloat) {
  EXPECT_CALL(jni_, GetStaticFloatField(_, kFieldId))
      .WillRepeatedly(Return(23.4564f));

  TestReadValue({ JType::Float }, "<float>23.4564");
}


TEST_F(JvmStaticFieldReaderTest, ReadDouble) {
  EXPECT_CALL(jni_, GetStaticDoubleField(_, kFieldId))
      .WillRepeatedly(Return(879.345));

  TestReadValue({ JType::Double }, "<double>879.345");
}


TEST_F(JvmStaticFieldReaderTest, ReadNullObject) {
  const std::string kObjectSignature = "Ljava/lang/String;";

  EXPECT_CALL(jni_, GetStaticObjectField(_, kFieldId))
      .WillRepeatedly(Return(nullptr));

  TestReadValue({ JType::Object, kObjectSignature }, "null");
}


TEST_F(JvmStaticFieldReaderTest, ReadObject) {
  const std::string kObjectSignature = "Ljava/lang/Thread;";
  jobject obj = fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1);
  jobject obj_ref_copy = jni_.NewLocalRef(obj);

  // The called to "GetStaticObjectField" will release "obj" ref.
  EXPECT_CALL(jni_, GetStaticObjectField(_, kFieldId))
      .WillOnce(Return(obj));

  JvmStaticFieldReader reader(
      cls_,
      "myvar",
      kFieldId,
      { JType::Object, kObjectSignature },
      false,
      read_error_);

  JVariant jvariant_value;
  FormatMessageModel error;
  EXPECT_TRUE(reader.ReadValue(&jvariant_value, &error));

  EXPECT_EQ(JType::Object, jvariant_value.type());

  jobject actual_object_value = nullptr;
  EXPECT_TRUE(jvariant_value.get<jobject>(&actual_object_value));
  EXPECT_TRUE(jni_.IsSameObject(obj_ref_copy, actual_object_value));

  reader.ReleaseRef();

  jni_.DeleteLocalRef(obj_ref_copy);
}


TEST_F(JvmStaticFieldReaderTest, Signature) {
  JvmStaticFieldReader reader(
      cls_,
      "myvar",
      kFieldId,
      { JType::Object, "Ljava/lang/Thread;" },
      false,
      read_error_);

  EXPECT_EQ("myvar", reader.GetName());
  EXPECT_EQ(JType::Object, reader.GetStaticType().type);
  EXPECT_EQ("Ljava/lang/Thread;", reader.GetStaticType().object_signature);

  reader.ReleaseRef();
}


TEST_F(JvmStaticFieldReaderTest, SignatureVoidType) {
  JvmStaticFieldReader reader(
      cls_,
      "myvar",
      kFieldId,
      { JType::Void, "Ljava/lang/Thread;" },
      false,
      read_error_);

  JVariant jvariant_value;
  FormatMessageModel error;
  EXPECT_FALSE(reader.ReadValue(&jvariant_value, &error));
  EXPECT_GT(error.format.length(), 0);

  reader.ReleaseRef();
}

TEST_F(JvmStaticFieldReaderTest, SignatureWithReadError) {
  read_error_.format = "read error";
  JvmStaticFieldReader reader(
      cls_,
      "myvar",
      kFieldId,
      { JType::Object, "Ljava/lang/Thread;" },
      true,
      read_error_);

  JVariant jvariant_value;
  FormatMessageModel error;
  EXPECT_FALSE(reader.ReadValue(&jvariant_value, &error));
  EXPECT_EQ(read_error_, error);
}

}  // namespace cdbg
}  // namespace devtools
