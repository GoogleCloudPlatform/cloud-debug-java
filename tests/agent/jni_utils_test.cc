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

#include "src/agent/jni_utils.h"

#include "gtest/gtest.h"
#include "mock_object.h"
#include "mock_printwriter.h"
#include "mock_stringwriter.h"
#include "mock_throwable.h"
#include "tests/agent/fake_jni.h"
#include "tests/agent/mock_jvmti_env.h"

using testing::NiceMock;

namespace devtools {
namespace cdbg {

class JniUtilsTest : public ::testing::Test {
 protected:
  JniUtilsTest() : global_jvm_(fake_jni_.jvmti(), fake_jni_.jni()) {}

  void SetUp() override {
    jniproxy::InjectObject(&object_);
    jniproxy::InjectPrintWriter(&print_writer_);
    jniproxy::InjectStringWriter(&string_writer_);
    jniproxy::InjectThrowable(&throwable_);
  }

  void TearDown() override {
    jniproxy::InjectObject(nullptr);
    jniproxy::InjectPrintWriter(nullptr);
    jniproxy::InjectStringWriter(nullptr);
    jniproxy::InjectThrowable(nullptr);
  }

 protected:
  FakeJni fake_jni_;
  GlobalJvmEnv global_jvm_;
  NiceMock<jniproxy::MockObject> object_;
  NiceMock<jniproxy::MockPrintWriter> print_writer_;
  NiceMock<jniproxy::MockStringWriter> string_writer_;
  NiceMock<jniproxy::MockThrowable> throwable_;
};


TEST_F(JniUtilsTest, JniLocalRef_DefaultConstructor) {
  JniLocalRef ref;
  EXPECT_EQ(nullptr, ref.get());
  EXPECT_EQ(nullptr, ref);
}


TEST_F(JniUtilsTest, JniLocalRef_AttachRef) {
  JniLocalRef ref(fake_jni_.CreateNewJavaString("abc"));

  EXPECT_NE(nullptr, ref.get());
  EXPECT_NE(nullptr, ref);
  EXPECT_EQ("abc", JniToNativeString(ref.get()));
}


TEST_F(JniUtilsTest, JniLocalRef_Reset) {
  JniLocalRef ref(fake_jni_.CreateNewJavaString("abc"));

  EXPECT_NE(nullptr, ref);

  ref.reset();

  EXPECT_EQ(nullptr, ref);
}


TEST_F(JniUtilsTest, JniLocalRef_Swap) {
  JniLocalRef ref1(fake_jni_.CreateNewJavaString("abc"));
  JniLocalRef ref2(fake_jni_.CreateNewJavaString("def"));

  ref1.swap(ref2);

  EXPECT_EQ("def", JniToNativeString(ref1.get()));
  EXPECT_EQ("abc", JniToNativeString(ref2.get()));
}


TEST_F(JniUtilsTest, JniLocalRef_Release) {
  JniLocalRef ref1(fake_jni_.CreateNewJavaString("abc"));
  JniLocalRef ref2(ref1.release());

  EXPECT_EQ(nullptr, ref1);
  EXPECT_EQ("abc", JniToNativeString(ref2.get()));
}


TEST_F(JniUtilsTest, JniLocalRef_ResetOnAttach) {
  JniLocalRef ref;
  ref = JniLocalRef(fake_jni_.CreateNewJavaString("abc"));
  ref = JniLocalRef(fake_jni_.CreateNewJavaString("def"));

  EXPECT_EQ("def", JniToNativeString(ref.get()));

  // "FakeJni" verifies that all the references were properly cleaned up.
}


TEST_F(JniUtilsTest, ExceptionOr_NoException) {
  ExceptionOr<int> e = CatchOr(nullptr, 123);
  EXPECT_FALSE(e.HasException());
  EXPECT_EQ(nullptr, e.GetException());
  EXPECT_EQ(123, e.GetData());
  EXPECT_EQ(123, e.Release(ExceptionAction::IGNORE));
  e.LogException();
  EXPECT_EQ(123, e.Release(ExceptionAction::LOG_AND_IGNORE));
}


TEST_F(JniUtilsTest, ExceptionOr_WithException_NoLogContext) {
  JniLocalRef exception(
      fake_jni_.CreateNewObject(FakeJni::StockClass::Object));
  EXPECT_EQ(0, jni()->Throw(static_cast<jthrowable>(exception.get())));

  ExceptionOr<int> e = CatchOr(nullptr, 123);
  EXPECT_FALSE(jni()->ExceptionCheck());

  EXPECT_TRUE(e.HasException());
  EXPECT_TRUE(jni()->IsSameObject(exception.get(), e.GetException()));
  EXPECT_EQ(0, e.Release(ExceptionAction::IGNORE));
  e.LogException();
  EXPECT_EQ(0, e.Release(ExceptionAction::LOG_AND_IGNORE));
}


TEST_F(JniUtilsTest, ExceptionOr_WithException_WithContext) {
  JniLocalRef exception(
      fake_jni_.CreateNewObject(FakeJni::StockClass::Object));
  EXPECT_EQ(0, jni()->Throw(static_cast<jthrowable>(exception.get())));

  ExceptionOr<int> e = CatchOr("unit test", 123);
  EXPECT_FALSE(jni()->ExceptionCheck());

  e.LogException();
  EXPECT_EQ(0, e.Release(ExceptionAction::LOG_AND_IGNORE));
}


TEST_F(JniUtilsTest, ExceptionOr_UniquePtr) {
  ExceptionOr<std::unique_ptr<int>> e =
      CatchOr(nullptr, std::unique_ptr<int>(new int(123)));

  EXPECT_FALSE(e.HasException());
  EXPECT_EQ(nullptr, e.GetException());
  EXPECT_EQ(123, *e.GetData());
  e.LogException();
  EXPECT_EQ(123, *e.Release(ExceptionAction::IGNORE));
}


TEST_F(JniUtilsTest, ExceptionOr_Nothing) {
  ExceptionOr<std::nullptr_t> e = CatchOr<std::nullptr_t>(nullptr, nullptr);

  EXPECT_FALSE(e.HasException());
  EXPECT_EQ(nullptr, e.GetException());
}

}  // namespace cdbg
}  // namespace devtools

