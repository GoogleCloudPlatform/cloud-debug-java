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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_STRING_EVALUATOR_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_STRING_EVALUATOR_H_

#include "common.h"
#include "expression_evaluator.h"

namespace devtools {
namespace cdbg {

// Evaluates string literal. This involves creation of a new Java string object
// in "Compile" and returning it in "Compute".
class StringEvaluator : public ExpressionEvaluator {
 public:
  explicit StringEvaluator(std::vector<jchar> string_content);

  ~StringEvaluator() override;

  bool Compile(
      ReadersFactory* readers_factory,
      FormatMessageModel* error_message) override;

  const JSignature& GetStaticType() const override;

  Nullable<jvalue> GetStaticValue() const override { return nullptr; }

  ErrorOr<JVariant> Evaluate(
      const EvaluationContext& evaluation_context) const override;

 private:
  // Set of Unicode characters definiting the Java string.
  const std::vector<jchar> string_content_;

  // Global reference to Java string object (product of "Compile").
  jstring jstr_;

  DISALLOW_COPY_AND_ASSIGN(StringEvaluator);
};


}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_STRING_EVALUATOR_H_


