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

#include "jvmti_agent_thread.h"

#include "jni_proxy_thread.h"

namespace devtools {
namespace cdbg {

// Thread local variable indicating whether the current thread is JVMTI
// agent thread created by "JvmtiAgentThread".
static __thread bool g_is_agent_thread = false;


JvmtiAgentThread::JvmtiAgentThread() {
}


JvmtiAgentThread::~JvmtiAgentThread() {
  // The caller is expected to wait for the worker thread terminates before
  // this class goes away.
  DCHECK(thread_ == nullptr) << "Agent thread abandoned";
}

bool JvmtiAgentThread::Start(const std::string& thread_name,
                             std::function<void()> thread_proc) {
  if (thread_ != nullptr) {
    LOG(ERROR) << "Thread already running";
    return false;
  }

  if (!StartAgentThread(thread_name, thread_proc)) {
    return false;
  }

  return true;
}

void JvmtiAgentThread::Join() {
  if (thread_ == nullptr) {
    return;
  }

  jniproxy::Thread()->join(thread_.get());
  thread_ = nullptr;
}


void JvmtiAgentThread::Sleep(int ms) {
  jniproxy::Thread()->sleep(ms);
}

bool JvmtiAgentThread::StartAgentThread(const std::string& thread_name,
                                        std::function<void()> thread_proc) {
  thread_ = JniNewGlobalRef(
      jniproxy::Thread()->NewObject()
      .Release(ExceptionAction::LOG_AND_IGNORE)
      .get());
  if (thread_ == nullptr) {
    LOG(ERROR) << "Failed to create new java.lang.Thread object";
    return false;
  }

  std::pair<std::string, std::function<void()>>* agent_arg =
      new std::pair<std::string, std::function<void()>>(
          std::make_pair(thread_name, thread_proc));

  // Run the code in the newly created thread.
  jvmtiError err = jvmti()->RunAgentThread(
      static_cast<jthread>(thread_.get()),
      [] (jvmtiEnv* jvmti, JNIEnv* jni, void* arg) {
        set_thread_jni(jni);

        DCHECK(!g_is_agent_thread);
        g_is_agent_thread = true;

        auto* agent_arg =
            reinterpret_cast<std::pair<std::string, std::function<bool()>>*>(
                arg);

        LOG(INFO) << "Agent thread started: " << agent_arg->first;

        agent_arg->second();

        LOG(INFO) << "Agent thread exited: " << agent_arg->first;

        g_is_agent_thread = false;

        delete agent_arg;
      },
      agent_arg,
      JVMTI_THREAD_NORM_PRIORITY);
  if (err != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "RunAgentThread failed, error: " << err;

    delete agent_arg;
    thread_ = nullptr;

    return false;
  }

  return true;
}

bool JvmtiAgentThread::IsInAgentThread() {
  return g_is_agent_thread;
}

}  // namespace cdbg
}  // namespace devtools

