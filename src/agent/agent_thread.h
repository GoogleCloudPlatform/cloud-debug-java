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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_AGENT_THREAD_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_AGENT_THREAD_H_

#include <functional>

#include "common.h"

namespace devtools {
namespace cdbg {

// Represents debugger worker thread. Operations on this class are not thread
// safe. Once the thread has been started, the caller must eventually call
// the "Join" method to wait for the thread to terminate and release all the
// references.
// This class is not thead safe.
class AgentThread {
 public:
  virtual ~AgentThread() { }

  // Starts the thread. The "thread_name" argument is only used for logging.
  virtual bool Start(const std::string& thread_name,
                     std::function<void()> thread_proc) = 0;

  // Checks whether "Start" has been previously called.
  virtual bool IsStarted() const = 0;

  // Waits for the thread to complete and then releases all the references.
  virtual void Join() = 0;

  // Stalls the thread that called "Sleep" function. This might not be the
  // thread created by "Start" function. The function may return prematurely
  // if the sleep was interrupted.
  virtual void Sleep(int ms) = 0;
};

}  // namespace cdbg
}  // namespace devtools


#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_AGENT_THREAD_H_
