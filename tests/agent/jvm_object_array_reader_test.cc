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

#include "src/agent/jvm_object_array_reader.h"

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
using testing::StrictMock;

namespace devtools {
namespace cdbg {

class JvmObjectArrayReaderTest : public ::testing::Test {
 protected:
  JvmObjectArrayReaderTest()
      : fake_jni_(&jni_),
        global_jvm_(fake_jni_.jvmti(), &jni_) {
  }

 protected:
  NiceMock<MockJNIEnv> jni_;
  FakeJni fake_jni_;
  GlobalJvmEnv global_jvm_;
};


TEST_F(JvmObjectArrayReaderTest, Success) {
  JVariant source;
  source.attach_ref(
      JVariant::ReferenceKind::Local,
      fake_jni_.CreateNewJavaString("a"));

  JVariant index = JVariant::Long(18);

  jobject array_element_object =
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass2);

  EXPECT_CALL(jni_, GetObjectArrayElement(NotNull(), 18))
      .WillOnce(Return(array_element_object));

  JvmObjectArrayReader reader;
  ErrorOr<JVariant> result = reader.ReadValue(source, index);

  EXPECT_FALSE(result.is_error());

  jobject obj = nullptr;
  EXPECT_TRUE(result.value().get<jobject>(&obj));
  EXPECT_NE(nullptr, obj);
  EXPECT_TRUE(jni_.GetObjectRefType(obj) == JNIGlobalRefType);
}


TEST_F(JvmObjectArrayReaderTest, BadSourceObject) {
  JVariant source = JVariant::Boolean(true);
  JVariant index = JVariant::Long(18);

  JvmObjectArrayReader reader;
  ErrorOr<JVariant> result = reader.ReadValue(source, index);

  EXPECT_TRUE(result.is_error());
}


TEST_F(JvmObjectArrayReaderTest, BadIndex) {
  JVariant source;
  source.attach_ref(
      JVariant::ReferenceKind::Local,
      fake_jni_.CreateNewJavaString("a"));

  JVariant index;
  index.attach_ref(JVariant::ReferenceKind::Global, nullptr);

  JvmObjectArrayReader reader;
  ErrorOr<JVariant> result = reader.ReadValue(source, index);

  EXPECT_TRUE(result.is_error());
}


TEST_F(JvmObjectArrayReaderTest, NullSourceObject) {
  JVariant source = JVariant::Null();
  JVariant index = JVariant::Long(18);

  JvmObjectArrayReader reader;
  ErrorOr<JVariant> result = reader.ReadValue(source, index);

  EXPECT_TRUE(result.is_error());
  EXPECT_EQ(NullPointerDereference, result.error_message().format);
}


TEST_F(JvmObjectArrayReaderTest, AccessException) {
  JVariant source = JVariant::LocalRef(
      fake_jni_.CreateNewJavaString("a"));

  JVariant index = JVariant::Long(18);

  EXPECT_CALL(jni_, GetObjectArrayElement(NotNull(), 18))
      .WillOnce(Return(nullptr));

  JniLocalRef exception_object(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass3));

  EXPECT_CALL(jni_, ExceptionCheck())
      .WillOnce(Return(true));

  EXPECT_CALL(jni_, ExceptionOccurred())
      .WillOnce(Return(static_cast<jthrowable>(
          jni_.NewLocalRef(exception_object.get()))));

  JvmObjectArrayReader reader;
  ErrorOr<JVariant> result = reader.ReadValue(source, index);

  EXPECT_TRUE(result.is_error());
  EXPECT_EQ(MethodCallExceptionOccurred, result.error_message().format);
  EXPECT_EQ(1, result.error_message().parameters.size());
  EXPECT_EQ("com.prod.MyClass3", result.error_message().parameters[0]);
}

}  // namespace cdbg
}  // namespace devtools
