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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_UNARY_EXPRESSION_EVALUATOR_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_UNARY_EXPRESSION_EVALUATOR_H_

#include "common.h"
#include "expression_evaluator.h"
#include "java_expression.h"

namespace devtools {
namespace cdbg {

// Implements all Java unary operators.
class UnaryExpressionEvaluator : public ExpressionEvaluator {
 public:
  // Class constructor. The instance will own "arg". "arg" is expected to be
  // uninitialized at this point.
  UnaryExpressionEvaluator(
      UnaryJavaExpression::Type type,
      std::unique_ptr<ExpressionEvaluator> arg);

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
  // Tries to compile the expression for unary plus and minus operators.
  // Returns false if the argument is not suitable.
  bool CompilePlusMinusOperators(FormatMessageModel* error_message);

  // Tries to compile the expression for bitwise complement operator (!).
  // Returns false if the argument is not suitable.
  bool CompileBitwiseComplement(FormatMessageModel* error_message);

  // Tries to compile the expression for logical complement operator (!).
  // Returns false if the argument is not suitable.
  bool CompileLogicalComplement(FormatMessageModel* error_message);

  // Computes the logical complement of a boolean argument (operator !).
  static ErrorOr<JVariant> LogicalComplementComputer(const JVariant& arg);

  // NOP computer used for unary plus operator (+) that does nothing beyond.
  // numeric promotion.
  static ErrorOr<JVariant> DoNothingComputer(const JVariant& arg);

  template <typename T>
  static ErrorOr<JVariant> MinusOperatorComputer(const JVariant& arg);

  template <typename T>
  static ErrorOr<JVariant> BitwiseComplementComputer(const JVariant& arg);

 private:
  // Binary expression type (e.g. +, -, ~, !).
  const UnaryJavaExpression::Type type_;

  // Compiled expression corresponding to the unary operator argument.
  std::unique_ptr<ExpressionEvaluator> arg_;

  // Pointer to a member function of this class to do the actual evaluation
  // of the binary expression.
  ErrorOr<JVariant> (*computer_)(const JVariant&);

  // Statically computed resulting type of the expression. This is what
  // computer_ is supposed product.
  JSignature result_type_;

  DISALLOW_COPY_AND_ASSIGN(UnaryExpressionEvaluator);
};


}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_UNARY_EXPRESSION_EVALUATOR_H_


