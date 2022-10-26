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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_CLASS_PATH_LOOKUP_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_CLASS_PATH_LOOKUP_H_

#include "gmock/gmock.h"
#include "src/agent/class_path_lookup.h"

namespace devtools {
namespace cdbg {

class MockClassPathLookup : public ClassPathLookup {
 public:
  MOCK_METHOD(void, ResolveSourceLocation,
              (const std::string&, int, ResolvedSourceLocation*), (override));

  MOCK_METHOD(std::vector<std::string>, FindClassesByName, (const std::string&),
              (override));

  MOCK_METHOD(std::string, ComputeDebuggeeUniquifier, (const std::string&),
              (override));

  MOCK_METHOD(std::set<std::string>, ReadApplicationResource,
              (const std::string&), (override));

  MOCK_METHOD(bool, IsMethodCallAllowed,
              (const std::string&, const std::string&, const std::string&,
               const bool));

  MOCK_METHOD(bool, IsSafeIterable, (const std::string&));

  MOCK_METHOD(bool, TransformMethod,
              (jobject, const JavaClass&, const std::string&,
               const std::string&, JavaClass*));
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_CLASS_PATH_LOOKUP_H_
