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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_METHOD_LOCALS_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_METHOD_LOCALS_H_

#include <map>
#include <memory>
#include <vector>

#include "common.h"
#include "data_visibility_policy.h"
#include "mutex.h"

namespace devtools {
namespace cdbg {

class LocalVariableReader;

// Looks up for local variables in a given method and creates instances of
// "LocalVariableReader" corresponding to the defined local variables.
// This class is thread safe.
class MethodLocals {
 public:
  // This structure may be released from CompiledMethodUnload. In this case
  // "JNIEnv*" is not going to be available. Therefore this structure must not
  // contain anything that requires "JNIEnv*" in destructor (e.g. "JVariant").
  struct Entry {
    // List of local variables.
    std::vector<std::unique_ptr<LocalVariableReader>> locals;

    // Reader of "this" or null if the method is static.
    std::unique_ptr<LocalVariableReader> local_instance;
  };

  // "data_visibility_policy" is not owned by this class and must outlive it.
  explicit MethodLocals(DataVisibilityPolicy* data_visibility_policy);

  virtual ~MethodLocals() {}

  // Gets readers for all local variable available at a particular code
  // location. The function returns "shared_ptr" to ensure that the caller
  // can still access the vector even if the method got unloaded right after
  // "GetLocalVariables" returned. The caller is not expected to keep reference
  // to the returned array. The function is virtual for unit testing purposes
  // only.
  virtual std::shared_ptr<const Entry> GetLocalVariables(jmethodID method);

  // Indicates that the specified Java method is no longer valid.
  void JvmtiOnCompiledMethodUnload(jmethodID method);

 private:
  // Loads "Entry" information for the specified Java method.
  std::shared_ptr<Entry> LoadEntry(jmethodID method);

  // Load information about local instance (i.e. "this" pointer). Returns
  // nullptr for static methods.
  static std::unique_ptr<LocalVariableReader> LoadLocalInstance(
      jclass cls,  // Class in which the method is defined.
      jmethodID method);

 private:
  // Filters local variables.
  DataVisibilityPolicy* const data_visibility_policy_;

  // Locks access to local variables cache.
  absl::Mutex mu_;

  // Cache of local variables in methods we visited so far.
  std::map<jmethodID, std::shared_ptr<Entry>> method_vars_;

  DISALLOW_COPY_AND_ASSIGN(MethodLocals);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_METHOD_LOCALS_H_
