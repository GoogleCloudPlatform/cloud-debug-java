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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_LITERAL_EVALUATOR_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_LITERAL_EVALUATOR_H_

#include "common.h"
#include "expression_evaluator.h"

namespace devtools {
namespace cdbg {

// Represents a constant of any type (other than a string).
class LiteralEvaluator : public ExpressionEvaluator {
 public:
  explicit LiteralEvaluator(JVariant* n)
      : result_type_({ n->type() }) {
    n_.swap(n);
  }

  bool Compile(
      ReadersFactory* readers_factory,
      FormatMessageModel* error_message) override {
    return true;
  }

  const JSignature& GetStaticType() const override { return result_type_; }

  Nullable<jvalue> GetStaticValue() const override { return n_.get_jvalue(); }

  ErrorOr<JVariant> Evaluate(
      const EvaluationContext& evaluation_context) const override {
    return JVariant(n_);
  }

 private:
  // Literal value associated with this leaf.
  JVariant n_;

  // Statically computed resulting type of the expression.
  const JSignature result_type_;

  DISALLOW_COPY_AND_ASSIGN(LiteralEvaluator);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_LITERAL_EVALUATOR_H_


