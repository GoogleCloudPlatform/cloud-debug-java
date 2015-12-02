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

#include "jni_semaphore.h"

namespace devtools {
namespace cdbg {

bool JniSemaphore::Initialize() {
  // Load java.util.concurrent.Semaphore methods.
  JavaClass semaphore_cls;
  if (!semaphore_cls.FindWithJNI("java/util/concurrent/Semaphore")) {
    return false;
  }

  jmethodID constructor = semaphore_cls.GetInstanceMethod("<init>", "(I)V");
  try_acquire_method_ = semaphore_cls.GetInstanceMethod("tryAcquire", "()Z");
  try_acquire_timeout_method_ = semaphore_cls.GetInstanceMethod(
      "tryAcquire",
      "(IJLjava/util/concurrent/TimeUnit;)Z");
  drain_permits_method_ =
      semaphore_cls.GetInstanceMethod("drainPermits", "()I");
  release_method_ = semaphore_cls.GetInstanceMethod("release", "()V");

  if ((constructor == nullptr) ||
      (try_acquire_method_ == nullptr) ||
      (try_acquire_timeout_method_ == nullptr) ||
      (drain_permits_method_ == nullptr) ||
      (release_method_ == nullptr)) {
    LOG(ERROR) << "java.util.concurrent.Semaphore methods not found";
    return false;
  }

  // Initialize semaphore with zero permits.
  JniLocalRef semaphore_local_ref(
      jni()->NewObject(semaphore_cls.get(), constructor, 0));
  if (!JniCheckNoException("new java.util.concurrent.Semaphore()")) {
    return false;
  }

  semaphore_ = JniNewGlobalRef(semaphore_local_ref.get());
  if (semaphore_ == nullptr) {
    LOG(ERROR) << "java.util.concurrent.Semaphore could not be constructed";
    return false;
  }

  // Load java.util.concurrent.TimeUnit enum values. Unlike in C++ In Java
  // enum values are objects.
  JavaClass time_unit_cls;
  if (!time_unit_cls.FindWithJNI("java/util/concurrent/TimeUnit")) {
    return false;
  }

  time_unit_ms_ = JniNewGlobalRef(
      JniGetEnumValue(time_unit_cls.get(), "MILLISECONDS").get());
  if (time_unit_ms_ == nullptr) {
    return false;
  }

  return true;
}


bool JniSemaphore::Acquire(int timeout_ms) {
  if ((semaphore_ == nullptr) ||
      (try_acquire_method_ == nullptr) ||
      (try_acquire_timeout_method_ == nullptr)) {
    LOG(ERROR) << "Class not initialized";
    return false;
  }

  jboolean rc;
  if (timeout_ms == 0) {
    rc = jni()->CallBooleanMethod(semaphore_.get(), try_acquire_method_);
  } else {
    rc = jni()->CallBooleanMethod(
        semaphore_.get(),
        try_acquire_timeout_method_,
        static_cast<jint>(1),            // permits
        static_cast<jlong>(timeout_ms),  // timeout
        time_unit_ms_.get());            // unit
  }

  if (!JniCheckNoException("java.util.concurrent.Semaphore.tryAcquire")) {
    return false;
  }

  return rc;
}


int JniSemaphore::DrainPermits() {
  if ((semaphore_ == nullptr) || (drain_permits_method_ == nullptr)) {
    LOG(ERROR) << "Class not initialized";
    return false;
  }

  jint count = jni()->CallIntMethod(semaphore_.get(), drain_permits_method_);
  if (!JniCheckNoException("java.util.concurrent.Semaphore.drainPermits")) {
    return 0;
  }

  return count;
}


void JniSemaphore::Release() {
  if ((semaphore_ == nullptr) || (release_method_ == nullptr)) {
    LOG(ERROR) << "Class not initialized";
    return;
  }

  jni()->CallVoidMethod(semaphore_.get(), release_method_);
  JniCheckNoException("java.util.concurrent.Semaphore.release");
}

}  // namespace cdbg
}  // namespace devtools

