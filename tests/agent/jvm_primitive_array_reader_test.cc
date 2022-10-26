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

#include "src/agent/jvm_primitive_array_reader.h"

#include "gtest/gtest.h"
#include "src/agent/jni_utils.h"
#include "src/agent/messages.h"
#include "src/agent/model.h"
#include "tests/agent/fake_jni.h"
#include "tests/agent/mock_jni_env.h"
#include "tests/agent/mock_jvmti_env.h"

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::NotNull;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;

namespace devtools {
namespace cdbg {

class JvmPrimitiveArrayReaderTest : public ::testing::Test {
 protected:
  JvmPrimitiveArrayReaderTest()
      : fake_jni_(&jni_),
        global_jvm_(fake_jni_.jvmti(), &jni_) {
  }

  template <typename TElement>
  void SuccessTestCommon(const std::string& expected_result) {
    JVariant source;
    source.attach_ref(
        JVariant::ReferenceKind::Local,
        fake_jni_.CreateNewJavaString("a"));

    JVariant index = JVariant::Long(73);

    JvmPrimitiveArrayReader<TElement> reader;
    ErrorOr<JVariant> result = reader.ReadValue(source, index);

    EXPECT_FALSE(result.is_error());
    EXPECT_EQ(expected_result, result.value().ToString(false));
  }

 protected:
  NiceMock<MockJNIEnv> jni_;
  FakeJni fake_jni_;
  GlobalJvmEnv global_jvm_;
};


TEST_F(JvmPrimitiveArrayReaderTest, SuccessBoolean) {
  EXPECT_CALL(jni_, GetBooleanArrayRegion(NotNull(), 73, 1, NotNull()))
      .WillOnce(SetArgPointee<3>(true));

  SuccessTestCommon<jboolean>("<boolean>true");
}


TEST_F(JvmPrimitiveArrayReaderTest, SuccessByte) {
  EXPECT_CALL(jni_, GetByteArrayRegion(NotNull(), 73, 1, NotNull()))
      .WillOnce(SetArgPointee<3>(-89));

  SuccessTestCommon<jbyte>("<byte>-89");
}


TEST_F(JvmPrimitiveArrayReaderTest, SuccessChar) {
  EXPECT_CALL(jni_, GetCharArrayRegion(NotNull(), 73, 1, NotNull()))
      .WillOnce(SetArgPointee<3>(54321));

  SuccessTestCommon<jchar>("<char>54321");
}


TEST_F(JvmPrimitiveArrayReaderTest, SuccessShort) {
  EXPECT_CALL(jni_, GetShortArrayRegion(NotNull(), 73, 1, NotNull()))
      .WillOnce(SetArgPointee<3>(-12345));

  SuccessTestCommon<jshort>("<short>-12345");
}


TEST_F(JvmPrimitiveArrayReaderTest, SuccessInt) {
  EXPECT_CALL(jni_, GetIntArrayRegion(NotNull(), 73, 1, NotNull()))
      .WillOnce(SetArgPointee<3>(3487634));

  SuccessTestCommon<jint>("<int>3487634");
}


TEST_F(JvmPrimitiveArrayReaderTest, SuccessLong) {
  EXPECT_CALL(jni_, GetLongArrayRegion(NotNull(), 73, 1, NotNull()))
      .WillOnce(SetArgPointee<3>(9387458734655L));

  SuccessTestCommon<jlong>("<long>9387458734655");
}


TEST_F(JvmPrimitiveArrayReaderTest, SuccessFloat) {
  EXPECT_CALL(jni_, GetFloatArrayRegion(NotNull(), 73, 1, NotNull()))
      .WillOnce(SetArgPointee<3>(1.23f));

  SuccessTestCommon<jfloat>("<float>1.23");
}


TEST_F(JvmPrimitiveArrayReaderTest, SuccessDouble) {
  EXPECT_CALL(jni_, GetDoubleArrayRegion(NotNull(), 73, 1, NotNull()))
      .WillOnce(SetArgPointee<3>(3.1415));

  SuccessTestCommon<jdouble>("<double>3.1415");
}


TEST_F(JvmPrimitiveArrayReaderTest, BadSourceObject) {
  JVariant source = JVariant::Boolean(true);
  JVariant index = JVariant::Long(18);

  JvmPrimitiveArrayReader<jint> reader;
  ErrorOr<JVariant> result = reader.ReadValue(source, index);

  EXPECT_TRUE(result.is_error());
}


TEST_F(JvmPrimitiveArrayReaderTest, BadIndex) {
  JVariant source = JVariant::LocalRef(
      fake_jni_.CreateNewJavaString("a"));

  JVariant index = JVariant::Null();

  JvmPrimitiveArrayReader<jint> reader;
  ErrorOr<JVariant> result = reader.ReadValue(source, index);

  EXPECT_TRUE(result.is_error());
}


TEST_F(JvmPrimitiveArrayReaderTest, NullSourceObject) {
  JVariant source = JVariant::Null();
  JVariant index = JVariant::Long(18);

  JvmPrimitiveArrayReader<jint> reader;
  ErrorOr<JVariant> result = reader.ReadValue(source, index);

  EXPECT_TRUE(result.is_error());
}


TEST_F(JvmPrimitiveArrayReaderTest, AccessException) {
  JVariant source = JVariant::LocalRef(
      fake_jni_.CreateNewJavaString("a"));

  JVariant index = JVariant::Long(18);

  EXPECT_CALL(jni_, GetIntArrayRegion(NotNull(), 18, 1, NotNull()))
      .WillOnce(Return());

  JniLocalRef exception_object(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass3));

  EXPECT_CALL(jni_, ExceptionCheck())
      .WillOnce(Return(true));

  EXPECT_CALL(jni_, ExceptionOccurred())
      .WillOnce(Return(static_cast<jthrowable>(
          jni_.NewLocalRef(exception_object.get()))));

  JvmPrimitiveArrayReader<jint> reader;
  ErrorOr<JVariant> result = reader.ReadValue(source, index);

  EXPECT_TRUE(result.is_error());
  EXPECT_EQ(MethodCallExceptionOccurred, result.error_message().format);
  EXPECT_EQ(1, result.error_message().parameters.size());
  EXPECT_EQ("com.prod.MyClass3", result.error_message().parameters[0]);
}

}  // namespace cdbg
}  // namespace devtools
