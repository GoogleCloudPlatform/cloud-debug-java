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

#include "common.h"
#include "jni_utils.h"

/*
 * Class:     com.google.devtools.cdbg.debuglets.java.AgentLogger
 * Method:    info
 * Signature: (Ljava/lang/String;)V
 */
extern "C" JNIEXPORT void JNICALL
Java_com_google_devtools_cdbg_debuglets_java_AgentLogger_info(
    JNIEnv* jni,
    jclass cls,
    jstring message) {
  devtools::cdbg::set_thread_jni(jni);
  LOG(INFO) << devtools::cdbg::JniToNativeString(message);
}


/*
 * Class:     com.google.devtools.cdbg.debuglets.java.AgentLogger
 * Method:    warn
 * Signature: (Ljava/lang/String;)V
 */
extern "C" JNIEXPORT void JNICALL
Java_com_google_devtools_cdbg_debuglets_java_AgentLogger_warn(
    JNIEnv* jni,
    jclass cls,
    jstring message) {
  devtools::cdbg::set_thread_jni(jni);
  LOG(WARNING) << devtools::cdbg::JniToNativeString(message);
}


/*
 * Class:     com.google.devtools.cdbg.debuglets.java.AgentLogger
 * Method:    severe
 * Signature: (Ljava/lang/String;)V
 */
extern "C" JNIEXPORT void JNICALL
Java_com_google_devtools_cdbg_debuglets_java_AgentLogger_severe(
    JNIEnv* jni,
    jclass cls,
    jstring message) {
  devtools::cdbg::set_thread_jni(jni);
  LOG(ERROR) << devtools::cdbg::JniToNativeString(message);
}
