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
