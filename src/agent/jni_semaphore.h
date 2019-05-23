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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JNI_SEMAPHORE_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JNI_SEMAPHORE_H_

#include "jni_utils.h"
#include "semaphore.h"

namespace devtools {
namespace cdbg {

// JNI based wrapper of Java Semaphore class. We use Java semaphore as
// opposed to native semaphore implementation because:
// 1. There is no semaphore implementation in the frameworks we use. We could
//    implement one with conditional variable and mutex or wrap sem_XXX
//    functions from pthread library, but that would be more work.
// 2. Unlike native semaphores, wait in Java semaphores can be interrupted.
// 3. It seems to me more natural to use Java facilities in Java thread.
class JniSemaphore : public Semaphore {
 public:
  JniSemaphore() { }

  bool Initialize() override;

  bool Acquire(int timeout_ms) override;

  int DrainPermits() override;

  void Release() override;

 private:
  // Global reference to instance of "java.util.concurrent.Semaphore" object.
  JniGlobalRef semaphore_ { nullptr };

  // Global reference to java.util.concurrent.TimeUnit.MILLISECONDS enum value.
  JniGlobalRef time_unit_ms_ { nullptr };

  // Method ID of Semaphore.tryAcquire(permits) instance method.
  jmethodID try_acquire_method_ { nullptr };

  // Method ID of Semaphore.tryAcquire(permits, timeout, unit) instance method.
  jmethodID try_acquire_timeout_method_ { nullptr };

  // Method ID of Semaphore.drainPermits() instance method.
  jmethodID drain_permits_method_ { nullptr };

  // Method ID of Semaphore.release() instance method.
  jmethodID release_method_ { nullptr };

  DISALLOW_COPY_AND_ASSIGN(JniSemaphore);
};

}  // namespace cdbg
}  // namespace devtools


#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JNI_SEMAPHORE_H_
