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

#include "src/agent/jobject_map.h"

#include <cstdint>

#include "gmock/gmock.h"

#include "src/agent/test_util/mock_jni_env.h"
#include "src/agent/test_util/mock_jvmti_env.h"

using testing::_;
using testing::Eq;
using testing::IsNull;
using testing::NotNull;
using testing::Test;
using testing::Return;
using testing::ReturnNew;
using testing::SaveArg;
using testing::WithArgs;
using testing::Invoke;
using testing::InvokeArgument;
using testing::SetArgPointee;

namespace devtools {
namespace cdbg {

class JobjectMapTest : public ::testing::Test {
 protected:
  JobjectMapTest() : global_jvm_(&jvmti_, &jni_) {
  }

  static jobject GetObj(int obj_id, int ref_id) {
    // Make sure we never return NULL.
    return reinterpret_cast<jobject>((obj_id << 16) | (ref_id << 1) | 1);
  }

  static jvmtiError GetObjectHashCode(jobject obj, jint* hash_code) {
    *hash_code = reinterpret_cast<uint64_t>(obj) >> 18;
    return JVMTI_ERROR_NONE;
  }

  static bool IsSameObject(jobject obj1, jobject obj2) {
    if ((obj1 == nullptr) && (obj2 == nullptr)) {
      return true;
    }

    if ((obj1 == nullptr) || (obj2 == nullptr)) {
      return false;
    }

    return (reinterpret_cast<uint64_t>(obj1) >> 16) ==
           (reinterpret_cast<uint64_t>(obj2) >> 16);
  }

  void SetUp() override {
    EXPECT_CALL(jvmti_, GetObjectHashCode(_, NotNull()))
      .WillRepeatedly(Invoke(GetObjectHashCode));

    EXPECT_CALL(jni_, IsSameObject(_, _))
      .WillRepeatedly(Invoke(IsSameObject));
  }

 protected:
  MockJvmtiEnv jvmti_;
  MockJNIEnv jni_;
  GlobalJvmEnv global_jvm_;
};


TEST_F(JobjectMapTest, Lookup_Empty_NotFound) {
  JobjectMap<JObject_WeakRef, int> m;
  EXPECT_FALSE(m.Contains(GetObj(1, 1)));
}


TEST_F(JobjectMapTest, InsertAndLookup) {
  JobjectMap<JObject_NoRef, int> m;

  // First insert should succeed.
  for (int obj_id = 0; obj_id < 17; ++obj_id) {
    EXPECT_TRUE(m.Insert(GetObj(obj_id, 1), obj_id + 1234));
  }

  // Second insert should now fail. Note that jobject value is now different,
  // but references the same Java object.
  for (int obj_id = 0; obj_id < 17; ++obj_id) {
    EXPECT_FALSE(m.Insert(GetObj(obj_id, 2), 0));
  }

  for (int obj_id = 0; obj_id < 17; ++obj_id) {
    for (int ref_id = 0; ref_id < 100; ++ref_id) {
      ASSERT_TRUE(m.Contains(GetObj(obj_id, ref_id)));
      EXPECT_EQ(obj_id + 1234, *m.Find(GetObj(obj_id, ref_id)));
    }
  }

  for (int ref_id = 0; ref_id < 100; ++ref_id) {
    ASSERT_FALSE(m.Contains(GetObj(18, 1)));
    ASSERT_EQ(nullptr, m.Find(GetObj(18, 1)));
  }
}


TEST_F(JobjectMapTest, Remove) {
  JobjectMap<JObject_NoRef, int> m;

  EXPECT_TRUE(m.Insert(GetObj(1, 1), 0));
  EXPECT_TRUE(m.Insert(GetObj(2, 1), 0));
  EXPECT_TRUE(m.Insert(GetObj(3, 1), 0));

  EXPECT_TRUE(m.Remove(GetObj(2, 18)));
  EXPECT_FALSE(m.Remove(GetObj(4, 18)));

  EXPECT_FALSE(m.Contains(GetObj(2, 11)));
  EXPECT_TRUE(m.Contains(GetObj(3, 11)));

  m.RemoveAll();

  EXPECT_FALSE(m.Contains(GetObj(3, 11)));

  m.RemoveAll();
}


TEST_F(JobjectMapTest, WeakRef_Insert_RemoveAll) {
  JobjectMap<JObject_WeakRef, int> m;

  EXPECT_CALL(jni_, NewWeakGlobalRef(GetObj(1, 1)))
      .WillOnce(Return(GetObj(1, 10)));

  EXPECT_TRUE(m.Insert(GetObj(1, 1), 0));

  EXPECT_CALL(jni_, DeleteWeakGlobalRef(GetObj(1, 10))).Times(1);

  m.RemoveAll();
}


TEST_F(JobjectMapTest, WeakRef_Insert_Remove) {
  JobjectMap<JObject_WeakRef, int> m;

  EXPECT_CALL(jni_, NewWeakGlobalRef(GetObj(1, 1)))
      .WillOnce(Return(GetObj(1, 10)));

  EXPECT_TRUE(m.Insert(GetObj(1, 1), 0));

  EXPECT_CALL(jni_, DeleteWeakGlobalRef(GetObj(1, 10))).Times(1);

  m.Remove(GetObj(1, 1));

  m.RemoveAll();
}


TEST_F(JobjectMapTest, GlobalRef_Insert_Remove) {
  JobjectMap<JObject_GlobalRef, int> m;

  EXPECT_CALL(jni_, NewGlobalRef(GetObj(1, 1)))
      .WillOnce(Return(GetObj(1, 10)));

  EXPECT_TRUE(m.Insert(GetObj(1, 1), 0));

  EXPECT_CALL(jni_, DeleteGlobalRef(GetObj(1, 10))).Times(1);

  m.Remove(GetObj(1, 1));

  m.RemoveAll();
}


TEST_F(JobjectMapTest, GlobalRef_Insert_RemoveAll) {
  JobjectMap<JObject_GlobalRef, int> m;

  EXPECT_CALL(jni_, NewGlobalRef(GetObj(1, 1)))
      .WillOnce(Return(GetObj(1, 10)));

  EXPECT_TRUE(m.Insert(GetObj(1, 1), 0));

  EXPECT_CALL(jni_, DeleteGlobalRef(GetObj(1, 10))).Times(1);

  m.RemoveAll();
}



}  // namespace cdbg
}  // namespace devtools
