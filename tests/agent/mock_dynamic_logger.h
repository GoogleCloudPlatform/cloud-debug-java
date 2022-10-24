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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_DYNAMIC_LOGGER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_DYNAMIC_LOGGER_H_

#include "gmock/gmock.h"
#include "src/agent/dynamic_logger.h"
#include "src/agent/resolved_source_location.h"

namespace devtools {
namespace cdbg {

class MockDynamicLogger : public DynamicLogger {
 public:
  MockDynamicLogger() {
    ON_CALL(*this, IsAvailable())
        .WillByDefault(testing::Return(true));
  }

  MOCK_METHOD(bool, IsAvailable, (), (const, override));

  MOCK_METHOD(void, Log,
              (BreakpointModel::LogLevel, const ResolvedSourceLocation&,
               const std::string&),
              (override));
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_DYNAMIC_LOGGER_H_
