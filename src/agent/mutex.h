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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_MUTEX_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_MUTEX_H_

#include <mutex>  // NOLINT
#include "common.h"

namespace absl {

// Wrapper class for non-recursive mutex.
class Mutex {
 public:
  Mutex() {}

  // Block if necessary until this Mutex is free, then acquire it exclusively.
  void Lock() {
    mu_.lock();
  }

  // If possible, acquire this Mutex exclusively without blocking and return
  // true. Otherwise return false. Returns true with high probability if the
  // Mutex was free.
  bool TryLock() {
    return mu_.try_lock();
  }

  // Release this Mutex. Caller must hold it exclusively.
  void Unlock() {
    mu_.unlock();
  }

 private:
  std::mutex mu_;

  DISALLOW_COPY_AND_ASSIGN(Mutex);
};

// Acquires mutex when constructed and releases it when destroyed.
class MutexLock {
 public:
  explicit MutexLock(Mutex* mu) : mu_(mu) {
    mu_->Lock();
  }

  ~MutexLock() {
    mu_->Unlock();
  }

 private:
  Mutex* const mu_;

  DISALLOW_COPY_AND_ASSIGN(MutexLock);
};

}  // namespace absl

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_MUTEX_H_
