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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_OBJECT_EVALUATOR_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_OBJECT_EVALUATOR_H_

#include "common.h"

namespace devtools {
namespace cdbg {

class MethodCaller;
struct NamedJVariant;

// Owns all available "TypeEvaluator" objects (aka pretty printers) and routes
// the "Evaluate" call to the most appropriate one.
class ObjectEvaluator {
 public:
  virtual ~ObjectEvaluator() {}

  // Queries the object metadata and selects the most appropriate
  // "TypeEvaluator" implementation to evaluate the contents of an object.
  // "status" may be set to information or error message. Example of such a
  // message is: "only first 10 elements out of 1578 were captured". The
  // "method_caller" argument is only used during "Evaluate" and not stored
  // afterwards.
  virtual void Evaluate(
      MethodCaller* method_caller,
      jobject obj,
      bool is_watch_expression,
      std::vector<NamedJVariant>* members) = 0;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_OBJECT_EVALUATOR_H_

