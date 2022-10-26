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

#include <memory>
#include <vector>

#include "src/agent//common.h"
#include "src/agent//jni_utils.h"
#include "src/agent//mutex.h"

using devtools::cdbg::JniToNativeString;
using devtools::cdbg::JniToJavaString;

static struct LogState {
  absl::Mutex mu;
  std::string log_buffer;
} *g_state;

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
  std::vector<char*> argv_vector;
  argv_vector.push_back(const_cast<char*>("test_logger"));

  int argc = argv_vector.size();
  char** argv = &argv_vector[0];

  g_state = new LogState;
  return JNI_VERSION_1_6;
}


extern "C" void JNIEXPORT JNICALL
JNI_OnUnLoad(JavaVM* jvm, void* reserved) {
  delete g_state;
}


extern "C" JNIEXPORT void JNICALL
Java_com_google_devtools_cdbg_debuglets_java_AgentLogger_info(
    JNIEnv* jni,
    jclass cls,
    jstring message) {
  devtools::cdbg::set_thread_jni(jni);

  absl::MutexLock lock(&g_state->mu);

  g_state->log_buffer += "[INFO] ";
  g_state->log_buffer += JniToNativeString(message);
  g_state->log_buffer += "\n";
}


extern "C" JNIEXPORT void JNICALL
Java_com_google_devtools_cdbg_debuglets_java_AgentLogger_warn(
    JNIEnv* jni,
    jclass cls,
    jstring message) {
  devtools::cdbg::set_thread_jni(jni);

  absl::MutexLock lock(&g_state->mu);

  g_state->log_buffer += "[WARNING] ";
  g_state->log_buffer += JniToNativeString(message);
  g_state->log_buffer += "\n";
}


extern "C" JNIEXPORT void JNICALL
Java_com_google_devtools_cdbg_debuglets_java_AgentLogger_severe(
    JNIEnv* jni,
    jclass cls,
    jstring message) {
  devtools::cdbg::set_thread_jni(jni);

  absl::MutexLock lock(&g_state->mu);

  g_state->log_buffer += "[ERROR] ";
  g_state->log_buffer += JniToNativeString(message);
  g_state->log_buffer += "\n";
}


extern "C" JNIEXPORT jstring JNICALL
Java_com_google_devtools_cdbg_debuglets_java_AgentLoggerTest_pull(
    JNIEnv* jni,
    jclass cls,
    jstring message) {
  devtools::cdbg::set_thread_jni(jni);

  std::string prev;
  {
    absl::MutexLock lock(&g_state->mu);
    g_state->log_buffer.swap(prev);
  }

  return static_cast<jstring>(JniToJavaString(prev).release());
}
