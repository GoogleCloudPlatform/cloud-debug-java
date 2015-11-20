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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_SEMAPHORE_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_SEMAPHORE_H_

#include "common.h"

namespace devtools {
namespace cdbg {

// Interface for Java "Semaphore" class. The purpose of this abstraction is to
// mock Java "Semaphore" class in unit tests (instead of initializing JVM).
class Semaphore {
 public:
  virtual ~Semaphore() { }

  // Semaphore initialization.
  virtual bool Initialize() = 0;

  // Acquires one permit from this semaphore, blocking until available. Returns
  // false if the wait timed out or was interrupted.
  virtual bool Acquire(int timeout_ms) = 0;

  // Acquires and returns all permits that are immediately available. If there
  // are no permits available, returns 0 immediately.
  virtual int DrainPermits() = 0;

  // Releases a single permit, returning them to the semaphore. There is no
  // requirement that a thread that releases a permit must have acquired that
  // permit by calling acquire.
  virtual void Release() = 0;
};

}  // namespace cdbg
}  // namespace devtools


#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_SEMAPHORE_H_
