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

#include "src/agent/common.h"
#include "src/agent/test_util/mock_jvmti_env.h"

namespace devtools {
namespace cdbg {

// Either the test mocks are thread safe or the test is single threaded. Either
// way we don't need to bother with thread local storage here.
static jvmtiEnv* g_jvmti = nullptr;
static JNIEnv* g_jni = nullptr;

jvmtiEnv* jvmti() {
  CHECK(g_jvmti != nullptr);
  return g_jvmti;
}

JNIEnv* jni() {
  CHECK(g_jni != nullptr);
  return g_jni;
}

JNIEnv* set_thread_jni(JNIEnv* jni) {
  CHECK_EQ(g_jni, jni);
  return g_jni;
}


bool BindSystemClasses() {
  return true;
}


void CleanupSystemClasses() {}


jobject GetSystemClassLoader() {
  return nullptr;
}


GlobalJvmEnv::GlobalJvmEnv(jvmtiEnv* jvmti, JNIEnv* jni) {
  CHECK(g_jvmti == nullptr);
  CHECK(g_jni == nullptr);

  g_jvmti = jvmti;
  g_jni = jni;
}


GlobalJvmEnv::~GlobalJvmEnv() {
  DCHECK(g_jvmti != nullptr);
  DCHECK(g_jni != nullptr);

  g_jvmti = nullptr;
  g_jni = nullptr;
}


GlobalNoJni::GlobalNoJni()
    : original_jni_(jni()) {
  g_jni = nullptr;
}

GlobalNoJni::~GlobalNoJni() {
  g_jni = original_jni_;
}


}  // namespace cdbg
}  // namespace devtools

