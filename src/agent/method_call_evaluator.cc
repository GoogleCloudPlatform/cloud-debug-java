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

#include "method_call_evaluator.h"

#include <sstream>

#include "class_indexer.h"
#include "class_metadata_reader.h"
#include "jni_utils.h"
#include "local_variable_reader.h"
#include "messages.h"
#include "method_caller.h"
#include "model.h"
#include "readers_factory.h"
#include "type_util.h"

namespace devtools {
namespace cdbg {

MethodCallEvaluator::MethodCallEvaluator(
    std::string method_name,
    std::unique_ptr<ExpressionEvaluator> instance_source,
    std::string possible_class_name,
    std::vector<std::unique_ptr<ExpressionEvaluator>> arguments)
    : method_name_(std::move(method_name)),
      instance_source_(std::move(instance_source)),
      possible_class_name_(std::move(possible_class_name)),
      arguments_(std::move(arguments)) {}

MethodCallEvaluator::~MethodCallEvaluator() {
}


bool MethodCallEvaluator::Compile(
    ReadersFactory* readers_factory,
    FormatMessageModel* error_message) {
  // Compile argument expressions.
  std::ostringstream argument_types;
  bool is_first_argument = true;
  for (auto& argument : arguments_) {
    if (!argument->Compile(readers_factory, error_message)) {
      return false;
    }

    if (!is_first_argument) {
      argument_types << ", ";
    } else {
      is_first_argument = false;
    }
    argument_types << TypeNameFromSignature(argument->GetStaticType());
  }

  LOG(INFO) << "Compiling method call " << method_name_
            << '(' << argument_types.str() << ')';

  bool matched = false;

  *error_message = FormatMessageModel();

  if ((instance_source_ == nullptr) && possible_class_name_.empty()) {
    // Case 1: implicitly referenced instance field ("doSomething()" is
    // equivalent to "this.doSomething()" unless we are in a static method).
    local_instance_reader_ = readers_factory->CreateLocalInstanceReader();
    if (local_instance_reader_ != nullptr) {
      std::vector<ClassMetadataReader::Method> instance_methods =
          readers_factory->FindLocalInstanceMethods(method_name_);

      MatchMethods(
          readers_factory,
          instance_methods,
          &matched,
          error_message);
      if (matched) {
        return error_message->format.empty();
      }
    }

    // Case 2: static method in the current class.
    std::vector<ClassMetadataReader::Method> static_methods =
        readers_factory->FindStaticMethods(method_name_);

    MatchMethods(
        readers_factory,
        static_methods,
        &matched,
        error_message);
    if (matched) {
      return error_message->format.empty();
    }
  }


  FormatMessageModel instance_source_error_message;
  if (instance_source_ != nullptr) {
    // Case 3: calling method on a result of prior expression (for example:
    // "a.b.startsWith(...)").
    MatchInstanceSourceMethod(
        readers_factory,
        &matched,
        &instance_source_error_message);
    if (matched) {
      *error_message = instance_source_error_message;
      return error_message->format.empty();
    }
  }

  FormatMessageModel fully_qualified_static_method_error_message;
  if (!possible_class_name_.empty()) {
    // Case 4: calling static method outside of the current class.
    MatchExplicitStaticMethod(
        readers_factory,
        &matched,
        &fully_qualified_static_method_error_message);
    if (matched) {
      *error_message = fully_qualified_static_method_error_message;
      return error_message->format.empty();
    }

    fully_qualified_static_method_error_message = {
      StaticMethodNotFound,
      { method_name_, possible_class_name_ }
    };
  }

  // Select the specific error message or return the default one.
  const FormatMessageModel* candidate_messages[] = {
    &instance_source_error_message,
    &fully_qualified_static_method_error_message
  };

  for (const auto* candidate_message : candidate_messages) {
    if (!candidate_message->format.empty() &&
        (candidate_message->format != InvalidIdentifier) &&
        (candidate_message->format != StaticFieldNotFound)) {
      *error_message = *candidate_message;
      break;
    }
  }

  if (error_message->format.empty()) {
    *error_message = {
      ImplicitMethodNotFound,
      { method_name_, readers_factory->GetEvaluationPointClassName() }
    };
  }

  return false;
}


void MethodCallEvaluator::MatchMethods(
    ReadersFactory* readers_factory,
    const std::vector<ClassMetadataReader::Method>& candidate_methods,
    bool* matched,
    FormatMessageModel* error_message) {
  if (candidate_methods.empty()) {
    *matched = false;
    return;
  }

  *matched = true;

  int match_count = 0;
  ClassMetadataReader::Method matched_method;
  JSignature matched_return_type;

  for (const auto& candidate_method : candidate_methods) {
    if (MatchMethod(readers_factory, candidate_method)) {
      LOG(INFO) << "Method matched, class: "
                << TypeNameFromSignature(candidate_method.class_signature)
                << ", signature: " << candidate_method.signature;

      ++match_count;
      matched_method = candidate_method;
    }
  }

  if (match_count == 0) {
    *error_message = {
      (candidate_methods.size() == 1)
          ? MethodCallArgumentsMismatchSingleCandidate
          : MethodCallArgumentsMismatchMultipleCandidates,
      { method_name_ }
    };

    return;
  }

  if (match_count > 1) {
    *error_message = { AmbiguousMethodCall, { method_name_ } };
    return;
  }

  JMethodSignature signature;
  if (!ParseJMethodSignature(matched_method.signature, &signature)) {
    *error_message = INTERNAL_ERROR_MESSAGE;
    return;
  }

  method_ = matched_method;
  return_type_ = signature.return_type;
}


bool MethodCallEvaluator::MatchMethod(
    ReadersFactory* readers_factory,
    const ClassMetadataReader::Method& candidate_method) {
  JMethodSignature method_signature;
  if (!ParseJMethodSignature(candidate_method.signature, &method_signature)) {
    return false;
  }

  if (method_signature.arguments.size() != arguments_.size()) {
    return false;
  }

  for (int i = 0; i < arguments_.size(); ++i) {
    if (!MatchArgument(
            readers_factory,
            method_signature.arguments[i],
            *arguments_[i])) {
      return false;
    }
  }

  return true;
}


bool MethodCallEvaluator::MatchArgument(
    ReadersFactory* readers_factory,
    const JSignature& expected_signature,
    const ExpressionEvaluator& evaluated_signature) {
  const JSignature& static_type = evaluated_signature.GetStaticType();

  if ((expected_signature.type == JType::Object) &&
      (static_type.type == JType::Object)) {
    // "null" implicitly casts into any object.
    Nullable<jvalue> static_value = evaluated_signature.GetStaticValue();
    if (static_value.has_value() && (static_value.value().l == nullptr)) {
      return true;
    }

    // Assignable objects.
    bool is_assignable = readers_factory->IsAssignable(
        static_type.object_signature,
        expected_signature.object_signature);
    if (is_assignable) {
      return true;
    }
  }

  // Actual type is identical to expected type.
  if ((expected_signature.type == static_type.type) &&
      (expected_signature.object_signature == static_type.object_signature)) {
    return true;
  }

  return false;
}


void MethodCallEvaluator::MatchInstanceSourceMethod(
    ReadersFactory* readers_factory,
    bool* matched,
    FormatMessageModel* error_message) {
  if (!instance_source_->Compile(readers_factory, error_message)) {
    *matched = false;
    return;
  }

  if (instance_source_->GetStaticType().type != JType::Object) {
    *error_message = {
      MethodCallOnPrimitiveType,
      {
        method_name_,
        TypeNameFromSignature(instance_source_->GetStaticType())
      }
    };

    *matched = true;
    return;
  }

  std::vector<ClassMetadataReader::Method> instance_methods;
  if (!readers_factory->FindInstanceMethods(
          instance_source_->GetStaticType().object_signature,
          method_name_,
          &instance_methods,
          error_message)) {
    *matched = true;
    return;
  }

  MatchMethods(
      readers_factory,
      std::move(instance_methods),
      matched,
      error_message);

  if (!*matched) {
    *error_message = {
      InstanceMethodNotFound,
      {
        method_name_,
        TypeNameFromSignature(instance_source_->GetStaticType())
      }
    };

    *matched = true;
    return;
  }
}


void MethodCallEvaluator::MatchExplicitStaticMethod(
    ReadersFactory* readers_factory,
    bool* matched,
    FormatMessageModel* error_message) {
  std::vector<ClassMetadataReader::Method> static_methods;
  if (!readers_factory->FindStaticMethods(
      possible_class_name_,
      method_name_,
      &static_methods,
      error_message)) {
    *matched = true;
    return;
  }

  MatchMethods(
      readers_factory,
      std::move(static_methods),
      matched,
      error_message);
}

ErrorOr<JVariant> MethodCallEvaluator::EvaluateSourceObject(
    const EvaluationContext& evaluation_context) const {
  if (method_.is_static()) {
    // The method is static. We don't have and don't need a source object.
    return JVariant();
  }

  if (instance_source_ != nullptr) {
    return instance_source_->Evaluate(evaluation_context);
  }

  if (local_instance_reader_ != nullptr) {
    JVariant source;
    FormatMessageModel error;
    if (local_instance_reader_->ReadValue(
        evaluation_context,
        &source,
        &error)) {
      return std::move(source);
    }

    return error;
  }

  return INTERNAL_ERROR_MESSAGE;
}


ErrorOr<JVariant> MethodCallEvaluator::Evaluate(
    const EvaluationContext& evaluation_context) const {
  // For instance methods we need to obtain the source object.
  ErrorOr<JVariant> source = EvaluateSourceObject(evaluation_context);
  if (source.is_error()) {
    return source;
  }

  // Compute arguments.
  std::vector<JVariant> arguments;
  arguments.reserve(arguments_.size());
  for (int i = 0; i < arguments_.size(); ++i) {
    ErrorOr<JVariant> arg_value = arguments_[i]->Evaluate(evaluation_context);
    if (arg_value.is_error()) {
      return arg_value;
    }

    arguments.push_back(ErrorOr<JVariant>::detach_value(std::move(arg_value)));
  }

  return evaluation_context.method_caller->Invoke(
      method_,
      source.value(),
      std::move(arguments));
}


}  // namespace cdbg
}  // namespace devtools

