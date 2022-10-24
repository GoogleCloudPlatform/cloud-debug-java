/**
 * Copyright 2021 Google Inc. All Rights Reserved.
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

#include "debuggee_labels.h"

#include <iostream>

#include "gmock/gmock.h"

#include "mock_ju_hashmap.h"
#include "jni_utils.h"
#include "src/agent/test_util/fake_jni.h"
#include "src/agent/test_util/mock_jni_env.h"
#include "src/agent/test_util/mock_jvmti_env.h"

namespace devtools {
namespace cdbg {

using testing::_;
using testing::ByMove;
using testing::Eq;
using testing::NiceMock;
using testing::Return;
using testing::StrEq;

class DebuggeeLabelsTest : public ::testing::Test {
 protected:
  DebuggeeLabelsTest() : global_jvm_(&jvmti_env_, &jni_env_full_) {}

  void SetUp() override { jniproxy::InjectHashMap(&hash_map_); }

  void TearDown() override { jniproxy::InjectHashMap(nullptr); }

  NiceMock<jniproxy::MockHashMap> hash_map_;
  NiceMock<MockJNIEnv> jni_env_full_;
  NiceMock<MockJvmtiEnv> jvmti_env_;
  GlobalJvmEnv global_jvm_;
};

TEST_F(DebuggeeLabelsTest, ExpectedLabelsMakeItToJava) {
  std::string name1 = "foo1";
  std::string name2 = "foo2";
  std::string value1 = "bar1";
  std::string value2 = "bar2";

  jstring name1_jstr = reinterpret_cast<jstring>(&name1);
  jstring name2_jstr = reinterpret_cast<jstring>(&name2);
  jstring value1_jstr = reinterpret_cast<jstring>(&value1);
  jstring value2_jstr = reinterpret_cast<jstring>(&value2);

  ExceptionOr<JniLocalRef> put1_ret(nullptr, nullptr, JniLocalRef());
  ExceptionOr<JniLocalRef> put2_ret(nullptr, nullptr, JniLocalRef());

  jobject hash_map_instance = reinterpret_cast<jobject>(0x1ULL);
  ExceptionOr<JniLocalRef> new_object_ret(nullptr, nullptr,
                                          JniLocalRef(hash_map_instance));

  EXPECT_CALL(hash_map_, NewObject())
      .WillOnce(Return(ByMove(std::move(new_object_ret))));

  EXPECT_CALL(jni_env_full_, NewStringUTF(StrEq(name1)))
      .WillOnce(Return(name1_jstr));

  EXPECT_CALL(jni_env_full_, NewStringUTF(StrEq(name2)))
      .WillOnce(Return(name2_jstr));

  EXPECT_CALL(jni_env_full_, NewStringUTF(StrEq(value1)))
      .WillOnce(Return(value1_jstr));

  EXPECT_CALL(jni_env_full_, NewStringUTF(StrEq(value2)))
      .WillOnce(Return(value2_jstr));

  EXPECT_CALL(hash_map_, put(hash_map_instance, name1_jstr, value1_jstr))
      .WillOnce(Return(ByMove(std::move(put1_ret))));

  EXPECT_CALL(hash_map_, put(hash_map_instance, name2_jstr, value2_jstr))
      .WillOnce(Return(ByMove(std::move(put2_ret))));

  DebuggeeLabels labels;
  labels.Set(name1, value1);
  labels.Set(name2, value2);
  EXPECT_THAT(labels.Get().get(), Eq(hash_map_instance));
}

TEST_F(DebuggeeLabelsTest, HandlesNewHashMapFailure) {
  ExceptionOr<JniLocalRef> new_object_ret(
      nullptr, JniLocalRef(reinterpret_cast<jobject>(0x1ULL)), JniLocalRef());

  EXPECT_CALL(hash_map_, NewObject())
      .WillOnce(Return(ByMove(std::move(new_object_ret))));

  EXPECT_CALL(jni_env_full_, NewStringUTF(_)).Times(0);
  EXPECT_CALL(hash_map_, put(_, _, _)).Times(0);

  DebuggeeLabels labels;
  labels.Set("foo", "bar");
  labels.Get();
}

TEST_F(DebuggeeLabelsTest, HandlesHashMapPutFailure) {
  ExceptionOr<JniLocalRef> put_ret(
      nullptr, JniLocalRef(reinterpret_cast<jobject>(0x1ULL)), JniLocalRef());

  std::string name1 = "foo1";
  std::string name2 = "foo2";
  std::string value1 = "bar1";
  std::string value2 = "bar2";

  jstring name1_jstr = reinterpret_cast<jstring>(&name1);
  jstring value1_jstr = reinterpret_cast<jstring>(&value1);

  // We expect the first label to be iterated, but we don't know or care the
  // order (ie foo1 or foo2 could be first, it doesn't matter, what matter is
  // is things stop after the first label since the java hashmap put will fail
  // for that first label.
  EXPECT_CALL(jni_env_full_, NewStringUTF(_))
      .Times(2)
      .WillOnce(Return(name1_jstr))
      .WillOnce(Return(value1_jstr));

  // After the first put failure there should be no further put calls.
  EXPECT_CALL(hash_map_, put(_, _, _))
      .Times(1)
      .WillOnce(Return(ByMove(std::move(put_ret))));

  DebuggeeLabels labels;
  labels.Set(name1, value1);
  labels.Set(name2, value2);
  EXPECT_THAT(labels.Get().get(), Eq(nullptr));
}

}  // namespace cdbg
}  // namespace devtools
