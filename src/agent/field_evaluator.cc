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

#include "field_evaluator.h"

#include "instance_field_reader.h"
#include "messages.h"
#include "model.h"
#include "readers_factory.h"
#include "static_field_reader.h"

namespace devtools {
namespace cdbg {

// Maximum depth of inner classes we support to follow the implicit
// chain of references.
static constexpr int kMaxInnerClassesDepth = 10;

std::vector<std::unique_ptr<InstanceFieldReader>>
CreateInstanceFieldReadersChain(ReadersFactory* readers_factory,
                                const std::string& class_signature,
                                const std::string& field_name,
                                FormatMessageModel* error_message) {
  std::vector<std::unique_ptr<InstanceFieldReader>> chain;
  std::unique_ptr<InstanceFieldReader> reader;

  // Try as "this.field".
  reader = readers_factory->CreateInstanceFieldReader(
      class_signature,
      field_name,
      error_message);
  if (reader != nullptr) {
    chain.push_back(std::move(reader));
    return chain;
  }

  // Inner classes will have "this$N" fields pointing to the outer class.
  // First level inner class will have "this$0". Second level will have
  // "this$1" and so on. The instance field can be in either.
  int inner_depth = kMaxInnerClassesDepth - 1;
  for (; inner_depth >= 0; --inner_depth) {
    FormatMessageModel inner_reader_error_message;
    reader = readers_factory->CreateInstanceFieldReader(
      class_signature,
      "this$" + std::to_string(inner_depth),
      &inner_reader_error_message);
    if (reader != nullptr) {
      break;
    }
  }

  if (reader == nullptr) {
    // This is not an inner class or exceeds maximum depth (which is unlikely).
    return {};
  }

  // This is an inner class. Iterate through chain of inner classes looking
  // for references.
  chain.push_back(std::move(reader));

  for (; inner_depth >= 0; --inner_depth) {
    reader = readers_factory->CreateInstanceFieldReader(
        chain.back()->GetStaticType().object_signature,
        field_name,
        error_message);
    if (reader != nullptr) {
      chain.push_back(std::move(reader));
      return chain;
    }

    if (inner_depth > 0) {
      std::string this_name = "this$" + std::to_string(inner_depth - 1);
      reader = readers_factory->CreateInstanceFieldReader(
          chain.back()->GetStaticType().object_signature,
          this_name,
          error_message);
      if (reader == nullptr) {
        LOG(WARNING) << "Broken inner classes reference chain, "
                     << "inner class: "
                     << chain.back()->GetStaticType().object_signature
                     << ", field: " << this_name;
        return {};
      }

      chain.push_back(std::move(reader));
    }
  }

  // We iterated through all the inner classes and still found nothing.
  return {};
}

FieldEvaluator::FieldEvaluator(
    std::unique_ptr<ExpressionEvaluator> instance_source,
    std::string identifier_name, std::string possible_class_name,
    std::string field_name)
    : instance_source_(std::move(instance_source)),
      identifier_name_(std::move(identifier_name)),
      possible_class_name_(std::move(possible_class_name)),
      field_name_(std::move(field_name)),
      computer_(nullptr) {}

FieldEvaluator::~FieldEvaluator() {
  if (static_field_reader_ != nullptr) {
    static_field_reader_->ReleaseRef();
  }
}


bool FieldEvaluator::Compile(
    ReadersFactory* readers_factory,
    FormatMessageModel* error_message) {
  FormatMessageModel instance_field_message;
  if (CompileInstanceField(readers_factory, &instance_field_message)) {
    return true;
  }

  FormatMessageModel static_field_message;
  if (CompileStaticField(readers_factory, &static_field_message)) {
    return true;
  }

  const bool specific_instance_field_message =
      !instance_field_message.format.empty() &&
      (instance_field_message.format != InvalidIdentifier);

  const bool specific_static_field_message =
      !static_field_message.format.empty() &&
      (static_field_message.format != InvalidIdentifier);

  // Choose the specific error.
  if (specific_instance_field_message) {
    *error_message = std::move(instance_field_message);
  } else if (specific_static_field_message) {
    *error_message = std::move(static_field_message);
  } else {
    // Both attempts to compile the expression as an instance field and as a
    // static field failed with a non specific error. Return the same
    // unspecific error, just expand the name of identifier to include the
    // current field name.
    *error_message = { InvalidIdentifier, { identifier_name_ } };
  }

  return false;
}


bool FieldEvaluator::CompileInstanceField(
    ReadersFactory* readers_factory,
    FormatMessageModel* error_message) {
  if (!instance_source_->Compile(readers_factory, error_message)) {
    return false;
  }

  const JSignature& instance_source_signature =
      instance_source_->GetStaticType();

  // Take care of "primitiveArray.length" expression. This is a special
  // construct in Java language.
  if ((field_name_ == "length") &&
      IsArrayObjectType(instance_source_signature)) {
    result_type_ = { JType::Int };
    computer_ = &FieldEvaluator::ArrayLengthComputer;

    return true;
  }

  if (instance_source_signature.type != JType::Object) {
    *error_message = {
      PrimitiveTypeField,
      { TypeNameFromSignature(instance_source_signature), field_name_ }
    };

    return false;
  }

  if (instance_source_signature.object_signature.empty()) {
    LOG(ERROR) << "Signature of source object not available";

    // This should not normally happen, so don't bother with a dedicated error
    // message template.
    *error_message = {
      InstanceFieldNotFound,
      { TypeNameFromSignature(instance_source_signature), field_name_ }
    };

    return false;
  }

  instance_fields_chain_ = CreateInstanceFieldReadersChain(
      readers_factory,
      instance_source_signature.object_signature,
      field_name_,
      error_message);
  if (instance_fields_chain_.empty()) {
    return false;
  }

  result_type_ = instance_fields_chain_.back()->GetStaticType();
  computer_ = &FieldEvaluator::InstanceFieldComputer;

  return true;
}


bool FieldEvaluator::CompileStaticField(
    ReadersFactory* readers_factory,
    FormatMessageModel* error_message) {
  if (possible_class_name_.empty()) {
    return false;
  }

  static_field_reader_ = readers_factory->CreateStaticFieldReader(
      possible_class_name_, field_name_, error_message);
  if (static_field_reader_ == nullptr) {
    return false;
  }

  result_type_ = static_field_reader_->GetStaticType();
  computer_ = &FieldEvaluator::StaticFieldComputer;

  return true;
}


ErrorOr<JVariant> FieldEvaluator::Evaluate(
    const EvaluationContext& evaluation_context) const {
  return (this->*computer_)(evaluation_context);
}


ErrorOr<JVariant> FieldEvaluator::InstanceFieldComputer(
    const EvaluationContext& evaluation_context) const {
  ErrorOr<JVariant> source = instance_source_->Evaluate(evaluation_context);
  if (source.is_error()) {
    return source;
  }

  JVariant result = ErrorOr<JVariant>::detach_value(std::move(source));
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


ErrorOr<JVariant> FieldEvaluator::ArrayLengthComputer(
    const EvaluationContext& evaluation_context) const {
  ErrorOr<JVariant> source = instance_source_->Evaluate(evaluation_context);
  if (source.is_error()) {
    return source;
  }

  jobject source_jobject = nullptr;
  if (!source.value().get<jobject>(&source_jobject)) {
    return INTERNAL_ERROR_MESSAGE;
  }

  if (source_jobject == nullptr) {
    // Attempt to dereference null object.
    return FormatMessageModel { NullPointerDereference };
  }

  return JVariant::Int(
      jni()->GetArrayLength(static_cast<jarray>(source_jobject)));
}


ErrorOr<JVariant> FieldEvaluator::StaticFieldComputer(
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

