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
#include "gmock/gmock.h"

using devtools::cdbg::set_thread_jni;

/**
 * Called when the Java code does the System.loadLibrary() call.
 */
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
  // Get JVMTI interface.
  jvmtiEnv* jvmti = nullptr;
  int err = vm->GetEnv(reinterpret_cast<void**>(&jvmti), JVMTI_VERSION);
  if (err != JNI_OK) {
    return 1;
  }

  devtools::cdbg::set_jvmti(jvmti);

  // Per the spec, the return value here indicates we aren't using any JVMTI
  // methods specified in JVM versions later than the given version.  To note,
  // we may in fact only be using methods from an even earlier version, but this
  // is safe upperbound to report.
  return JNI_VERSION_1_8;
}

/*
 * Class:     JvmTestMain
 * Method:    run
 * Signature: ()V
 */
extern "C" JNIEXPORT void JNICALL Java_JvmTestMain_run(JNIEnv* jni, jclass) {
  absl::SetFlag(&FLAGS_logtostderr, true);
  set_thread_jni(jni);

  ::testing::InitGoogleTest();
  int rc = RUN_ALL_TESTS();
  CHECK_EQ(0, rc);
}
