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

#include "expression_util.h"

#include <sstream>

#include "JavaExpressionCompiler.hpp"
#include "JavaExpressionLexer.hpp"
#include "JavaExpressionParser.hpp"
#include "expression_evaluator.h"
#include "java_expression.h"
#include "messages.h"
#include "model_util.h"
#include "readers_factory.h"

namespace devtools {
namespace cdbg {

static CompiledExpression EnsureDefaultErrorMessage(
    CompiledExpression compiled_expression) {
  if (compiled_expression.evaluator == nullptr) {
    if (compiled_expression.error_message.format.empty()) {
      compiled_expression.error_message = { GeneralExpressionError };
    }
  }

  return compiled_expression;
}

CompiledExpression CompileExpression(const std::string& string_expression,
                                     ReadersFactory* readers_factory) {
  if (string_expression.size() > kMaxExpressionLength) {
    LOG(WARNING) << "Expression can't be compiled because it is too long: "
                 << string_expression.size();
    return { nullptr, { ExpressionTooLong }, string_expression };
  }

  // Parse the expression.
  std::istringstream input_stream(string_expression);
  JavaExpressionLexer lexer(input_stream);
  JavaExpressionParser parser(lexer);
  parser.Init();

  parser.statement();
  if (parser.ActiveException()) {
    parser.reportError(parser.ActiveException()->getMessage());
  }

  if (parser.num_errors() > 0) {
    LOG(WARNING) << "Expression parsing failed" << std::endl
                 << "Input: " << string_expression << std::endl
                 << "Parser error: " << parser.errors()[0];
    return { nullptr, { ExpressionParserError }, string_expression };
  }

  // Transform ANTLR AST into "JavaExpression" tree.
  JavaExpressionCompiler compiler;
  compiler.Init();

  std::unique_ptr<JavaExpression> expression = compiler.Walk(parser.getAST());
  if (expression == nullptr) {
    LOG(WARNING) << "Tree walking on parsed expression failed" << std::endl
                 << "Input: " << string_expression << std::endl
                 << "AST: " << parser.getAST()->toStringTree();

    return { nullptr, compiler.error_message(), string_expression };
  }

  // Compile the expression.
  CompiledExpression compiled_expression = expression->CreateEvaluator();
  compiled_expression.expression = string_expression;
  if (compiled_expression.evaluator == nullptr) {
    LOG(WARNING) << "Expression not supported by the evaluator" << std::endl
                 << "Input: " << string_expression << std::endl
                 << "AST: " << parser.getAST()->toStringTree();

    return EnsureDefaultErrorMessage(std::move(compiled_expression));
  }

  if (!compiled_expression.evaluator->Compile(
        readers_factory,
        &compiled_expression.error_message)) {
    LOG(WARNING) << "Expression could not be compiled" << std::endl
                 << "Input: " << string_expression << std::endl
                 << "AST: " << parser.getAST()->toStringTree() << std::endl
                 << "Error message: " << compiled_expression.error_message;
    compiled_expression.evaluator = nullptr;

    return EnsureDefaultErrorMessage(std::move(compiled_expression));
  }

  VLOG(1) << "Expression compiled successfully" << std::endl
          << "Input: " << string_expression << std::endl
          << "AST: " << parser.getAST()->toStringTree();

  return compiled_expression;
}

}  // namespace cdbg
}  // namespace devtools



