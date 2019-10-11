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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_IDENTIFIER_EVALUATOR_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_IDENTIFIER_EVALUATOR_H_

#include "common.h"
#include "expression_evaluator.h"

namespace devtools {
namespace cdbg {

class LocalVariableReader;
class InstanceFieldReader;
class StaticFieldReader;

// Evaluates local variables, static variables and member variables encountered
// in Java expression.
class IdentifierEvaluator : public ExpressionEvaluator {
 public:
  explicit IdentifierEvaluator(std::string identifier_name);

  ~IdentifierEvaluator() override;

  bool Compile(
      ReadersFactory* readers_factory,
      FormatMessageModel* error_message) override;

  const JSignature& GetStaticType() const override { return result_type_; }

  Nullable<jvalue> GetStaticValue() const override { return nullptr; }

  ErrorOr<JVariant> Evaluate(
      const EvaluationContext& evaluation_context) const override;

 private:
  // Tries to creates a reader for the identifier as instance field. Returns
  // false if no field was matched. This function supports following "this"
  // reference chain in inner classes.
  bool CreateInstanceFieldReader(
      ReadersFactory* readers_factory,
      FormatMessageModel* error_message);

  // Evaluates the identifier as a local variable.
  ErrorOr<JVariant> LocalVariableComputer(
      const EvaluationContext& evaluation_context) const;

  // Evaluates the identifier as implicit instance field.
  ErrorOr<JVariant> ImplicitInstanceFieldComputer(
      const EvaluationContext& evaluation_context) const;

  // Evaluates the identifier as a static variable of a class containing the
  // current evaluation point.
  ErrorOr<JVariant> StaticFieldComputer(
      const EvaluationContext& evaluation_context) const;

 private:
  // Name of the identifier (whether it is local variable or something else).
  std::string identifier_name_;

  // Local variable reader.
  std::unique_ptr<LocalVariableReader> variable_reader_;

  // Optional reader for instance field to handle the implicit field
  // reference (e.g. "myInt" that's actually "this.myInt"). In case of
  // an inner class this chain will follow inner classes references
  // (e.g. this$3.this$2.this$1.myField).
  std::vector<std::unique_ptr<InstanceFieldReader>> instance_fields_chain_;

  // Reader for static fields.
  std::unique_ptr<StaticFieldReader> static_field_reader_;

  // Pointer to a member function of this class to do the actual evaluation
  // based on whether it's local variable, implicit instance field, etc.
  ErrorOr<JVariant> (IdentifierEvaluator::*computer_)(
      const EvaluationContext&) const;

  // Statically computed resulting type of the expression. This is what
  // computer_ is supposed product.
  JSignature result_type_;

  DISALLOW_COPY_AND_ASSIGN(IdentifierEvaluator);
};


}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_IDENTIFIER_EVALUATOR_H_


