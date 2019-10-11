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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_LOG_DATA_COLLECTOR_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_LOG_DATA_COLLECTOR_H_

#include <functional>

#include "class_indexer.h"
#include "common.h"
#include "expression_util.h"
#include "method_caller.h"
#include "model.h"
#include "object_evaluator.h"
#include "type_util.h"

namespace devtools {
namespace cdbg {

// Evaluates watched expressions and formats the log message string for
// dynamic logs.
class LogDataCollector {
 public:
  LogDataCollector() {}

  // Evaluates the expressions to be included in the log message.
  void Collect(
      MethodCaller* method_caller,
      ObjectEvaluator* object_evaluator,
      const std::vector<CompiledExpression>& watches,
      jthread thread);

  // Formats the log message string.
  std::string Format(const BreakpointModel& breakpoint) const;

 private:
  // Evaluates a watched expression. Returns compilation error message if
  // the expression previously failed to compile.
  NamedJVariant EvaluateWatchedExpression(
      MethodCaller* method_caller,
      const CompiledExpression& watch,
      jthread thread) const;

 private:
  // Evaluated watched expressions. The string will contain one of:
  // 1. Actual result of expression (if primitive type or a string).
  // 2. Formatted error status either due to a failure to compile an expression
  //    or due to a runtime failure.
  // 3. Formatted object if an expression evaluates to an object. The
  //    formatting may either call "toString()" or print out all the object
  //    fields.
  std::vector<std::string> watch_results_;

  DISALLOW_COPY_AND_ASSIGN(LogDataCollector);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_LOG_DATA_COLLECTOR_H_
