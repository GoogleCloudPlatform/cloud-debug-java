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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_EVAL_CALL_STACK_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_EVAL_CALL_STACK_H_

#include "common.h"

namespace devtools {
namespace cdbg {

// Reads stack trace from JVM. EvalCallStack caches names of methods and code
// location for efficiency reasons. The resolution of method name and location
// can't be deferred because (theoretically) a method can be unloaded by JVM at
// any moment.
// This class is thread safe.
class EvalCallStack {
 public:
  // Formatted version of a single call stack frame.
  struct FrameInfo {
    // Signature of the parent class.
    std::string class_signature;

    // Generic signature of the parent class.
    std::string class_generic;

    // Method executing code at the call frame.
    std::string method_name;

    // Name of the source code file or empty string if the Java class was
    // compiled without source debugging information.
    std::string source_file_name;

    // Line number of the statement in the call frame or -1 if the Java class
    // was compiled without line number debugging information.
    int line_number;
  };

  // Raw version of a single call stack frame. The purpose of the split between
  // "FrameInfo" and "JvmFrame" is to separate data collection from data
  // formatting. "JvmFrame" contains the necessary information to read local
  // variables and points to "FrameInfo" through "frame_info_key" member.
  // "JvmFrame" can only be used within the scope of JVMTI callback and should
  // be discarded immediately thereafter. The formatting of the protocol message
  // to the Hub service, on the other hand, is deferred to a worker thread that
  // only uses data in "FrameInfo" structure.
  struct JvmFrame {
    // Code location at the current stack frame. If not available (for example
    // for injected frames), "code_location.method" will be nullptr.
    jvmtiFrameInfo code_location;

    // Reference to "FrameInfo" structure. The data is obtained through
    // "ResolveCallFrameKey" method.
    int frame_info_key;
  };

 public:
  virtual ~EvalCallStack() { }

  // Reads call stack of a particular thread (typically that would be the
  // thread that hit a breakpoint). The function returns list of call frame
  // keys, which can be resolved to a more detailed information through
  // "ResolveCallFrameKey" function. The call frame key stays valid even if
  // the Java method has been unloaded.
  virtual void Read(jthread thread, std::vector<JvmFrame>* result) = 0;

  // Resolves call frame key returned in "Read" function. The data is stored
  // indefinitely and the call frame key will remain valid even if JVM unloads
  // the method or the class.
  virtual const FrameInfo& ResolveCallFrameKey(int key) const = 0;

  // Associates frame key with the specified frame. This method is used to
  // inject artificial stack frames.
  virtual int InjectFrame(const FrameInfo& frame_info) = 0;

  // Indicates that the specified Java method is no longer valid.
  virtual void JvmtiOnCompiledMethodUnload(jmethodID method) = 0;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_EVAL_CALL_STACK_H_
