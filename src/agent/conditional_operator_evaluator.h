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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_CONDITIONAL_OPERATOR_EVALUATOR_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_CONDITIONAL_OPERATOR_EVALUATOR_H_

#include "common.h"
#include "expression_evaluator.h"

namespace devtools {
namespace cdbg {

// Implements conditional Java operator (i.e. a ? b : c). The details of this
// operator are explained in Java Language Specification section 15.25.
class ConditionalOperatorEvaluator : public ExpressionEvaluator {
 public:
  // Class constructor. The instance will own "condition", "if_true" and
  // "if_false". These are expected to be uninitialized at this point.
  ConditionalOperatorEvaluator(
      std::unique_ptr<ExpressionEvaluator> condition,
      std::unique_ptr<ExpressionEvaluator> if_true,
      std::unique_ptr<ExpressionEvaluator> if_false);

  bool Compile(
      ReadersFactory* readers_factory,
      FormatMessageModel* error_message) override;

  const JSignature& GetStaticType() const override {
    return result_type_;
  }

  Nullable<jvalue> GetStaticValue() const override {
    return nullptr;
  }

  ErrorOr<JVariant> Evaluate(
      const EvaluationContext& evaluation_context) const override;

 private:
  // Compiles the conditional operator if both "if_true_" and "if_false_"
  // are boolean. Returns false if arguments are of other types.
  bool CompileBoolean();

  // Compiles the conditional operator if both "if_true_" and "if_false_"
  // are numeric. Potentially applies binary numeric promotion. Returns false
  // if arguments are of other types.
  bool CompileNumeric();

  // Compiles the conditional operator if both "if_true_" and "if_false_"
  // are references to objects. Potentially applies boxing and computes common
  // types ("lub" in Java Language Specification).
  bool CompileObjects();

  // Checks whether "if_true_" or "if_false_" are of the specified type.
  bool IsEitherType(JType type) const;

  // Applies binary numeric promotion of type "TTargetType" to both "if_true_"
  // and "if_false_". Returns false if either numeric promotion is not viable
  // (one of the arguments is boolean or object).
  template <typename TTargetType>
  bool ApplyNumericPromotions(FormatMessageModel* error_message);

 private:
  // Compiled expression corresponding to the condition.
  std::unique_ptr<ExpressionEvaluator> condition_;

  // Expression applied if "condition_" evaluates to true.
  std::unique_ptr<ExpressionEvaluator> if_true_;

  // Expression applied if "condition_" evaluates to false.
  std::unique_ptr<ExpressionEvaluator> if_false_;

  // Statically computed resulting type of the expression. This is what
  // computer_ is supposed product.
  JSignature result_type_;

  DISALLOW_COPY_AND_ASSIGN(ConditionalOperatorEvaluator);
};


}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_CONDITIONAL_OPERATOR_EVALUATOR_H_


