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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_LOCAL_VARIABLE_READER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_LOCAL_VARIABLE_READER_H_

#include "common.h"
#include "local_variable_reader.h"

namespace devtools {
namespace cdbg {

class JVariant;

// This class may be released from CompiledMethodUnload. In this case
// "JNIEnv*" is not going to be available. Therefore this structure must not
// contain anything that requires "JNIEnv*" in destructor (e.g. "JVariant").
class JvmLocalVariableReader : public LocalVariableReader {
 public:
  // Constructs local variable reader from the appropriate JVMTI structure. If
  // "entry.length" is -1, the local variable is assumed to be available in all
  // function locations.
  //
  // If is_read_error == true, then read_error will be returned on any calls to
  // ReadValue().
  JvmLocalVariableReader(
      const jvmtiLocalVariableEntry& entry,
      bool is_argument,
      bool is_read_error,
      const FormatMessageModel& read_error);

  JvmLocalVariableReader(const JvmLocalVariableReader& source);

  std::unique_ptr<LocalVariableReader> Clone() const override;

  bool IsArgument() const override { return is_argument_; }

  const std::string& GetName() const override { return name_; }

  const JSignature& GetStaticType() const override { return signature_; }

  bool ReadValue(
      const EvaluationContext& evaluation_context,
      JVariant* result,
      FormatMessageModel* error) const override;

  bool IsDefinedAtLocation(jlocation location) const override;

 private:
  // Distinguishes between local variable and a method argument.
  const bool is_argument_;

  // Name of the local variable.
  const std::string name_;

  // Compile-time type of the local variable.
  const JSignature signature_;

  // Code location where the local variable is first valid.
  const jlocation start_location_;

  // The length of the valid section for this local variable. The last code
  // array index where the local variable is valid is
  // "start_location_ + section_length_".
  const jint section_length_;

  // Local variable slot (runtime identifier of the local variable).
  const jint slot_;

  // If is_read_error_ is true, read_error_ is returned whenever ReadValue is
  // called.
  const bool is_read_error_;
  const FormatMessageModel read_error_;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_LOCAL_VARIABLE_READER_H_
