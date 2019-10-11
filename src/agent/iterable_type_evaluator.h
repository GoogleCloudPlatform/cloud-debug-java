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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_ITERABLE_TYPE_EVALUATOR_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_ITERABLE_TYPE_EVALUATOR_H_

#include <memory>

#include "common.h"
#include "type_evaluator.h"

namespace devtools {
namespace cdbg {

class ClassMetadataReader;
struct NamedJVariant;

// Captures elements of a Java class that implements Iterable interface.
// This class doesn't verify that the object is safe for method calls.
class IterableTypeEvaluator : public TypeEvaluator {
 public:
  IterableTypeEvaluator();

  // Checks whether the specified class implements "java.lang.Iterable"
  // interface.
  bool IsIterable(jclass cls) const;

  std::string GetEvaluatorName() override { return "IterableTypeEvaluator"; }

  void Evaluate(
      MethodCaller* method_caller,
      const ClassMetadataReader::Entry& class_metadata,
      jobject obj,
      bool is_watch_expression,
      std::vector<NamedJVariant>* members) override {
    Evaluate(method_caller, obj, is_watch_expression, members);
  }

  void Evaluate(
      MethodCaller* method_caller,
      jobject obj,
      bool is_watch_expression,
      std::vector<NamedJVariant>* members);

 private:
  // Method metadata for the Java methods this pretty printer is using.
  const ClassMetadataReader::Method iterable_iterator_;
  const ClassMetadataReader::Method iterator_has_next_;
  const ClassMetadataReader::Method iterator_next_;

  DISALLOW_COPY_AND_ASSIGN(IterableTypeEvaluator);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_ITERABLE_TYPE_EVALUATOR_H_

