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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_NANOJAVA_INTERNAL_ERROR_BUILDER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_NANOJAVA_INTERNAL_ERROR_BUILDER_H_

#include <sstream>

#include "common.h"
#include "method_call_result.h"

namespace devtools {
namespace cdbg {
namespace nanojava {

//
// Macro and helper method to generate an error message in the unlikely event
// of an internal error. The error details include source file name and line
// number, interpreter call stack and optional details.
//
// The internal error message will be actually displayed to the end user, so
// it should not contain too much gibberish. Class signatures should be
// converted to type names (i.e. "Ljava/lang/String;" -> "java.lang.String").
//
// Typical usage:
//     SET_INTERNAL_ERROR("expected $0, but got $1", expected, actual)
//

#define INTERNAL_ERROR_RESULT(format, ...) \
    BuildNanoJavaInternalError( \
        internal_error_provider(), \
        ShortFileName(__FILE__), \
        __LINE__, \
        format, \
        { __VA_ARGS__ })

#define SET_INTERNAL_ERROR(format, ...) \
    internal_error_provider()->SetResult( \
        INTERNAL_ERROR_RESULT(format, __VA_ARGS__))

// The provider of the data included in the internal error details.
class NanoJavaInternalErrorProvider {
 public:
  virtual ~NanoJavaInternalErrorProvider() {}

  // Gets the name of the currently executing method.
  virtual std::string method_name() const = 0;

  // Format call stack of the interpreted methods.
  virtual std::string FormatCallStack() const = 0;

  // Sets result of the method. This will stop the execution.
  virtual void SetResult(MethodCallResult result) = 0;
};


// Augments FormatMessageModel with additional details ("internal error"
// prefix, source file name and line number and the interpreter call stack).
inline MethodCallResult BuildNanoJavaInternalError(
    NanoJavaInternalErrorProvider* provider, std::string source_file_name,
    int line, std::string format, std::vector<std::string> parameters) {
  const int size = parameters.size();

  std::ostringstream ss;
  ss << "Internal error executing $" << size
     << " at $" << (size + 1) << ":$" << (size + 2)
     << ": " << format << ", call stack:\n$" << (size + 3);

  parameters.push_back(provider->method_name());
  parameters.push_back(std::move(source_file_name));
  parameters.push_back(std::to_string(line));
  parameters.push_back(provider->FormatCallStack());

  return MethodCallResult::Error({ ss.str(), std::move(parameters) });
}

}  // namespace nanojava
}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_NANOJAVA_INTERNAL_ERROR_BUILDER_H_
