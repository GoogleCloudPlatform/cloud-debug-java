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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_METHOD_CALLER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_METHOD_CALLER_H_

#include "class_metadata_reader.h"
#include "common.h"
#include "model_util.h"

namespace devtools {
namespace cdbg {

class JVariant;

// Invokes Java method (either static or instance).
// This interface is not thread safe. It should be instantiated for a single
// method call or for series of calls within the same expression.
class MethodCaller {
 public:
  virtual ~MethodCaller() { }

  // Invokes the method. When invoking static method, "source" is ignored.
  // For instance methods "source" is the object on which the instance method
  // is invoked.
  virtual ErrorOr<JVariant> Invoke(
      const ClassMetadataReader::Method& metadata,
      const JVariant& source,
      std::vector<JVariant> arguments) = 0;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_METHOD_CALLER_H_
