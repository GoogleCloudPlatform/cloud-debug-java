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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_METHOD_CALL_EVALUATOR_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_METHOD_CALL_EVALUATOR_H_

#include "class_metadata_reader.h"
#include "common.h"
#include "expression_evaluator.h"

namespace devtools {
namespace cdbg {

class LocalVariableReader;
class MethodCaller;

// Invokes methods specified in expressions.
class MethodCallEvaluator : public ExpressionEvaluator {
 public:
  MethodCallEvaluator(
      std::string method_name,
      std::unique_ptr<ExpressionEvaluator> instance_source,
      std::string possible_class_name,
      std::vector<std::unique_ptr<ExpressionEvaluator>> arguments);

  ~MethodCallEvaluator() override;

  bool Compile(
      ReadersFactory* readers_factory,
      FormatMessageModel* error_message) override;

  const JSignature& GetStaticType() const override { return return_type_; }

  Nullable<jvalue> GetStaticValue() const override { return nullptr; }

  ErrorOr<JVariant> Evaluate(
      const EvaluationContext& evaluation_context) const override;

 private:
  // Gets the list of potential methods that the expression might be trying to
  // invoke. These are all the overloaded static/instance methods with name of
  // "method_name_". "MatchMethods" looks for methods with signature matching
  // "arguments_". If none were found, returns "matched = false". If more than
  // one match was found returns "matched = true" with an appropriate error
  // message. Otherwise if a single match was found, completes the compilation
  // and returns "matched = true" with empty error message.
  void MatchMethods(
      ReadersFactory* readers_factory,
      const std::vector<ClassMetadataReader::Method>& candidate_methods,
      bool* matched,
      FormatMessageModel* error_message);

  // Checks whether the signature of "candidate_method" matches expressions
  // in "arguments_".
  bool MatchMethod(
      ReadersFactory* readers_factory,
      const ClassMetadataReader::Method& candidate_method);

  // Matches single method argument. Returns converter if successful.
  bool MatchArgument(
      ReadersFactory* readers_factory,
      const JSignature& expected_signature,
      const ExpressionEvaluator& evaluated_signature);

  // Tries to compile evaluation of a method on the object returned by
  // prior evaluation.
  void MatchInstanceSourceMethod(
      ReadersFactory* readers_factory,
      bool* matched,
      FormatMessageModel* error_message);

  // Tries to compile evaluation of a static method invoked on explicitly
  // specified class ("possible_class_name_"). We support:
  // 1. Fully qualified names (e.g. com.myprod.MyClass.myMethod).
  // 2. Classes in lava.lang namespace (e.g. Integer.valueOf).
  // 3. Names relative to the current scope (e.g. OtherClass.myMethod
  //    or OtherClass.StaticClass.myMethod).
  void MatchExplicitStaticMethod(
      ReadersFactory* readers_factory,
      bool* matched,
      FormatMessageModel* error_message);

  // Obtains the source object for method call evaluation if we are calling
  // an instance method. Does nothing if calling static method.
  ErrorOr<JVariant> EvaluateSourceObject(
      const EvaluationContext& evaluation_context) const;

 private:
  // Method name (whether it's an instance method or a static method).
  const std::string method_name_;

  // Source object on which the instance method is invoked. Ignored if the
  // call turns out to be to a static method.
  std::unique_ptr<ExpressionEvaluator> instance_source_;

  // Fully qualified class name to try to interpret "method_name_" as a static
  // method.
  const std::string possible_class_name_;

  // Reader for local instance object (i.e. "this") for implicit instance
  // method calls (example: "1+getSomething()", where "getSomething" is an
  // instance method).
  std::unique_ptr<LocalVariableReader> local_instance_reader_;

  // Arguments to the method call.
  std::vector<std::unique_ptr<ExpressionEvaluator>> arguments_;

  // Signature of the invoked method (e.g. "(IZ)Ljava/lang/Object;"). This
  // class selects the best match among all overloaded methods.
  ClassMetadataReader::Method method_;

  // Return value of the method.
  JSignature return_type_;

  DISALLOW_COPY_AND_ASSIGN(MethodCallEvaluator);
};


}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_METHOD_CALL_EVALUATOR_H_


