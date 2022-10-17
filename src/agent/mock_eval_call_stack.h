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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_EVAL_CALL_STACK_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_EVAL_CALL_STACK_H_

#include "eval_call_stack.h"
#include "gmock/gmock.h"

namespace devtools {
namespace cdbg {

class MockEvalCallStack : public EvalCallStack {
 public:
  MockEvalCallStack() {
    ON_CALL(*this, InjectFrame(testing::_))
        .WillByDefault(testing::Invoke([this] (const FrameInfo& frame_info) {
          frames_.push_back(
              std::unique_ptr<FrameInfo>(new FrameInfo(frame_info)));
          return frames_.size() - 1;
        }));

    ON_CALL(*this, ResolveCallFrameKey(testing::_))
        .WillByDefault(testing::Invoke(
            this,
            &MockEvalCallStack::DefaultResolveCallFrameKey));
  }

  MOCK_METHOD(void, Read, (jthread, std::vector<JvmFrame>*), (override));

  MOCK_METHOD(const FrameInfo&, ResolveCallFrameKey, (int), (const, override));

  MOCK_METHOD(int, InjectFrame, (const FrameInfo&), (override));

  MOCK_METHOD(void, JvmtiOnCompiledMethodUnload, (jmethodID), (override));

 private:
  const FrameInfo& DefaultResolveCallFrameKey(int frame_key) {
    return *frames_[frame_key];
  }

 private:
  std::vector<std::unique_ptr<FrameInfo>> frames_;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_EVAL_CALL_STACK_H_
