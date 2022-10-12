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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_BREAKPOINT_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_BREAKPOINT_H_

#include "gmock/gmock.h"

#include "breakpoint.h"
#include "model_util.h"

namespace devtools {
namespace cdbg {

class MockBreakpoint : public Breakpoint {
 public:
  explicit MockBreakpoint(const std::string& id) : id_(id) {
    ON_CALL(*this, id())
        .WillByDefault(testing::ReturnRef(id_));
  }

  MOCK_METHOD(const std::string&, id, (), (const, override));

  MOCK_METHOD(void, Initialize, (), (override));

  MOCK_METHOD(void, ResetToPending, (), (override));

  MOCK_METHOD(void, OnClassPrepared, (const std::string&, const std::string&),
              (override));

  MOCK_METHOD(void, OnJvmBreakpointHit, (jthread, jmethodID, jlocation),
              (override));

  void CompleteBreakpointWithStatus(
      std::unique_ptr<StatusMessageModel> status) {
    std::ostringstream ss;
    ss << status;

    CompleteBreakpointWithStatus(ss.str());
  }

  MOCK_METHOD(void, CompleteBreakpointWithStatus, (const std::string&));

 private:
  const std::string id_;
};

}  // namespace cdbg
}  // namespace devtools


#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_BREAKPOINT_H_
