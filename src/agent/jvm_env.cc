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

#include <map>

#include "jni_proxy_arithmeticexception.h"
#include "jni_proxy_bigdecimal.h"
#include "jni_proxy_biginteger.h"
#include "jni_proxy_class.h"
#include "jni_proxy_classcastexception.h"
#include "jni_proxy_classloader.h"
#include "jni_proxy_exception.h"
#include "jni_proxy_iterable.h"
#include "jni_proxy_ju_hashmap.h"
#include "jni_proxy_ju_map.h"
#include "jni_proxy_ju_map_entry.h"
#include "jni_proxy_jul_logger.h"
#include "jni_proxy_negativearraysizeexception.h"
#include "jni_proxy_nullpointerexception.h"
#include "jni_proxy_object.h"
#include "jni_proxy_printwriter.h"
#include "jni_proxy_string.h"
#include "jni_proxy_stringwriter.h"
#include "jni_proxy_thread.h"
#include "jni_proxy_throwable.h"
#include "common.h"

namespace devtools {
namespace cdbg {

static jvmtiEnv* g_jvmti = nullptr;
static __thread JNIEnv* g_jni = nullptr;

// Global reference to system class loader.
static jobject g_system_class_loader = nullptr;

jvmtiEnv* jvmti() {
  return g_jvmti;
}


JNIEnv* jni() {
  return g_jni;
}


void set_jvmti(jvmtiEnv* jvmti) {
  g_jvmti = jvmti;
}


JNIEnv* set_thread_jni(JNIEnv* jni) {
  JNIEnv* previous_jni = g_jni;
  g_jni = jni;
  return previous_jni;
}


bool BindSystemClasses() {
  if (!jniproxy::BindArithmeticException() ||
      !jniproxy::BindBigDecimal() ||
      !jniproxy::BindBigInteger() ||
      !jniproxy::BindClass() ||
      !jniproxy::BindClassCastException() ||
      !jniproxy::BindClassLoader() ||
      !jniproxy::BindException() ||
      !jniproxy::BindHashMap() ||
      !jniproxy::BindIterable() ||
      !jniproxy::BindLogger() ||
      !jniproxy::BindMap() ||
      !jniproxy::BindMap_Entry() ||
      !jniproxy::BindNegativeArraySizeException() ||
      !jniproxy::BindNullPointerException() ||
      !jniproxy::BindObject() ||
      !jniproxy::BindPrintWriter() ||
      !jniproxy::BindString() ||
      !jniproxy::BindStringWriter() ||
      !jniproxy::BindThread() ||
      !jniproxy::BindThrowable()) {
    return false;
  }

  const JniLocalRef& system_class_loader =
      jniproxy::ClassLoader()->getSystemClassLoader()
      .Release(ExceptionAction::LOG_AND_IGNORE);
  if (system_class_loader == nullptr) {
    LOG(ERROR) << "Failed to obtain reference to system class loader";
    return false;
  }

  g_system_class_loader = jni()->NewGlobalRef(system_class_loader.get());

  return true;
}


void CleanupSystemClasses() {
  jniproxy::CleanupArithmeticException();
  jniproxy::CleanupBigDecimal();
  jniproxy::CleanupBigInteger();
  jniproxy::CleanupClass();
  jniproxy::CleanupClassCastException();
  jniproxy::CleanupClassLoader();
  jniproxy::CleanupException();
  jniproxy::CleanupHashMap();
  jniproxy::CleanupIterable();
  jniproxy::CleanupLogger();
  jniproxy::CleanupMap();
  jniproxy::CleanupMap_Entry();
  jniproxy::CleanupNegativeArraySizeException();
  jniproxy::CleanupNullPointerException();
  jniproxy::CleanupObject();
  jniproxy::CleanupPrintWriter();
  jniproxy::CleanupString();
  jniproxy::CleanupStringWriter();
  jniproxy::CleanupThread();
  jniproxy::CleanupThrowable();

  if (g_system_class_loader != nullptr) {
    jni()->DeleteGlobalRef(g_system_class_loader);
    g_system_class_loader = nullptr;
  }
}


jobject GetSystemClassLoader() {
  return g_system_class_loader;
}


}  // namespace cdbg
}  // namespace devtools
