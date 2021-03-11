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

#include "binary_expression_evaluator.h"

#include <cmath>
#include <cstdint>
#include <limits>

#include "messages.h"
#include "model.h"
#include "numeric_cast_evaluator.h"

namespace devtools {
namespace cdbg {

// Implementation of Java modulo (%) operator for int data type.
static jint ComputeModulo(jint x, jint y) {
  return x % y;
}


// Implementation of Java modulo (%) operator for long data type.
static jlong ComputeModulo(jlong x, jlong y) {
  return x % y;
}


// Implementation of Java modulo (%) operator for float data type.
static jfloat ComputeModulo(jfloat x, jfloat y) {
  return std::fmod(static_cast<float>(x), static_cast<float>(y));
}


// Implementation of Java modulo (%) operator for double data type.
static jdouble ComputeModulo(jdouble x, jdouble y) {
  return std::fmod(static_cast<double>(x), static_cast<double>(y));
}


// Checks that the divisor will not trigger "division by zero" signal.
static bool IsDivisionByZero(jint divisor) {
  return divisor == 0;
}


// Checks that the divisor will not trigger "division by zero" signal.
static bool IsDivisionByZero(jlong divisor) {
  return divisor == 0;
}


// Checks that the divisor will not trigger "division by zero" signal.
static bool IsDivisionByZero(jfloat divisor) {
  return false;  // Floating point division never triggers the signal
}


// Checks that the divisor will not trigger "division by zero" signal.
static bool IsDivisionByZero(jdouble divisor) {
  return false;  // Floating point division never triggers the signal
}


// Detects edge case in integer division that causes SIGFPE signal.
static bool IsDivisionOverflow(jint value1, jint value2) {
  return (value1 == std::numeric_limits<jint>::min()) &&
         (value2 == -1);
}


// Detects edge case in integer division that causes SIGFPE signal.
static bool IsDivisionOverflow(jlong value1, jlong value2) {
  return (value1 == std::numeric_limits<jlong>::min()) &&
         (value2 == -1);
}


// Detects edge case in integer division that causes SIGFPE signal.
static bool IsDivisionOverflow(jfloat value1, jfloat value2) {
  return false;  // This condition does not apply to floating point.
}


// Detects edge case in integer division that causes SIGFPE signal.
static bool IsDivisionOverflow(jdouble value1, jdouble value2) {
  return false;  // This condition does not apply to floating point.
}


// Compares two Java strings.
static bool IsEqual(jstring string1, jstring string2) {
  if ((string1 == nullptr) || (string2 == nullptr)) {
    return string1 == string2;
  }

  const jint length1 = jni()->GetStringLength(string1);
  const jint length2 = jni()->GetStringLength(string2);

  if (length1 != length2) {
    return false;
  }

  if (length1 == 0) {
    return true;
  }

  const jchar* data1 = jni()->GetStringCritical(string1, nullptr);
  if (data1 == nullptr) {   // Some error occurred.
    return false;
  }

  const jchar* data2 = jni()->GetStringCritical(string2, nullptr);
  if (data2 == nullptr) {   // Some error occurred.
    jni()->ReleaseStringCritical(string1, data1);
    return false;
  }

  const bool is_equal = (memcmp(data1, data2, sizeof(jchar) * length1) == 0);

  jni()->ReleaseStringCritical(string1, data1);
  jni()->ReleaseStringCritical(string2, data2);

  return is_equal;
}


BinaryExpressionEvaluator::BinaryExpressionEvaluator(
    BinaryJavaExpression::Type type,
    std::unique_ptr<ExpressionEvaluator> arg1,
    std::unique_ptr<ExpressionEvaluator> arg2)
    : type_(type),
      arg1_(std::move(arg1)),
      arg2_(std::move(arg2)),
      computer_(nullptr) {
  result_type_.type = JType::Object;
}


bool BinaryExpressionEvaluator::Compile(
    ReadersFactory* readers_factory,
    FormatMessageModel* error_message) {
  if (!arg1_->Compile(readers_factory, error_message)) {
    return false;
  }

  if (!arg2_->Compile(readers_factory, error_message)) {
    return false;
  }

  switch (type_) {
    case BinaryJavaExpression::Type::add:
    case BinaryJavaExpression::Type::sub:
    case BinaryJavaExpression::Type::mul:
    case BinaryJavaExpression::Type::div:
    case BinaryJavaExpression::Type::mod:
      return CompileArithmetical(error_message);

    case BinaryJavaExpression::Type::conditional_and:
    case BinaryJavaExpression::Type::conditional_or:
    case BinaryJavaExpression::Type::eq:
    case BinaryJavaExpression::Type::ne:
    case BinaryJavaExpression::Type::le:
    case BinaryJavaExpression::Type::ge:
    case BinaryJavaExpression::Type::lt:
    case BinaryJavaExpression::Type::gt:
      return CompileConditional(error_message);

    case BinaryJavaExpression::Type::bitwise_and:
    case BinaryJavaExpression::Type::bitwise_or:
    case BinaryJavaExpression::Type::bitwise_xor:
      return CompileBitwise(error_message);

    case BinaryJavaExpression::Type::shl:
    case BinaryJavaExpression::Type::shr_s:
    case BinaryJavaExpression::Type::shr_u:
      return CompileShift(error_message);
  }

  // Compiler should catch any missing enums. We should never get here.
  return false;
}

bool BinaryExpressionEvaluator::CompileArithmetical(
    FormatMessageModel* error_message) {
  // TODO: unbox (Java Language Specification section 5.1.8).

  // TODO: implement concatenation for strings

  // Apply numeric promotions (Java Language Specification section 5.6.2)
  // and initialize the computation routine.
  if (IsEitherType(JType::Double)) {
    if (!ApplyNumericPromotions<jdouble>(error_message)) {
      return false;
    }

    computer_ = &BinaryExpressionEvaluator::ArithmeticComputer<jdouble>;
    result_type_ = { JType::Double };

    return true;
  } else if (IsEitherType(JType::Float)) {
    if (!ApplyNumericPromotions<jfloat>(error_message)) {
      return false;
    }

    computer_ = &BinaryExpressionEvaluator::ArithmeticComputer<jfloat>;
    result_type_ = { JType::Float };

    return true;
  } else if (IsEitherType(JType::Long)) {
    if (!ApplyNumericPromotions<jlong>(error_message)) {
      return false;
    }

    computer_ = &BinaryExpressionEvaluator::ArithmeticComputer<jlong>;
    result_type_ = { JType::Long };

    return true;
  } else {
    if (!ApplyNumericPromotions<jint>(error_message)) {
      return false;
    }

    computer_ = &BinaryExpressionEvaluator::ArithmeticComputer<jint>;
    result_type_ = { JType::Int };

    return true;
  }
}


bool BinaryExpressionEvaluator::CompileConditional(
    FormatMessageModel* error_message) {
  const JSignature& signature1 = arg1_->GetStaticType();
  const JSignature& signature2 = arg2_->GetStaticType();

  // Conditional operations applied to objects.
  if ((signature1.type == JType::Object) &&
      (signature2.type == JType::Object) &&
      ((type_ == BinaryJavaExpression::Type::eq) ||
       (type_ == BinaryJavaExpression::Type::ne))) {
    // Use regular comparison operators ("==" and "!=") to compare Java
    // strings. This is not consistent with Java language. The way to compare
    // strings in Java is through "equals" method, but expression evaluator
    // doesn't support methods yet. Also it wouldn't make sense if breakpoint
    // condition like (myName == "vlad") would always evaluate to false.
    // TODO: support string comparison through "equals" method and keep
    // this kind of string comparison for inline strings only.
    if ((signature1.object_signature == kJavaStringClassSignature) &&
        (signature2.object_signature == kJavaStringClassSignature)) {
      computer_ = &BinaryExpressionEvaluator::ConditionalStringComputer;
    } else {
      computer_ = &BinaryExpressionEvaluator::ConditionalObjectComputer;
    }
    result_type_ = { JType::Boolean };

    return true;
  }

  // TODO: unbox (Java Language Specification section 5.1.8).
  FormatMessageModel unused_error_message;
  if (CompileBooleanConditional(&unused_error_message)) {
    return true;
  }

  // Numerical comparison operators.
  if ((type_ == BinaryJavaExpression::Type::eq) ||
      (type_ == BinaryJavaExpression::Type::ne) ||
      (type_ == BinaryJavaExpression::Type::le) ||
      (type_ == BinaryJavaExpression::Type::ge) ||
      (type_ == BinaryJavaExpression::Type::lt) ||
      (type_ == BinaryJavaExpression::Type::gt)) {
    // Apply numeric promotions (Java Language Specification section 5.6.2)
    // and initialize the computation routine.
    if (IsEitherType(JType::Double)) {
      if (!ApplyNumericPromotions<jdouble>(error_message)) {
        return false;
      }

      computer_ =
          &BinaryExpressionEvaluator::NumericalComparisonComputer<jdouble>;
    } else if (IsEitherType(JType::Float)) {
      if (!ApplyNumericPromotions<jfloat>(error_message)) {
        return false;
      }

      computer_ =
          &BinaryExpressionEvaluator::NumericalComparisonComputer<jfloat>;
    } else if (IsEitherType(JType::Long)) {
      if (!ApplyNumericPromotions<jlong>(error_message)) {
        return false;
      }

      computer_ =
          &BinaryExpressionEvaluator::NumericalComparisonComputer<jlong>;
    } else {
      if (!ApplyNumericPromotions<jint>(error_message)) {
        return false;
      }

      computer_ = &BinaryExpressionEvaluator::NumericalComparisonComputer<jint>;
    }

    result_type_ = { JType::Boolean };
    return true;
  }

  *error_message = { TypeMismatch };

  return false;
}


bool BinaryExpressionEvaluator::CompileBooleanConditional(
    FormatMessageModel* error_message) {
  // Conditional operations that apply to boolean arguments.
  if (IsBooleanType(arg1_->GetStaticType().type) &&
      IsBooleanType(arg2_->GetStaticType().type) &&
      ((type_ == BinaryJavaExpression::Type::conditional_and) ||
       (type_ == BinaryJavaExpression::Type::conditional_or) ||
       (type_ == BinaryJavaExpression::Type::eq) ||
       (type_ == BinaryJavaExpression::Type::ne) ||
       (type_ == BinaryJavaExpression::Type::bitwise_and) ||
       (type_ == BinaryJavaExpression::Type::bitwise_or) ||
       (type_ == BinaryJavaExpression::Type::bitwise_xor))) {
    computer_ = &BinaryExpressionEvaluator::ConditionalBooleanComputer;
    result_type_ = { JType::Boolean };

    return true;
  }

  *error_message = { TypeMismatch };

  return false;
}


bool BinaryExpressionEvaluator::CompileBitwise(
    FormatMessageModel* error_message) {
  // TODO: unbox (Java Language Specification section 5.1.8).

  // Bitwise operators become conditional when applied to boolean arguments
  // (Java Language Specification, section 15.22.2).
  FormatMessageModel unused_error_message;
  if (CompileBooleanConditional(&unused_error_message)) {
    return true;
  }

  // Integer bitwise operators are only applicable to int and long.
  if (!IsIntegerType(arg1_->GetStaticType().type) ||
      !IsIntegerType(arg2_->GetStaticType().type)) {
    *error_message = { TypeMismatch };
    return false;
  }

  // Shift of "long".
  if (IsEitherType(JType::Long)) {
    if (!ApplyNumericPromotions<jlong>(error_message)) {
      return false;
    }

    computer_ = &BinaryExpressionEvaluator::BitwiseComputer<jlong>;
    result_type_ = { JType::Long };

    return true;
  }

  // Shift of "int".
  if (!ApplyNumericPromotions<jint>(error_message)) {
    return false;
  }

  computer_ = &BinaryExpressionEvaluator::BitwiseComputer<jint>;
  result_type_ = { JType::Int };

  return true;
}


bool BinaryExpressionEvaluator::CompileShift(
    FormatMessageModel* error_message) {
  // TODO: unbox (Java Language Specification section 5.1.8).

  // Numeric promotion is applied separately for each argument
  // (Java Language Specification section 15.19)
  if (!IsIntegerType(arg1_->GetStaticType().type) ||
      !IsIntegerType(arg2_->GetStaticType().type)) {
    *error_message = { TypeMismatch };
    return false;
  }

  if (!ApplyShiftNumericPromotion(&arg1_, error_message) ||
      !ApplyShiftNumericPromotion(&arg2_, error_message)) {
    return false;
  }

  switch (arg1_->GetStaticType().type) {
    case JType::Int:
      computer_ =
          &BinaryExpressionEvaluator::ShiftComputer<jint, uint32_t, 0x1f>;
      result_type_ = { JType::Int };

      return true;

    case JType::Long:
      computer_ =
          &BinaryExpressionEvaluator::ShiftComputer<jlong, uint64_t, 0x3f>;
      result_type_ = { JType::Long };

      return true;

    default:
      *error_message = { TypeMismatch };
      return false;
  }
}


bool BinaryExpressionEvaluator::IsEitherType(JType type) const {
  return (arg1_->GetStaticType().type == type) ||
         (arg2_->GetStaticType().type == type);
}


template <typename TTargetType>
bool BinaryExpressionEvaluator::ApplyNumericPromotions(
    FormatMessageModel* error_message) {
  return ApplyNumericCast<TTargetType>(&arg1_, error_message) &&
         ApplyNumericCast<TTargetType>(&arg2_, error_message);
}


bool BinaryExpressionEvaluator::ApplyShiftNumericPromotion(
    std::unique_ptr<ExpressionEvaluator>* arg,
    FormatMessageModel* error_message) {
  switch ((*arg)->GetStaticType().type) {
    case JType::Byte:
    case JType::Char:
    case JType::Short:
      return ApplyNumericCast<jint>(arg, error_message);

    case JType::Int:
    case JType::Long:
      return true;  // No numeric promotion needed.

    default:
      *error_message = { TypeMismatch };
      return false;  // Shift operator not applicable for this type.
  }
}


ErrorOr<JVariant> BinaryExpressionEvaluator::Evaluate(
    const EvaluationContext& evaluation_context) const {
  ErrorOr<JVariant> arg1_value = arg1_->Evaluate(evaluation_context);
  if (arg1_value.is_error()) {
    return arg1_value;
  }

  // Short-circuit the evaluation for "&&" or "||": don't evaluate arg2_ when
  // arg1_ can decide the value of the expression.
  if (type_ == BinaryJavaExpression::Type::conditional_and ||
      type_ == BinaryJavaExpression::Type::conditional_or) {
    jboolean arg1_value_boolean;
    if (!arg1_value.value().get<jboolean>(&arg1_value_boolean)) {
      return INTERNAL_ERROR_MESSAGE;
    }
    if ((type_ == BinaryJavaExpression::Type::conditional_and &&
         arg1_value_boolean == false) ||
        (type_ == BinaryJavaExpression::Type::conditional_or &&
         arg1_value_boolean == true)) {
      return JVariant::Boolean(arg1_value_boolean);
    }
  }

  ErrorOr<JVariant> arg2_value = arg2_->Evaluate(evaluation_context);
  if (arg2_value.is_error()) {
    return arg2_value;
  }

  return (this->*computer_)(arg1_value.value(), arg2_value.value());
}


template <typename T>
ErrorOr<JVariant> BinaryExpressionEvaluator::ArithmeticComputer(
    const JVariant& arg1,
    const JVariant& arg2) const {
  T value1 = T();
  if (!arg1.get<T>(&value1)) {
    return INTERNAL_ERROR_MESSAGE;
  }

  T value2 = T();
  if (!arg2.get<T>(&value2)) {
    return INTERNAL_ERROR_MESSAGE;
  }

  switch (type_) {
    case BinaryJavaExpression::Type::add:
      return JVariant::Primitive<T>(value1 + value2);

    case BinaryJavaExpression::Type::sub:
      return JVariant::Primitive<T>(value1 - value2);

    case BinaryJavaExpression::Type::mul:
      return JVariant::Primitive<T>(value1 * value2);

    case BinaryJavaExpression::Type::mod:
    case BinaryJavaExpression::Type::div:
      if (IsDivisionByZero(value2)) {
        return FormatMessageModel { DivisionByZero };
      }

      if (IsDivisionOverflow(value1, value2)) {
        return FormatMessageModel { IntegerDivisionOverflow };
      }

      if (type_ == BinaryJavaExpression::Type::div) {
        return JVariant::Primitive<T>(value1 / value2);
      } else {
        return JVariant::Primitive<T>(ComputeModulo(value1, value2));
      }

    default:
      DCHECK(false);  // Any non arithmetic operations are unexpected here.
      return INTERNAL_ERROR_MESSAGE;
  }
}


template <typename T>
ErrorOr<JVariant> BinaryExpressionEvaluator::BitwiseComputer(
    const JVariant& arg1,
    const JVariant& arg2) const {
  T value1 = T();
  if (!arg1.get<T>(&value1)) {
    return INTERNAL_ERROR_MESSAGE;
  }

  T value2 = T();
  if (!arg2.get<T>(&value2)) {
    return INTERNAL_ERROR_MESSAGE;
  }

  switch (type_) {
    case BinaryJavaExpression::Type::bitwise_and:
      return JVariant::Primitive<T>(value1 & value2);

    case BinaryJavaExpression::Type::bitwise_or:
      return JVariant::Primitive<T>(value1 | value2);

    case BinaryJavaExpression::Type::bitwise_xor:
      return JVariant::Primitive<T>(value1 ^ value2);

    default:
      DCHECK(false);  // Any other operations are unexpected here.
      return INTERNAL_ERROR_MESSAGE;
  }
}

template <typename T, typename TUnsigned, uint16_t Bitmask>
ErrorOr<JVariant> BinaryExpressionEvaluator::ShiftComputer(
    const JVariant& arg1, const JVariant& arg2) const {
  T value1 = T();
  if (!arg1.get<T>(&value1)) {
    return INTERNAL_ERROR_MESSAGE;
  }

  jint value2 = 0;
  if (!arg2.get<jint>(&value2)) {
    jlong value2_long = 0;
    if (!arg2.get<jlong>(&value2_long)) {
      return INTERNAL_ERROR_MESSAGE;
    }

    value2 = static_cast<jint>(value2_long);
  }

  // From Java Language Specification, section 15.19:
  // If the promoted type of the left-hand operand is int, only the five lowest-
  // order bits of the right-hand operand are used as the shift distance. It is
  // as if the right-hand operand were subjected to a bitwise logical AND
  // operator & (15.22.1) with the mask value 0x1f (0b11111). The shift
  // distance actually used is therefore always in the range 0 to 31, inclusive.
  // If the promoted type of the left-hand operand is long, then only the six
  // lowest-order bits of the right-hand operand are used as the shift distance.
  // It is as if the right-hand operand were subjected to a bitwise logical AND
  // operator & (15.22.1) with the mask value 0x3f (0b111111). The shift
  // distance actually used is therefore always in the range 0 to 63, inclusive.
  value2 &= Bitmask;

  switch (type_) {
    case BinaryJavaExpression::Type::shl:
      return JVariant::Primitive<T>(value1 << value2);

    case BinaryJavaExpression::Type::shr_s:
      return JVariant::Primitive<T>(value1 >> value2);

    case BinaryJavaExpression::Type::shr_u:
      return JVariant::Primitive<T>(static_cast<TUnsigned>(value1) >> value2);

    default:
      DCHECK(false);  // Any operations other than shift are unexpected here.
      return INTERNAL_ERROR_MESSAGE;
  }
}

ErrorOr<JVariant> BinaryExpressionEvaluator::ConditionalObjectComputer(
    const JVariant& arg1,
    const JVariant& arg2) const {
  jobject object1 = nullptr;
  if (!arg1.get<jobject>(&object1)) {
    return INTERNAL_ERROR_MESSAGE;
  }

  jobject object2 = nullptr;
  if (!arg2.get<jobject>(&object2)) {
    return INTERNAL_ERROR_MESSAGE;
  }

  switch (type_) {
    case BinaryJavaExpression::Type::eq:
      return JVariant::Boolean(jni()->IsSameObject(object1, object2));

    case BinaryJavaExpression::Type::ne:
      return JVariant::Boolean(!jni()->IsSameObject(object1, object2));

    default:
      DCHECK(false);  // Any other operations are not supported for objects.
      return INTERNAL_ERROR_MESSAGE;
  }
}


ErrorOr<JVariant> BinaryExpressionEvaluator::ConditionalStringComputer(
    const JVariant& arg1,
    const JVariant& arg2) const {
  jobject object1 = nullptr;
  if (!arg1.get<jobject>(&object1)) {
    return INTERNAL_ERROR_MESSAGE;
  }

  jobject object2 = nullptr;
  if (!arg2.get<jobject>(&object2)) {
    return INTERNAL_ERROR_MESSAGE;
  }

  const bool is_equal =
      IsEqual(static_cast<jstring>(object1), static_cast<jstring>(object2));

  switch (type_) {
    case BinaryJavaExpression::Type::eq:
      return JVariant::Boolean(is_equal);

    case BinaryJavaExpression::Type::ne:
      return JVariant::Boolean(!is_equal);

    default:
      DCHECK(false);  // Any other operations are not supported for strings.
      return INTERNAL_ERROR_MESSAGE;
  }
}


ErrorOr<JVariant> BinaryExpressionEvaluator::ConditionalBooleanComputer(
    const JVariant& arg1,
    const JVariant& arg2) const {
  jboolean boolean1 = false;
  if (!arg1.get<jboolean>(&boolean1)) {
    return INTERNAL_ERROR_MESSAGE;
  }

  jboolean boolean2 = false;
  if (!arg2.get<jboolean>(&boolean2)) {
    return INTERNAL_ERROR_MESSAGE;
  }

  switch (type_) {
    case BinaryJavaExpression::Type::conditional_and:
    case BinaryJavaExpression::Type::bitwise_and:
      return JVariant::Boolean(boolean1 && boolean2);

    case BinaryJavaExpression::Type::conditional_or:
    case BinaryJavaExpression::Type::bitwise_or:
      return JVariant::Boolean(boolean1 || boolean2);

    case BinaryJavaExpression::Type::eq:
      return JVariant::Boolean(boolean1 == boolean2);

    case BinaryJavaExpression::Type::ne:
    case BinaryJavaExpression::Type::bitwise_xor:
      return JVariant::Boolean(boolean1 != boolean2);

    default:
      DCHECK(false);  // Any other operations are unexpected here.
      return INTERNAL_ERROR_MESSAGE;
  }
}


template <typename T>
ErrorOr<JVariant> BinaryExpressionEvaluator::NumericalComparisonComputer(
    const JVariant& arg1,
    const JVariant& arg2) const {
  T value1 = T();
  if (!arg1.get<T>(&value1)) {
    return INTERNAL_ERROR_MESSAGE;
  }

  T value2 = T();
  if (!arg2.get<T>(&value2)) {
    return INTERNAL_ERROR_MESSAGE;
  }

  switch (type_) {
    case BinaryJavaExpression::Type::eq:
      return JVariant::Boolean(value1 == value2);

    case BinaryJavaExpression::Type::ne:
    case BinaryJavaExpression::Type::bitwise_xor:
      return JVariant::Boolean(value1 != value2);

    case BinaryJavaExpression::Type::le:
      return JVariant::Boolean(value1 <= value2);

    case BinaryJavaExpression::Type::ge:
      return JVariant::Boolean(value1 >= value2);

    case BinaryJavaExpression::Type::lt:
      return JVariant::Boolean(value1 < value2);

    case BinaryJavaExpression::Type::gt:
      return JVariant::Boolean(value1 > value2);

    default:
      DCHECK(false);  // Any other operations are not supported for booleans.
      return INTERNAL_ERROR_MESSAGE;
  }
}


}  // namespace cdbg
}  // namespace devtools

