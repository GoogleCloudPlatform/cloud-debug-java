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

#include "conditional_operator_evaluator.h"

#include "messages.h"
#include "model.h"
#include "numeric_cast_evaluator.h"

namespace devtools {
namespace cdbg {

ConditionalOperatorEvaluator::ConditionalOperatorEvaluator(
    std::unique_ptr<ExpressionEvaluator> condition,
    std::unique_ptr<ExpressionEvaluator> if_true,
    std::unique_ptr<ExpressionEvaluator> if_false)
    : condition_(std::move(condition)),
      if_true_(std::move(if_true)),
      if_false_(std::move(if_false)) {
  result_type_.type = JType::Object;
}


bool ConditionalOperatorEvaluator::Compile(
    ReadersFactory* readers_factory,
    FormatMessageModel* error_message) {
  if (!condition_->Compile(readers_factory, error_message) ||
      !if_true_->Compile(readers_factory, error_message) ||
      !if_false_->Compile(readers_factory, error_message)) {
    return false;
  }

  // TODO: unbox "condition_" (Java Language Specification section 5.1.8).

  // All conditional operators must have "condition_" of a boolean type.
  if (condition_->GetStaticType().type != JType::Boolean) {
    *error_message = { TypeMismatch };
    return false;
  }

  // Case 1: both "if_true_" and "if_false_" are of a boolean type.
  if (CompileBoolean()) {
    return true;
  }

  // Case 2: both "if_true_" and "if_false_" are numeric.
  if (CompileNumeric()) {
    return true;
  }

  // Case 3: both "if_true_" and "if_false_" are objects.
  if (CompileObjects()) {
    return true;
  }

  *error_message = { TypeMismatch };

  return false;
}


bool ConditionalOperatorEvaluator::CompileBoolean() {
  // TODO: unbox "if_true_" and "if_false_" from Boolean to boolean.

  if ((if_true_->GetStaticType().type == JType::Boolean) &&
      (if_false_->GetStaticType().type == JType::Boolean)) {
    result_type_ = { JType::Boolean };
    return true;
  }

  return false;
}


bool ConditionalOperatorEvaluator::CompileNumeric() {
  FormatMessageModel unused_error_message;

  // TODO: unbox "if_true_" and "if_false_".

  // TODO: implement this clause after we have support for byte/short:
  //    If one of the operands is of type byte or Byte and the other is of
  //    type short or Short, then the type of the conditional expression is
  //    short.

  // TODO: add support for constants and implement this clause:
  //    If one of the operands is of type T where T is byte, short, or char,
  //    and the other operand is a constant expression (15.28) of type int
  //    whose value is representable in type T, then the type of the
  //    conditional expression is T.

  // Default case of numeric conditional expression (applying binary numeric
  // promotion).
  if (IsEitherType(JType::Double)) {
    if (!ApplyNumericPromotions<jdouble>(&unused_error_message)) {
      return false;
    }

    result_type_ = { JType::Double };
    return true;

  } else if (IsEitherType(JType::Float)) {
    if (!ApplyNumericPromotions<jfloat>(&unused_error_message)) {
      return false;
    }

    result_type_ = { JType::Float };
    return true;

  } else if (IsEitherType(JType::Long)) {
    if (!ApplyNumericPromotions<jlong>(&unused_error_message)) {
      return false;
    }

    result_type_ = { JType::Long };
    return true;

  } else {
    if (!ApplyNumericPromotions<jint>(&unused_error_message)) {
      return false;
    }

    result_type_ = { JType::Int };
    return true;
  }
}


bool ConditionalOperatorEvaluator::CompileObjects() {
  // TODO: this is a super-simplistic implementation that doesn't cover
  // a lot of cases. For more details please refer to Java Language
  // Specification section 15.25.3 and tables in section 15.25.

  const JSignature& true_signature = if_true_->GetStaticType();
  const JSignature& false_signature = if_false_->GetStaticType();
  if ((true_signature.type == JType::Object) &&
      (false_signature.type == JType::Object)) {
    if (true_signature.object_signature == false_signature.object_signature) {
      result_type_ = true_signature;
    } else {
      result_type_ = { JType::Object };  // Note the lost signature.
    }
    return true;
  }

  return false;
}


bool ConditionalOperatorEvaluator::IsEitherType(JType type) const {
  return (if_true_->GetStaticType().type == type) ||
         (if_false_->GetStaticType().type == type);
}


template <typename TTargetType>
bool ConditionalOperatorEvaluator::ApplyNumericPromotions(
    FormatMessageModel* error_message) {
  return ApplyNumericCast<TTargetType>(&if_true_, error_message) &&
         ApplyNumericCast<TTargetType>(&if_false_, error_message);
}


ErrorOr<JVariant> ConditionalOperatorEvaluator::Evaluate(
    const EvaluationContext& evaluation_context) const {
  ErrorOr<JVariant> evaluated_condition =
      condition_->Evaluate(evaluation_context);
  if (evaluated_condition.is_error()) {
    return evaluated_condition;
  }

  jboolean condition_value = false;
  if (!evaluated_condition.value().get<jboolean>(&condition_value)) {
    return INTERNAL_ERROR_MESSAGE;
  }

  const ExpressionEvaluator& target_expression =
      (condition_value ? *if_true_ : *if_false_);

  return target_expression.Evaluate(evaluation_context);
}

}  // namespace cdbg
}  // namespace devtools

