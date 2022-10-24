/**
 * Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_DATA_VISIBILITY_POLICY_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_DATA_VISIBILITY_POLICY_H_

#include <cstdint>

#include "gmock/gmock.h"
#include "src/agent/data_visibility_policy.h"

namespace devtools {
namespace cdbg {

class MockDataVisibilityPolicy : public DataVisibilityPolicy {
 public:
  MOCK_METHOD(std::unique_ptr<Class>, GetClassVisibility, (jclass), (override));
  MOCK_METHOD(bool, HasSetupError, (std::string*), (const, override));

  class Class : public DataVisibilityPolicy::Class {
   public:
    MOCK_METHOD(bool, IsFieldVisible, (const std::string&, int32_t),
                (override));
    MOCK_METHOD(bool, IsFieldDataVisible,
                (const std::string&, int32_t, std::string*), (override));
    MOCK_METHOD(bool, IsMethodVisible,
                (const std::string&, const std::string&, int32_t), (override));
    MOCK_METHOD(bool, IsVariableVisible,
                (const std::string&, const std::string&, const std::string&),
                (override));
    MOCK_METHOD(bool, IsVariableDataVisible,
                (const std::string&, const std::string&, const std::string&,
                 std::string*),
                (override));
  };
};


}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_DATA_VISIBILITY_POLICY_H_
