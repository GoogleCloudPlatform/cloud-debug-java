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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_BINARY_EXPRESSION_EVALUATOR_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_BINARY_EXPRESSION_EVALUATOR_H_

#include <cstdint>

#include "common.h"
#include "expression_evaluator.h"
#include "java_expression.h"

namespace devtools {
namespace cdbg {

// Implements all Java binary operators.
class BinaryExpressionEvaluator : public ExpressionEvaluator {
 public:
  // Class constructor. The instance will own "arg1" and "arg2". These are
  // expected to be uninitialized at this point.
  BinaryExpressionEvaluator(
      BinaryJavaExpression::Type type,
      std::unique_ptr<ExpressionEvaluator> arg1,
      std::unique_ptr<ExpressionEvaluator> arg2);

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
  // Implements "Compile" for arithmetical operators (+, -, *, /, %).
  bool CompileArithmetical(FormatMessageModel* error_message);

  // Implements "Compile" for conditional operators (e.g. &&, ==, <=).
  bool CompileConditional(FormatMessageModel* error_message);

  // Implements "Compile" for boolean conditional operators
  // (e.g. &, |, &&, ==, <=).
  bool CompileBooleanConditional(FormatMessageModel* error_message);

  // Implements "Compile" for bitwise operators (&, |, ^).
  bool CompileBitwise(FormatMessageModel* error_message);

  // Implements "Compile" for shoft operators (<<, >>, >>>).
  bool CompileShift(FormatMessageModel* error_message);

  // Checks whether "arg1" or "arg2" are of the specified type.
  bool IsEitherType(JType type) const;

  // Applies numeric promotion of type "TTargetType" to both "arg1" and "arg2".
  // Returns false if either numeric promotion is not viable (one of the
  // arguments is boolean or object).
  template <typename TTargetType>
  bool ApplyNumericPromotions(FormatMessageModel* error_message);

  // Applies numeric promotion of either "arg1" or "arg2" as per Java Language
  // Specifications section 5.6.1.
  static bool ApplyShiftNumericPromotion(
      std::unique_ptr<ExpressionEvaluator>* arg,
      FormatMessageModel* error_message);

  // Computes the value of the expression for arithmetical operators. The
  // template type "T" is the type that both arguments were promoted into.
  // See Java Language Specification section 5.6.2 for more details.
  template <typename T>
  ErrorOr<JVariant> ArithmeticComputer(
      const JVariant& arg1,
      const JVariant& arg2) const;

  // Computes the value of the expression for bitwise operators. This does not
  // include bitwise operators applied on booleans (which become conditional
  // operators). The template type "T" can be either jint or jlong as per
  // Java Language Specification section 15.22).
  template <typename T>
  ErrorOr<JVariant> BitwiseComputer(
      const JVariant& arg1,
      const JVariant& arg2) const;

  // Computes the value of shift expression. The template type "T" denotes the
  // type of the first argument (the shifted number). As per Java Language
  // Specification section 15.19), "T" can only be jint or jlong. The type of
  // the second argument is either int or long. "Bitmask" is applied to the
  // second argument as per specifications (also section 15.19).
  template <typename T, typename TUnsigned, uint16_t Bitmask>
  ErrorOr<JVariant> ShiftComputer(const JVariant& arg1,
                                  const JVariant& arg2) const;

  // Implements comparison operator on Java objects. JNI method IsSameObject
  // is used to actually compare the two objects.
  ErrorOr<JVariant> ConditionalObjectComputer(
      const JVariant& arg1,
      const JVariant& arg2) const;

  // Compares two Java strings (including inline string literals).
  ErrorOr<JVariant> ConditionalStringComputer(
      const JVariant& arg1,
      const JVariant& arg2) const;

  // Implements conditional operators. As per Java Language
  // Specifications sections 15.23 and 15.24 logical operators && and || only
  // apply to boolean type. Comparison operators == and != can also apply to
  // boolean.
  ErrorOr<JVariant> ConditionalBooleanComputer(
      const JVariant& arg1,
      const JVariant& arg2) const;

  // Implements comparison operators for numerical types (i.e. not booleans).
  // As per Java Language Specifications section 15.20 the two arguments are
  // promoted to the same type and compared against each other.
  template <typename T>
  ErrorOr<JVariant> NumericalComparisonComputer(
      const JVariant& arg1,
      const JVariant& arg2) const;

 private:
  // Binary expression type (e.g. + or <<).
  const BinaryJavaExpression::Type type_;

  // Compiled expression corresponding to the first operand.
  std::unique_ptr<ExpressionEvaluator> arg1_;

  // Compiled expression corresponding to the second operand.
  std::unique_ptr<ExpressionEvaluator> arg2_;

  // Pointer to a member function of this class to do the actual evaluation
  // of the binary expression.
  ErrorOr<JVariant> (BinaryExpressionEvaluator::*computer_)(
      const JVariant&,
      const JVariant&) const;

  // Statically computed resulting type of the expression. This is what
  // computer_ is supposed product.
  JSignature result_type_;

  DISALLOW_COPY_AND_ASSIGN(BinaryExpressionEvaluator);
};


}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_BINARY_EXPRESSION_EVALUATOR_H_


