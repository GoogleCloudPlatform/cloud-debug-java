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

#include "unary_expression_evaluator.h"

#include "messages.h"
#include "model.h"
#include "numeric_cast_evaluator.h"

namespace devtools {
namespace cdbg {

UnaryExpressionEvaluator::UnaryExpressionEvaluator(
    UnaryJavaExpression::Type type,
    std::unique_ptr<ExpressionEvaluator> arg)
    : type_(type),
      arg_(std::move(arg)),
      computer_(nullptr) {
  result_type_.type = JType::Object;
}


bool UnaryExpressionEvaluator::Compile(
    ReadersFactory* readers_factory,
    FormatMessageModel* error_message) {
  if (!arg_->Compile(readers_factory, error_message)) {
    return false;
  }

  switch (type_) {
    case UnaryJavaExpression::Type::plus:
    case UnaryJavaExpression::Type::minus:
      return CompilePlusMinusOperators(error_message);

    case UnaryJavaExpression::Type::bitwise_complement:
      return CompileBitwiseComplement(error_message);

    case UnaryJavaExpression::Type::logical_complement:
      return CompileLogicalComplement(error_message);
  }

  return false;
}


bool UnaryExpressionEvaluator::CompilePlusMinusOperators(
    FormatMessageModel* error_message) {
  // TODO: unbox (Java Language Specification section 5.1.8).

  DCHECK((type_ == UnaryJavaExpression::Type::plus) ||
         (type_ == UnaryJavaExpression::Type::minus));
  const bool is_plus = (type_ == UnaryJavaExpression::Type::plus);

  switch (arg_->GetStaticType().type) {
    case JType::Byte:
    case JType::Char:
    case JType::Short:
    case JType::Int:
      if (!ApplyNumericCast<jint>(&arg_, error_message)) {
        return false;
      }

      computer_ = is_plus ? DoNothingComputer : MinusOperatorComputer<jint>;
      result_type_ = { JType::Int };

      return true;

    case JType::Long:
      computer_ = is_plus ? DoNothingComputer : MinusOperatorComputer<jlong>;
      result_type_ = { JType::Long };

      return true;

    case JType::Float:
      computer_ = is_plus ? DoNothingComputer : MinusOperatorComputer<jfloat>;
      result_type_ = { JType::Float };

      return true;

    case JType::Double:
      computer_ = is_plus ? DoNothingComputer : MinusOperatorComputer<jdouble>;
      result_type_ = { JType::Double };

      return true;

    default:
      // Unary plus and minus operators only apply to primitive numeric type.
      *error_message = { TypeMismatch };
      return false;
  }
}


bool UnaryExpressionEvaluator::CompileBitwiseComplement(
    FormatMessageModel* error_message) {
  // TODO: unbox (Java Language Specification section 5.1.8).

  switch (arg_->GetStaticType().type) {
    case JType::Byte:
    case JType::Char:
    case JType::Short:
    case JType::Int:
      if (!ApplyNumericCast<jint>(&arg_, error_message)) {
        return false;
      }

      computer_ = BitwiseComplementComputer<jint>;
      result_type_ = { JType::Int };

      return true;

    case JType::Long:
      computer_ = BitwiseComplementComputer<jlong>;
      result_type_ = { JType::Long };

      return true;

    default:
      // Bitwise complement only applies to primitive integral types.
      *error_message = { TypeMismatch };
      return false;
  }
}


bool UnaryExpressionEvaluator::CompileLogicalComplement(
    FormatMessageModel* error_message) {
  // TODO: unbox (Java Language Specification section 5.1.8).

  if (arg_->GetStaticType().type == JType::Boolean) {
    computer_ = LogicalComplementComputer;
    result_type_ = { JType::Boolean };

    return true;
  }

  *error_message = { TypeMismatch };
  return false;
}


ErrorOr<JVariant> UnaryExpressionEvaluator::Evaluate(
    const EvaluationContext& evaluation_context) const {
  ErrorOr<JVariant> arg_value = arg_->Evaluate(evaluation_context);
  if (arg_value.is_error()) {
    return arg_value;
  }

  return computer_(arg_value.value());
}


ErrorOr<JVariant> UnaryExpressionEvaluator::LogicalComplementComputer(
    const JVariant& arg) {
  jboolean boolean_value = false;
  if (!arg.get<jboolean>(&boolean_value)) {
    return INTERNAL_ERROR_MESSAGE;
  }

  return JVariant::Boolean(!boolean_value);
}


ErrorOr<JVariant> UnaryExpressionEvaluator::DoNothingComputer(
    const JVariant& arg) {
  return JVariant(arg);
}


template <typename T>
ErrorOr<JVariant> UnaryExpressionEvaluator::MinusOperatorComputer(
    const JVariant& arg) {
  T value = T();
  if (!arg.get<T>(&value)) {
    return INTERNAL_ERROR_MESSAGE;
  }

  return JVariant::Primitive<T>(-value);
}


template <typename T>
ErrorOr<JVariant> UnaryExpressionEvaluator::BitwiseComplementComputer(
    const JVariant& arg) {
  T value = T();
  if (!arg.get<T>(&value)) {
    return INTERNAL_ERROR_MESSAGE;
  }

  return JVariant::Primitive<T>(~value);
}


}  // namespace cdbg
}  // namespace devtools

