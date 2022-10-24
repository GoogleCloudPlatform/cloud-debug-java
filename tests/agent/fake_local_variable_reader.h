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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_FAKE_LOCAL_VARIABLE_READER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_FAKE_LOCAL_VARIABLE_READER_H_

#include "gmock/gmock.h"

#include "src/agent/jvariant.h"
#include "src/agent/local_variable_reader.h"

namespace devtools {
namespace cdbg {

// Implements "LocalVariableReader" interface exposing preset data. This class
// is intended for unit tests only.
class FakeLocalVariableReader : public LocalVariableReader {
 public:
  FakeLocalVariableReader(bool is_argument, const std::string& name,
                          const JSignature& signature,
                          const JVariant& expected_value)
      : is_argument_(is_argument),
        name_(name),
        signature_(signature),
        expected_value_(expected_value) {}

  static std::unique_ptr<LocalVariableReader> CreateArgument(
      const std::string& name, const JSignature& signature,
      const JVariant& expected_value) {
    return std::unique_ptr<LocalVariableReader>(
        new FakeLocalVariableReader(
            true,
            name,
            signature,
            expected_value));
  }

  static std::unique_ptr<LocalVariableReader> CreateLocal(
      const std::string& name, const JSignature& signature,
      const JVariant& expected_value) {
    return std::unique_ptr<LocalVariableReader>(
        new FakeLocalVariableReader(
            false,
            name,
            signature,
            expected_value));
  }

  std::unique_ptr<LocalVariableReader> Clone() const override {
    if (is_argument_) {
      return CreateArgument(name_, signature_, expected_value_);
    } else {
      return CreateLocal(name_, signature_, expected_value_);
    }
  }

  bool IsArgument() const override { return is_argument_; }

  const std::string& GetName() const override { return name_; }

  const JSignature& GetStaticType() const override { return signature_; }

  bool IsDefinedAtLocation(jlocation location) const override {
    return true;
  }

  bool ReadValue(
      const EvaluationContext& evaluation_context,
      JVariant* result,
      FormatMessageModel* error) const override {
    *result = JVariant(expected_value_);
    return true;
  }

 private:
  // Distinguishes between local variable and a method argument.
  bool is_argument_;

  // Instance field name.
  std::string name_;

  // Field type.
  JSignature signature_;

  // Expected field value (the object in which we read the value is ignored).
  JVariant expected_value_;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_FAKE_LOCAL_VARIABLE_READER_H_

