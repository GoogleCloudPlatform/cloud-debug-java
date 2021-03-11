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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_DATA_VISIBILITY_POLICY_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_DATA_VISIBILITY_POLICY_H_

#include <cstdint>
#include <memory>

#include "common.h"

namespace devtools {
namespace cdbg {

// Data visibility policy for Java classes.
class DataVisibilityPolicy {
 public:
  // Consolidated visibility configuration of a single class. This does not
  // apply to inner or static classes.
  class Class {
   public:
    virtual ~Class() {}

    // Returns false if the field (or the entire class) is marked as invisible
    // for debugging.
    virtual bool IsFieldVisible(const std::string& name,
                                int32_t field_modifiers) = 0;

    // Returns false if the field data is invisible.
    //
    // If IsFieldVisible() == false, then the return value from this method can
    // not be trusted as we assume that it will never be called in that case.
    virtual bool IsFieldDataVisible(const std::string& name,
                                    int32_t field_modifiers,
                                    std::string* reason) = 0;

    // Returns false if calling the specified method must not be allowed, even
    // if the method is immutable (e.g. simple getter).
    virtual bool IsMethodVisible(const std::string& method_name,
                                 const std::string& method_signature,
                                 int32_t method_modifiers) = 0;

    // Returns false if the local variable or an argument is effectively
    // invisible for debugging.
    virtual bool IsVariableVisible(const std::string& method_name,
                                   const std::string& method_signature,
                                   const std::string& variable_name) = 0;

    // Returns false if the local variable or an argument data is invisible.
    //
    // If IsVariableVisible() == false, then the return value from this method
    // can not be trusted as we assume that it will never be called in that
    // case.
    virtual bool IsVariableDataVisible(const std::string& method_name,
                                       const std::string& method_signature,
                                       const std::string& variable_name,
                                       std::string* reason) = 0;
  };

  virtual ~DataVisibilityPolicy() {}

  // Gets visibility rules for the specified class. The returned configuration
  // only applies to a single class (and not to inner or static classes).
  //
  // Returns nullptr if the specified class doesn't have any visibility rules.
  // Everything is then visible by default. This is an optimization to avoid
  // allocating extra objects for the most common case.
  //
  // This function is not very fast. The caller should cache the result
  // and reuse it rather than calling this function repeatedly while capturing
  // breakpoint data.
  //
  // The instance of this class must outlive the returned object.
  virtual std::unique_ptr<Class> GetClassVisibility(jclass cls) = 0;

  // Returns true if there was a setup error.  If true, the provided error
  // parameter is set to the error message.  If false, the error parameter
  // is not modified.
  //
  // Note, that even if this method returns false, the object is expected to
  // provide a valid API.
  virtual bool HasSetupError(std::string* error) const = 0;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_DATA_VISIBILITY_POLICY_H_
