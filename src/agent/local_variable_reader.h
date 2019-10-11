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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_LOCAL_VARIABLE_READER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_LOCAL_VARIABLE_READER_H_

#include <memory>

#include "common.h"
#include "model.h"
#include "type_util.h"

namespace devtools {
namespace cdbg {

struct EvaluationContext;

// Reads value of a local variable variable given the JVM context.
class LocalVariableReader {
 public:
  virtual ~LocalVariableReader() { }

  // Creates copy of this instance. The caller owns the returned value.
  virtual std::unique_ptr<LocalVariableReader> Clone() const = 0;

  // Returns true if this variable corresponds to a method argument or false
  // if it's just a regular local variable.
  virtual bool IsArgument() const = 0;

  // Gets the name of the variable (either name of local/static variable or
  // member variable name).
  virtual const std::string& GetName() const = 0;

  // Gets the type of the variable as it is known at compile time.
  virtual const JSignature& GetStaticType() const = 0;

  // Checks whether this local variable is defined at "location". If the local
  // variable is defined inside a lexical block, it will not be available
  // outside of that block. For example in the code snippet below, z will not:
  // be defined outside of the "if ..." line.
  //   void f(int x) {
  //     int y;
  //     if (x > 0) { int z = h(); y = z * 2; }
  //     ...
  //   }
  virtual bool IsDefinedAtLocation(jlocation location) const = 0;

  // Reads the value of a variable.
  //
  // If there is an error, false is returned and the error field is populated
  // with an error message.
  virtual bool ReadValue(
      const EvaluationContext& evaluation_context,
      JVariant* result,
      FormatMessageModel* error) const = 0;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_LOCAL_VARIABLE_READER_H_



