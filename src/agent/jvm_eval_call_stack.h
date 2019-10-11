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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_EVAL_CALL_STACK_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_EVAL_CALL_STACK_H_

#include <map>
#include <memory>
#include <vector>

#include "common.h"
#include "eval_call_stack.h"
#include "mutex.h"

namespace devtools {
namespace cdbg {

// Reads call stack using JVMTI methods.
class JvmEvalCallStack : public EvalCallStack {
 public:
  JvmEvalCallStack() { }
  ~JvmEvalCallStack() override { }

  void Read(jthread thread, std::vector<JvmFrame>* result) override;

  const FrameInfo& ResolveCallFrameKey(int key) const override;

  int InjectFrame(const FrameInfo& frame_info) override;

  void JvmtiOnCompiledMethodUnload(jmethodID method) override;

 private:
  // This structure may be released from CompiledMethodUnload. In this case
  // "JNIEnv*" is not going to be available. Therefore this structure must not
  // contain anything that requires "JNIEnv*" in destructor (e.g. "JVariant").
  struct MethodCache {
    // Signature of the parent class.
    std::string class_signature;

    // Generic signature of the parent class.
    std::string class_generic;

    // Method executing code at the call frame.
    std::string method_name;

    // Name of the source code file.
    std::string source_file_name;

    // Caches FrameInfo for a given location.
    std::map<jlocation, int> frames_cache;
  };

  // Loads information about the call stack frame into the frames cache and
  // returns call frame key.
  int DecodeFrame(const jvmtiFrameInfo& frame_info);

  // Loads method and class information into MethodCache.
  void LoadMethodCache(jmethodID method, MethodCache* method_cache);

  // Locates a line number corresponding to a method location. Returns -1
  // if line information is absent or the specified location doesn't match
  // the line information embedded in the Java .class file.
  int GetMethodLocationLineNumber(const jvmtiFrameInfo& frame_info);

 private:
  // Locks access to jmethodID pointers. JVM can unload a method any time.
  // When it does, it calls JvmtiOnCompiledMethodUnload function. After this
  // function returns jmethodID of that method points to a released memory.
  mutable absl::Mutex jmethods_mu_;

  // Locks access to the data structures used in this class.
  mutable absl::Mutex data_mu_;

  // List of resolved call frames. The call frame key is index in this array.
  std::vector<std::unique_ptr<FrameInfo>> frames_;

  // Cached information about methods we encountered so far.
  std::map<jmethodID, MethodCache> method_cache_;

  DISALLOW_COPY_AND_ASSIGN(JvmEvalCallStack);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_EVAL_CALL_STACK_H_
