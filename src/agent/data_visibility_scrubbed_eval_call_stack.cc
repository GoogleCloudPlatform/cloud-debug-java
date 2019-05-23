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

#include "data_visibility_scrubbed_eval_call_stack.h"

#include "jvmti_buffer.h"

namespace devtools {
namespace cdbg {

// Returns true if the provided policy object determines that the provided
// method is visible.
static bool isMethodVisible(
    DataVisibilityPolicy* policy,
    const jmethodID method) {
  if (method == nullptr) {
    // This method was already scrubbed by the inner nested scrubber. Treat it
    // as visible at the current level to prevent a negative interaction between
    // nested scrubbers (returning 'false' will cause the rest of the frames to
    // be blocked).
    return true;
  }

  // Read class information.
  jclass cls = nullptr;
  jvmtiError err = jvmti()->GetMethodDeclaringClass(method, &cls);
  if (err !=  JVMTI_ERROR_NONE) {
    LOG(ERROR) << "DataVisibilityScrubbedEvalCallStack failed to get "
               << "declaring class.";
    return false;  // Have to assume sensitive
  }

  // Get visibility policy for this class.

  std::unique_ptr<DataVisibilityPolicy::Class> policy_cls =
      policy->GetClassVisibility(cls);
  if (policy_cls == nullptr) {
    // The entire class is considered safe.
    return true;
  }

  // Get Method information which is needed to check visibility.

  JvmtiBuffer<char> method_name;
  JvmtiBuffer<char> method_signature;
  err = jvmti()->GetMethodName(
      method,
      method_name.ref(),
      method_signature.ref(),
      nullptr);
  if (err != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "GetMethodName failed, error: " << err;
    return false;  // Have to assume sensitive
  }

  jint method_modifiers = 0;
  err = jvmti()->GetMethodModifiers(method, &method_modifiers);
  if (err != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "GetMethodModifiers failed, error: " << err;
    return false;  // Have to assume sensitive
  }

  // Check method visibility.
  //
  // Note: Calling isMethodVisible() here is debatable. It only works when
  // isMethodVisible() results should cascade to children in the stack. This is
  // currently determined via context - we only initialize
  // DataVisibilityScrubbedEvalCallStack with visibility objects that meet this
  // requirement.
  //
  // An alternate would be to create a DataVisiblityObject API method like,
  // doesMethodHideStack().  This has the downside of making the visibility
  // interface less abstract as it would be embedding specific caller metadata.
  return policy_cls->IsMethodVisible(
      method_name.get(),
      method_signature.get(),
      method_modifiers);
}

DataVisibilityScrubbedEvalCallStack::DataVisibilityScrubbedEvalCallStack(
    std::unique_ptr<EvalCallStack> unscrubbed_eval_call_stack,
    DataVisibilityPolicy* policy)
    : unscrubbed_eval_call_stack_(std::move(unscrubbed_eval_call_stack)),
      policy_(policy) {
}


void DataVisibilityScrubbedEvalCallStack::Read(
    jthread thread,
    std::vector<JvmFrame>* result) {
  unscrubbed_eval_call_stack_->Read(thread, result);
  const int call_frames_count = result->size();

  // Search for the highest invisible frame in the stack
  int depth = call_frames_count - 1;
  for (; depth >= 0; --depth) {
    if (!isMethodVisible(policy_, result->at(depth).code_location.method)) {
      // decrement so that the next loop does not recheck this frame (slight
      // performance increase).
      --depth;
      break;
    }
  }

  // depth is now at the first child frame under invisible data.  All visible
  // frames should be scrubbed for the rest of the stack.
  for (; depth >= 0; --depth) {
    if (isMethodVisible(policy_, result->at(depth).code_location.method)) {
      result->at(depth).code_location.method = nullptr;
    }
  }
}


const EvalCallStack::FrameInfo&
    DataVisibilityScrubbedEvalCallStack::ResolveCallFrameKey(
        int key) const {
  return unscrubbed_eval_call_stack_->ResolveCallFrameKey(key);
}


int DataVisibilityScrubbedEvalCallStack::InjectFrame(
    const FrameInfo& frame_info) {
  return unscrubbed_eval_call_stack_->InjectFrame(frame_info);
}


void DataVisibilityScrubbedEvalCallStack::JvmtiOnCompiledMethodUnload(
    jmethodID method) {
  unscrubbed_eval_call_stack_->JvmtiOnCompiledMethodUnload(method);
}

}  // namespace cdbg
}  // namespace devtools

