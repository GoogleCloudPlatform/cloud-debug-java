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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_MULTI_DATA_VISIBILITY_POLICY_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_MULTI_DATA_VISIBILITY_POLICY_H_

#include <vector>

#include "data_visibility_policy.h"

namespace devtools {
namespace cdbg {

// Data visibility policy that can act as a frontend for two or three
// backend policies.
//
// This simplifies the logic of client code, which can act as if there
// is only a single policy.
class MultiDataVisibilityPolicy : public DataVisibilityPolicy {
 public:
  // Build a policy with two child policies.  Takes ownership of arguments.
  MultiDataVisibilityPolicy(
      std::unique_ptr<DataVisibilityPolicy> policy1,
      std::unique_ptr<DataVisibilityPolicy> policy2);

  // Build a policy with three child policies.  Takes ownership of arguments.
  MultiDataVisibilityPolicy(
      std::unique_ptr<DataVisibilityPolicy> policy1,
      std::unique_ptr<DataVisibilityPolicy> policy2,
      std::unique_ptr<DataVisibilityPolicy> policy3);

  std::unique_ptr<Class> GetClassVisibility(jclass cls) override;

  // Returns the error of the first found policy error, or false
  // if no policies have an error.
  bool HasSetupError(std::string* error) const override;

 private:
  std::vector<std::unique_ptr<DataVisibilityPolicy>> policy_list_;

  DISALLOW_COPY_AND_ASSIGN(MultiDataVisibilityPolicy);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_MULTI_DATA_VISIBILITY_POLICY_H_
