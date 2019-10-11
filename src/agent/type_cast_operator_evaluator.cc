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

#include "type_cast_operator_evaluator.h"

#include "class_indexer.h"
#include "common.h"
#include "jvmti_buffer.h"
#include "messages.h"
#include "model.h"
#include "numeric_cast_evaluator.h"
#include "readers_factory.h"
#include "type_util.h"

namespace devtools {
namespace cdbg {

TypeCastOperatorEvaluator::TypeCastOperatorEvaluator(
    std::unique_ptr<ExpressionEvaluator> source, const std::string& target_type)
    : source_(std::move(source)),
      target_type_(target_type),
      target_class_(nullptr),
      computer_(nullptr) {
  result_type_.type = JType::Object;
}

TypeCastOperatorEvaluator::~TypeCastOperatorEvaluator() {
  if (target_class_ != nullptr) {
    jni()->DeleteGlobalRef(target_class_);
    target_class_ = nullptr;
  }
}


bool TypeCastOperatorEvaluator::Compile(
    ReadersFactory* readers_factory,
    FormatMessageModel* error_message) {
  if (!source_->Compile(readers_factory, error_message)) {
    return false;
  }

  result_type_ = source_->GetStaticType();

  // Checks if only one of the source and target is boolean.
  if (IsInvalidPrimitiveBooleanTypeConversion()) {
    // Currently, it is going to say Invalid for boolean unbox cases.
    // TODO : Fix this soon.
    *error_message = {
        TypeCastCompileInvalid,
        { target_type_, TypeNameFromSignature(source_->GetStaticType()) }
    };
    return false;
  }

  if (AreBothTypesPrimitiveBoolean()) {
    computer_ = &TypeCastOperatorEvaluator::DoNothingComputer;
    return true;
  }

  // TODO: Handle unbox. Or at least put the unsupported information
  // clearly in the error_message.
  if (IsNumericJType(source_->GetStaticType().type) &&
      NumericTypeNameToJType(target_type_, &(result_type_.type))) {
    switch (result_type_.type) {
      case JType::Byte:
        if (!ApplyNumericCast<jbyte>(&source_, error_message)) {
          return false;
        }
        break;

      case JType::Char:
        if (!ApplyNumericCast<jchar>(&source_, error_message)) {
          return false;
        }
        break;

      case JType::Short:
        if (!ApplyNumericCast<jshort>(&source_, error_message)) {
          return false;
        }
        break;

      case JType::Int:
        if (!ApplyNumericCast<jint>(&source_, error_message)) {
          return false;
        }
        break;

      case JType::Long:
        if (!ApplyNumericCast<jlong>(&source_, error_message)) {
          return false;
        }
        break;

      case JType::Float:
        if (!ApplyNumericCast<jfloat>(&source_, error_message)) {
          return false;
        }
        break;

      case JType::Double:
        if (!ApplyNumericCast<jdouble>(&source_, error_message)) {
          return false;
        }
        break;

      default:
        LOG(ERROR) << "Unknown Numeric type.";
        return false;
    }

    computer_ = &TypeCastOperatorEvaluator::DoNothingComputer;
    return true;
  }

  if (!IsNumericTypeName(target_type_) &&
      (source_->GetStaticType().type == JType::Object)) {
    // Both source and target are Objects.
    computer_ = &TypeCastOperatorEvaluator::ObjectTypeComputer;

    JniLocalRef target_class_local_ref =
        readers_factory->FindClassByName(target_type_, error_message);
    if (target_class_local_ref == nullptr) {
      return false;
    }

    target_class_ = jni()->NewGlobalRef(target_class_local_ref.get());

    JvmtiBuffer<char> signature;
    if (jvmti()->GetClassSignature(
          static_cast<jclass>(target_class_),
          signature.ref(),
          nullptr) != JVMTI_ERROR_NONE) {
      *error_message = INTERNAL_ERROR_MESSAGE;
      return false;
    }

    result_type_.object_signature = signature.get();

    if (IsEitherTypeObjectArray()) {
      *error_message = {
          TypeCastUnsupported,
          { TypeNameFromSignature(source_->GetStaticType()), target_type_ }
      };
      return false;
    }

    return true;
  }

  *error_message = {
      TypeCastUnsupported,
      { TypeNameFromSignature(source_->GetStaticType()), target_type_ }
  };
  return false;
}


ErrorOr<JVariant> TypeCastOperatorEvaluator::Evaluate(
    const EvaluationContext& evaluation_context) const {
  ErrorOr<JVariant> source_result = source_->Evaluate(evaluation_context);
  if (source_result.is_error()) {
    return source_result;
  }

  return (this->*computer_)(source_result.value());
}


bool TypeCastOperatorEvaluator::IsInvalidPrimitiveBooleanTypeConversion()
  const {
  if (target_type_ == "boolean" &&
      IsNumericJType(source_->GetStaticType().type)) {
    return true;
  }

  if ((IsNumericTypeName(target_type_)) &&
     (IsBooleanType(source_->GetStaticType().type))) {
      return true;
  }
  return false;
}


bool TypeCastOperatorEvaluator::AreBothTypesPrimitiveBoolean() const {
  return (target_type_ == "boolean")
      && IsBooleanType(source_->GetStaticType().type);
}


bool TypeCastOperatorEvaluator::IsEitherTypeObjectArray() {
  return IsArrayObjectType(result_type_) ||
      IsArrayObjectType(source_->GetStaticType());
}


ErrorOr<JVariant> TypeCastOperatorEvaluator::DoNothingComputer(
    const JVariant& source) const {
  return JVariant(source);
}


ErrorOr<JVariant> TypeCastOperatorEvaluator::ObjectTypeComputer(
    const JVariant& source) const {
  jobject source_value = nullptr;
  if (!source.get<jobject>(&source_value)) {
    LOG(ERROR) << "Couldn't extract the source value as an Object: "
               << source.ToString(false);
    return INTERNAL_ERROR_MESSAGE;
  }

  if (!jni()->IsInstanceOf(source_value,
                           static_cast<jclass>(target_class_))) {
    return FormatMessageModel {
        TypeCastEvaluateInvalid,
        { TypeNameFromSignature(source_->GetStaticType()), target_type_ }
    };
  }

  JVariant result;
  result.assign_new_ref(JVariant::ReferenceKind::Local, source_value);

  return std::move(result);
}

}  // namespace cdbg
}  // namespace devtools
