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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_NUMERIC_CAST_EVALUATOR_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_NUMERIC_CAST_EVALUATOR_H_

#include "common.h"
#include "expression_evaluator.h"
#include "messages.h"
#include "model.h"

namespace devtools {
namespace cdbg {

// Helper node in expression tree to cast primitive types.
template <typename TTargetType>
class NumericCastEvaluator : public ExpressionEvaluator {
 public:
  // Constructs expression that casts the value in "source" to "TTargetType".
  // This instance owns "source". "source" is initialized at this point:
  // NumericCastEvaluator will not propagate Compile to "source".
  explicit NumericCastEvaluator(
      std::unique_ptr<ExpressionEvaluator> source)
      : source_(std::move(source)),
        result_type_({ TargetType() }) {
  }

  // Gets the "JType" enumeration corresponding to "TTargetType". For example
  // JType::Float corresponds to jfloat.
  static JType TargetType();

  // "NumericCastEvaluator" assumes the caller calls "Compile" on
  // source_.
  bool Compile(
      ReadersFactory* readers_factory,
      FormatMessageModel* error_message) override {
    // Performs compile time type checking. Boolean and objects are the only
    // two types that can't be casted.
    if ((source_->GetStaticType().type == JType::Boolean) ||
        (source_->GetStaticType().type == JType::Object)) {
      *error_message = { TypeMismatch };
      return false;
    }

    return true;
  }

  // Detaches the source expression from this instance. Used to clean up after
  // failed call to "Compile".
  std::unique_ptr<ExpressionEvaluator> MoveSource() {
    return std::move(source_);
  }

  const JSignature& GetStaticType() const override {
    return result_type_;
  }

  Nullable<jvalue> GetStaticValue() const override {
    return nullptr;
  }

  ErrorOr<JVariant> Evaluate(
      const EvaluationContext& evaluation_context) const override {
    ErrorOr<JVariant> source_result = source_->Evaluate(evaluation_context);
    if (source_result.is_error()) {
      return source_result;
    }

    switch (source_result.value().type()) {
      case JType::Byte:
        return Cast<jbyte>(source_result.value());

      case JType::Char:
        return Cast<jchar>(source_result.value());

      case JType::Short:
        return Cast<jshort>(source_result.value());

      case JType::Int:
        return Cast<jint>(source_result.value());

      case JType::Long:
        return Cast<jlong>(source_result.value());

      case JType::Float:
        return Cast<jfloat>(source_result.value());

      case JType::Double:
        return Cast<jdouble>(source_result.value());

      default:
        return INTERNAL_ERROR_MESSAGE;
    }
  }

 private:
  // Utility function to read value of type "TSourceType" from "source",
  // cast it to "TTargetType" and returns it.
  template <typename TSourceType>
  static ErrorOr<JVariant> Cast(const JVariant& source) {
    TSourceType source_value = TSourceType();
    if (!source.get<TSourceType>(&source_value)) {
      return INTERNAL_ERROR_MESSAGE;
    }

    return JVariant::Primitive<TTargetType>(
        static_cast<TTargetType>(source_value));
  }

 private:
  std::unique_ptr<ExpressionEvaluator> source_;

  // Statically computed resulting type of the expression.
  const JSignature result_type_;

  DISALLOW_COPY_AND_ASSIGN(NumericCastEvaluator);
};


template <>
inline JType NumericCastEvaluator<jint>::TargetType() {
  return JType::Int;
}


template <>
inline JType NumericCastEvaluator<jlong>::TargetType() {
  return JType::Long;
}


template <>
inline JType NumericCastEvaluator<jfloat>::TargetType() {
  return JType::Float;
}


template <>
inline JType NumericCastEvaluator<jdouble>::TargetType() {
  return JType::Double;
}


template <>
inline JType NumericCastEvaluator<jshort>::TargetType() {
  return JType::Short;
}


template <>
inline JType NumericCastEvaluator<jchar>::TargetType() {
  return JType::Char;
}


template <>
inline JType NumericCastEvaluator<jbyte>::TargetType() {
  return JType::Byte;
}


// Utility function to wrap the instance of "ExpressionEvaluator" with
// numeric cast.
template <typename TTargetType>
bool ApplyNumericCast(
    std::unique_ptr<ExpressionEvaluator>* e,
    FormatMessageModel* error_message) {
  // If the return type of "e" is the same type we want to cast into, the whole
  // thing can be skipped.
  if ((*e)->GetStaticType().type ==
      NumericCastEvaluator<TTargetType>::TargetType()) {
    return true;
  }

  std::unique_ptr<NumericCastEvaluator<TTargetType>> cast(
      new NumericCastEvaluator<TTargetType>(std::move(*e)));

  // NumericCastEvaluator doesn't really need ReadersFactory.
  if (!cast->Compile(nullptr, error_message)) {
    // Revert back as if nothing happened.
    *e = cast->MoveSource();
    return false;
  }

  *e = std::move(cast);
  return true;
}


}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_NUMERIC_CAST_EVALUATOR_H_


