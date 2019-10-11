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

#include "identifier_evaluator.h"

#include "field_evaluator.h"
#include "instance_field_reader.h"
#include "local_variable_reader.h"
#include "messages.h"
#include "model.h"
#include "readers_factory.h"
#include "static_field_reader.h"

namespace devtools {
namespace cdbg {

IdentifierEvaluator::IdentifierEvaluator(std::string identifier_name)
    : identifier_name_(identifier_name) {}

IdentifierEvaluator::~IdentifierEvaluator() {
  if (static_field_reader_ != nullptr) {
    static_field_reader_->ReleaseRef();
  }
}


bool IdentifierEvaluator::Compile(
    ReadersFactory* readers_factory,
    FormatMessageModel* error_message) {
  *error_message = FormatMessageModel();

  // Case 1: this is a local variable.
  FormatMessageModel local_variable_message;
  variable_reader_ = readers_factory->CreateLocalVariableReader(
      identifier_name_,
      &local_variable_message);
  if (variable_reader_ != nullptr) {
    result_type_ = variable_reader_->GetStaticType();
    computer_ = &IdentifierEvaluator::LocalVariableComputer;
    return true;
  }

  // Case 2: implicitly referenced instance field ("myInt" is equivalent to
  // "this.myInt" unless we are in a static method).
  FormatMessageModel local_instance_message;
  std::unique_ptr<LocalVariableReader> local_instance_reader =
      readers_factory->CreateLocalInstanceReader();
  if (local_instance_reader != nullptr) {
    auto chain = CreateInstanceFieldReadersChain(
        readers_factory,
        local_instance_reader->GetStaticType().object_signature,
        identifier_name_,
        &local_instance_message);

    if (!chain.empty()) {
      variable_reader_ = std::move(local_instance_reader);
      instance_fields_chain_ = std::move(chain);

      result_type_ = instance_fields_chain_.back()->GetStaticType();
      computer_ = &IdentifierEvaluator::ImplicitInstanceFieldComputer;

      return true;
    }

    // "Field not found" error is considered non-specific here.
    if (local_instance_message.format == InstanceFieldNotFound) {
      local_instance_message = FormatMessageModel();
    }
  }

  // Case 3: static variable in the class containing the current evaluation
  // point.
  FormatMessageModel static_field_message;
  std::unique_ptr<StaticFieldReader> static_field_reader =
      readers_factory->CreateStaticFieldReader(
          identifier_name_,
          &static_field_message);
  if (static_field_reader != nullptr) {
    static_field_reader_ = std::move(static_field_reader);

    result_type_ = static_field_reader_->GetStaticType();
    computer_ = &IdentifierEvaluator::StaticFieldComputer;

    return true;
  }

  // Choose the most specific message defaulting to "invalid
  // identifier".
  const FormatMessageModel* messages[] = {
    &local_variable_message,
    &local_instance_message,
    &static_field_message
  };

  for (const FormatMessageModel* message : messages) {
    if (!message->format.empty() &&
        message->format != InvalidIdentifier) {
      *error_message = *message;
      break;
    }
  }

  if (error_message->format.empty()) {
    *error_message = { InvalidIdentifier, { identifier_name_ } };
  }

  return false;
}


ErrorOr<JVariant> IdentifierEvaluator::Evaluate(
    const EvaluationContext& evaluation_context) const {
  return (this->*computer_)(evaluation_context);
}


ErrorOr<JVariant> IdentifierEvaluator::LocalVariableComputer(
    const EvaluationContext& evaluation_context) const {
  JVariant result;
  FormatMessageModel error;
  if (!variable_reader_->ReadValue(evaluation_context, &result, &error)) {
    return error;
  }

  return std::move(result);
}


ErrorOr<JVariant> IdentifierEvaluator::ImplicitInstanceFieldComputer(
    const EvaluationContext& evaluation_context) const {
  JVariant result;
  FormatMessageModel error;
  if (!variable_reader_->ReadValue(evaluation_context, &result, &error)) {
    return error;
  }

  for (const auto& reader : instance_fields_chain_) {
    jobject source_jobject = nullptr;
    if (!result.get<jobject>(&source_jobject)) {
      return INTERNAL_ERROR_MESSAGE;
    }

    if (source_jobject == nullptr) {
      // Attempt to dereference null object.
      return FormatMessageModel { NullPointerDereference };
    }

    JVariant next;
    FormatMessageModel error;
    if (!reader->ReadValue(source_jobject, &next, &error)) {
      return error;
    }

    result = std::move(next);
  }

  return std::move(result);
}


ErrorOr<JVariant> IdentifierEvaluator::StaticFieldComputer(
    const EvaluationContext& evaluation_context) const {
  JVariant result;
  FormatMessageModel error;
  if (!static_field_reader_->ReadValue(&result, &error)) {
    return error;
  }

  return std::move(result);
}

}  // namespace cdbg
}  // namespace devtools

