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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_DATA_VISIBILITY_SCRUBBED_EVAL_CALL_STACK_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_DATA_VISIBILITY_SCRUBBED_EVAL_CALL_STACK_H_

#include "data_visibility_policy.h"
#include "eval_call_stack.h"

namespace devtools {
namespace cdbg {

// Removes the local variables of child frames below an identified "sensitive
// frame" (as determined by the provided DataVisibilityPolicy).  It's possible
// for a sensitive method to pass sensitive data to child frames for processing
// (e.g. string splitting, sorting).  Without scrubbing, this data could be
// viewed by a user of the debugger.
class DataVisibilityScrubbedEvalCallStack : public EvalCallStack {
 public:
  // policy is not owned by this class
  explicit DataVisibilityScrubbedEvalCallStack(
      std::unique_ptr<EvalCallStack> unscrubbed_eval_call_stack,
      DataVisibilityPolicy* policy);

  ~DataVisibilityScrubbedEvalCallStack() override { }

  // For a scrubbed stack frame, the corresponding entry in *result will have
  // its method field set to null.
  void Read(jthread thread, std::vector<JvmFrame>* result) override;

  const FrameInfo& ResolveCallFrameKey(int key) const override;

  int InjectFrame(const FrameInfo& frame_info) override;

  void JvmtiOnCompiledMethodUnload(jmethodID method) override;

 private:
  // Downstream instance of "EvalCallStack" that bring unscrubbed call stack.
  std::unique_ptr<EvalCallStack> unscrubbed_eval_call_stack_;

  // DataVisibilityPolicy to use.  Not owned by this class.
  DataVisibilityPolicy* policy_;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_DATA_VISIBILITY_SCRUBBED_EVAL_CALL_STACK_H_
