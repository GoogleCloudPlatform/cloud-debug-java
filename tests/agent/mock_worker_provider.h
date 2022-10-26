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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_WORKER_PROVIDER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_WORKER_PROVIDER_H_

#include "gmock/gmock.h"
#include "src/agent/model_json.h"
#include "src/agent/worker.h"

namespace devtools {
namespace cdbg {

class MockWorkerProvider : public Worker::Provider {
 public:
  MockWorkerProvider() {
    ON_CALL(*this, OnWorkerReady(::testing::_))
        .WillByDefault(testing::Return(true));
  }

  MOCK_METHOD(bool, OnWorkerReady, (DebuggeeLabels*), (override));

  MOCK_METHOD(void, OnIdle, (), (override));

  MOCK_METHOD(void, OnBreakpointsUpdated,
              (std::vector<std::unique_ptr<BreakpointModel>>), (override));

  MOCK_METHOD(void, EnableDebugger, (bool), (override));
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_WORKER_PROVIDER_H_
