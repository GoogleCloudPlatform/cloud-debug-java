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

using google::GetCommandLineOption;

/*
 * Class:     com.google.devtools.cdbg.debuglets.java.GcpEnvironment
 * Method:    getAgentFlag
 * Signature: (Ljava/lang/String;)Ljava/lang/String;
 */
extern "C" JNIEXPORT jobject JNICALL
Java_com_google_devtools_cdbg_debuglets_java_GcpEnvironment_getAgentFlag(
    JNIEnv* jni,
    jclass cls,
    jstring flag) {
  devtools::cdbg::set_thread_jni(jni);

  std::string name = devtools::cdbg::JniToNativeString(flag);
  std::string value;
  if (!GetCommandLineOption(name.c_str(), &value)) {
    LOG(WARNING) << "Flag " << name << " not found";
    return nullptr;  // Flag not found.
  }

  return devtools::cdbg::JniToJavaString(value).release();
}

