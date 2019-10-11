/**
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_STRINGABLE_TYPE_EVALUATOR_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_STRINGABLE_TYPE_EVALUATOR_H_

#include "type_evaluator.h"

namespace devtools {
namespace cdbg {

class ClassMetadataReader;
struct NamedJVariant;

// Captures value of some objects through 'toString()'.
// This class doesn't verify that the object is safe for method calls.
class StringableTypeEvaluator : public TypeEvaluator {
 public:
  StringableTypeEvaluator();

  // Checks whether the specified class is supported by evaluator.
  // Although any class supports 'toString()', not any class is supported by
  // StringableTypeEvaluator. The reason is that for some objects calling
  // 'toString' might be too expensive (for example for some exception with
  // long call stack).
  bool IsSupported(jclass cls) const;

  std::string GetEvaluatorName() override { return "StringableTypeEvaluator"; }

  void Evaluate(MethodCaller* method_caller,
                const ClassMetadataReader::Entry& class_metadata,
                jobject obj,
                bool is_watch_expression,
                std::vector<NamedJVariant>* members) override;

 private:
  // Method metadata for the Java methods this pretty printer is using.
  const ClassMetadataReader::Method to_string_method_;

  DISALLOW_COPY_AND_ASSIGN(StringableTypeEvaluator);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_STRINGABLE_TYPE_EVALUATOR_H_
