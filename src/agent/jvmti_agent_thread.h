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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVMTI_AGENT_THREAD_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVMTI_AGENT_THREAD_H_

#include "agent_thread.h"
#include "common.h"
#include "jni_utils.h"

namespace devtools {
namespace cdbg {

// Implements JVMTI agent thread.
class JvmtiAgentThread : public AgentThread {
 public:
  JvmtiAgentThread();
  ~JvmtiAgentThread() override;

  bool Start(const std::string& thread_name,
             std::function<void()> thread_proc) override;

  bool IsStarted() const override { return thread_ != nullptr; }

  void Join() override;

  void Sleep(int ms) override;

  // Returns true if the current thread is JVMTI agent thread created by
  // this class.
  static bool IsInAgentThread();

 private:
  // Creates the thread objects and starts the agent thread.
  bool StartAgentThread(const std::string& thread_name,
                        std::function<void()> thread_proc);

 private:
  // Global reference to Java thread object.
  JniGlobalRef thread_;

  DISALLOW_COPY_AND_ASSIGN(JvmtiAgentThread);
};

}  // namespace cdbg
}  // namespace devtools


#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVMTI_AGENT_THREAD_H_
