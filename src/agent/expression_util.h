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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_EXPRESSION_UTIL_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_EXPRESSION_UTIL_H_

#include <memory>

#include "common.h"
#include "model.h"

namespace devtools {
namespace cdbg {

class ExpressionEvaluator;
class ReadersFactory;

// Some limit on expression length to prevent DoS inadvertently caused by
// expressions that take too much time and memory to compile and evaluate.
constexpr int kMaxExpressionLength = 2048;

// Holds either the compiled expression that can be executed or human
// readable error message describing why the expression could not be
// compiled.
struct CompiledExpression {
  // Compiled artifact that can compute the value of the expression. If
  // expression could not be compiled, "evaluator" will be set to null.
  std::unique_ptr<ExpressionEvaluator> evaluator;

  // Error message explaining why the expression could not be compiled. Only
  // relevant if "evaluator" is null.
  FormatMessageModel error_message;

  // Original expression text.
  std::string expression;
};

// Shortcut method to tokenize, parse, tree-walk and compile the specified
// expression. Returns nullptr if any error occures (syntactically or
// semantically incorrect expression). In such cases, "error_message" is
// populated with a human readable parameterized description of why the
// expression could not be compiled.
CompiledExpression CompileExpression(const std::string& string_expression,
                                     ReadersFactory* readers_factory);

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_EXPRESSION_UTIL_H_


