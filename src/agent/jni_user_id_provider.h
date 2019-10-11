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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JNI_USER_ID_PROVIDER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JNI_USER_ID_PROVIDER_H_

#include "common.h"
#include "jni_utils.h"
#include "user_id_provider.h"

namespace devtools {
namespace cdbg {

// Invokes com.google.devtools.cdbg.UserIdProvider to expose end user identity
// to the debugger agent.
class JniUserIdProvider : public UserIdProvider {
 public:
  // The "factory" callback creates a Java object implementing the
  // com.google.devtools.cdbg.UserIdProvider interface.
  explicit JniUserIdProvider(std::function<JniLocalRef()> factory);

  void Collect() override;

  bool Format(std::string* kind, std::string* id) override;

 private:
  // Callback that creates a Java object implementing the
  // com.google.devtools.cdbg.UserIdProvider interface.
  std::function<JniLocalRef()> factory_;

  // Global reference to Java class implementing the
  // com.google.devtools.cdbg.UserIdProvider interface.
  JniGlobalRef provider_;

  DISALLOW_COPY_AND_ASSIGN(JniUserIdProvider);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JNI_USER_ID_PROVIDER_H_
