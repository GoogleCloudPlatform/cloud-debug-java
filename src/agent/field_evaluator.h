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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_FIELD_EVALUATOR_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_FIELD_EVALUATOR_H_

#include "common.h"
#include "expression_evaluator.h"

namespace devtools {
namespace cdbg {

class InstanceFieldReader;
class StaticFieldReader;

// Evaluates class fields (either instance or static).
class FieldEvaluator : public ExpressionEvaluator {
 public:
  // Class constructor for "field" reader. It can handle two cases:
  // 1. Instance field of an object computed by "instance_source". The
  // "possible_class_name" is ignored in this case.
  // 2. Static variable of a "possible_class_name" class (if specified). The
  // name should be fully qualified (e.g. "com.my.Green"). "instance_source"
  // is ignored in this case.
  FieldEvaluator(std::unique_ptr<ExpressionEvaluator> instance_source,
                 std::string identifier_name, std::string possible_class_name,
                 std::string field_name);

  ~FieldEvaluator() override;

  bool Compile(
      ReadersFactory* readers_factory,
      FormatMessageModel* error_message) override;

  const JSignature& GetStaticType() const override { return result_type_; }

  Nullable<jvalue> GetStaticValue() const override { return nullptr; }

  ErrorOr<JVariant> Evaluate(
      const EvaluationContext& evaluation_context) const override;

 private:
  // Tries to compile the subexpression as a reader of instance field.
  bool CompileInstanceField(
      ReadersFactory* readers_factory,
      FormatMessageModel* error_message);

  // Tries to compile the subexpression as a reader of a static field.
  bool CompileStaticField(
      ReadersFactory* readers_factory,
      FormatMessageModel* error_message);

  // Evaluation method when the expression refers to instance field.
  ErrorOr<JVariant> InstanceFieldComputer(
      const EvaluationContext& evaluation_context) const;

  // Evaluates length of primitive or object array.
  ErrorOr<JVariant> ArrayLengthComputer(
      const EvaluationContext& evaluation_context) const;

  // Evaluation method when the expression refers to a static field.
  ErrorOr<JVariant> StaticFieldComputer(
      const EvaluationContext& evaluation_context) const;

 private:
  // Expression computing the source object to read field from.
  std::unique_ptr<ExpressionEvaluator> instance_source_;

  // Fully qualified identifier name we are trying to interpret. This should
  // be "possible_class_name_.identifier_name".
  const std::string identifier_name_;

  // Fully qualified class name to try to interpret "field_name_" as static.
  const std::string possible_class_name_;

  // Name of the instance field to read.
  const std::string field_name_;

  // Reader for instance fields. In case of an inner class this chain will
  // follow inner classes references (e.g. this$3.this$2.this$1.myField).
  std::vector<std::unique_ptr<InstanceFieldReader>> instance_fields_chain_;

  // Reader for a static field.
  std::unique_ptr<StaticFieldReader> static_field_reader_;

  // Statically computed resulting type of the expression. This is what
  // computer_ is supposed product.
  JSignature result_type_;

  // Pointer to a member function of this class to do the actual evaluation.
  ErrorOr<JVariant> (FieldEvaluator::*computer_)(
      const EvaluationContext&) const;

  DISALLOW_COPY_AND_ASSIGN(FieldEvaluator);
};


// Helper function to create a chain of instance field readers supporting
// inner classes. Returns empty vector if no field was matched.
std::vector<std::unique_ptr<InstanceFieldReader>>
CreateInstanceFieldReadersChain(ReadersFactory* readers_factory,
                                const std::string& class_signature,
                                const std::string& field_name,
                                FormatMessageModel* error_message);

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_FIELD_EVALUATOR_H_


