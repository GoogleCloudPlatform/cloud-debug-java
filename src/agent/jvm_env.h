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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_ENV_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_ENV_H_

namespace devtools {
namespace cdbg {

// Maximum time we allow the Cloud Debugger to spend inside a callback. Beyond
// that we declare the agent as unhealthy. This is used for breakpoints canary.
// The interval of 5 seconds is way longer than anything that the debugger
// will ever take, but we need to account for potential GC cycles that
// may interrupt the debugger operation.
constexpr int kDefaultMaxCallbackTimeMs = 5000;

// Gets the global instance of JVMTI interface.
jvmtiEnv* jvmti();

// Gets the JNI interface instance for the current thread.
JNIEnv* jni();

// Sets the global instance of JVMTI interface. Needs to be called only once.
void set_jvmti(jvmtiEnv* jvmti);

// Associates the JNI environment with the current thread.
JNIEnv* set_thread_jni(JNIEnv* jni);

// Loads references to system classes.
bool BindSystemClasses();

// Releases up references to system classes.
void CleanupSystemClasses();

// Gets the cached return value of ClassLoader.getSystemClassLoader().
jobject GetSystemClassLoader();

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_ENV_H_
