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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_AUTO_RESET_EVENT_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_AUTO_RESET_EVENT_H_

#include <memory>

#include "common.h"
#include "semaphore.h"

namespace devtools {
namespace cdbg {

class Semaphore;

// Notifies a waiting thread that an event has occurred. The current
// implementation only supports a single thread to wait on the event.
// The class is thread safe.
class AutoResetEvent {
 public:
  explicit AutoResetEvent(std::unique_ptr<Semaphore> semaphore)
      : semaphore_(std::move(semaphore)) {
  }

  // Initializes the underlying semaphore.
  bool Initialize() {
    return semaphore_->Initialize();
  }

  // Sets the event to a signalled state. If the event is already in signalled
  // state, this call has no effect. If there is a thread waiting on this
  // event, the wait will complete. If a thread will start waiting in the
  // future, it the wait will return immediately.
  void Signal()  {
    semaphore_->Release();
  }

  // Waits for a the event to transition to a signalled state. If the event
  // is in signalled state when this function is called, the wait is completed
  // immediately. Returns false if the wait timed out or was interrupted. If
  // successful, the resets the state of the event to non-signalled.
  bool Wait(int timeout_ms) {
    if (!semaphore_->Acquire(timeout_ms)) {
      return false;
    }

    semaphore_->DrainPermits();

    return true;
  }

 private:
  std::unique_ptr<Semaphore> semaphore_;

  DISALLOW_COPY_AND_ASSIGN(AutoResetEvent);
};

}  // namespace cdbg
}  // namespace devtools


#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_AUTO_RESET_EVENT_H_
