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

#include "array_expression_evaluator.h"

#include "array_reader.h"
#include "jni_utils.h"
#include "messages.h"
#include "model.h"
#include "numeric_cast_evaluator.h"
#include "readers_factory.h"
#include "type_util.h"

namespace devtools {
namespace cdbg {

ArrayExpressionEvaluator::ArrayExpressionEvaluator(
    std::unique_ptr<ExpressionEvaluator> source_array,
    std::unique_ptr<ExpressionEvaluator> source_index)
    : source_array_(std::move(source_array)),
      source_index_(std::move(source_index)) {
}


ArrayExpressionEvaluator::~ArrayExpressionEvaluator() {
}


bool ArrayExpressionEvaluator::Compile(
    ReadersFactory* readers_factory,
    FormatMessageModel* error_message) {
  if (!source_array_->Compile(readers_factory, error_message) ||
      !source_index_->Compile(readers_factory, error_message)) {
    return false;
  }

  if (!IsArrayObjectType(source_array_->GetStaticType())) {
    *error_message = {
      ArrayTypeExpected,
      { TypeNameFromSignature(source_array_->GetStaticType()) }
    };
    return false;
  }

  return_type_ = GetArrayElementJSignature(source_array_->GetStaticType());
  if (return_type_.type == JType::Void) {
    *error_message = INTERNAL_ERROR_MESSAGE;
    return false;
  }

  array_reader_ = readers_factory->CreateArrayReader(
      source_array_->GetStaticType());
  if (array_reader_ == nullptr) {
    *error_message = INTERNAL_ERROR_MESSAGE;
    return false;
  }

  if (!IsIntegerType(source_index_->GetStaticType().type)) {
    *error_message = {
      ArrayIndexNotInteger,
      { TypeNameFromSignature(source_index_->GetStaticType()) }
    };
    return false;
  }

  if (!ApplyNumericCast<jlong>(&source_index_, error_message)) {
    *error_message = INTERNAL_ERROR_MESSAGE;
    return false;
  }

  return true;
}


ErrorOr<JVariant> ArrayExpressionEvaluator::Evaluate(
    const EvaluationContext& evaluation_context) const {
  ErrorOr<JVariant> array = source_array_->Evaluate(evaluation_context);
  if (array.is_error()) {
    return array;
  }

  ErrorOr<JVariant> index = source_index_->Evaluate(evaluation_context);
  if (index.is_error()) {
    return index;
  }

  return array_reader_->ReadValue(array.value(), index.value());
}


}  // namespace cdbg
}  // namespace devtools

