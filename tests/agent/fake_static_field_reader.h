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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_FAKE_STATIC_FIELD_READER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_FAKE_STATIC_FIELD_READER_H_

#include "gmock/gmock.h"
#include "src/agent/static_field_reader.h"

namespace devtools {
namespace cdbg {

// Implements "StaticFieldReader" interface exposing preset data. This class
// is intended for unit tests only. It correctly implements the "Clone" method,
// which is something gMock doesn't handle.
class FakeStaticFieldReader : public StaticFieldReader {
 public:
  FakeStaticFieldReader(const std::string& name, const JSignature& signature,
                        const JVariant& expected_value)
      : name_(name), signature_(signature), expected_value_(expected_value) {}

  static std::unique_ptr<StaticFieldReader> Create(
      const std::string& name, const JSignature& signature,
      const JVariant& expected_value) {
    return std::unique_ptr<StaticFieldReader>(
        new FakeStaticFieldReader(name, signature, expected_value));
  }

  void ReleaseRef() override {
  }

  std::unique_ptr<StaticFieldReader> Clone() const override {
    return Create(name_, signature_, expected_value_);
  }

  const std::string& GetName() const override { return name_; }

  const JSignature& GetStaticType() const override { return signature_; }

  bool ReadValue(JVariant* result, FormatMessageModel* error) const override {
    *result = JVariant(expected_value_);
    return true;
  }

 private:
  // Static field name.
  const std::string name_;

  // Field type.
  const JSignature signature_;

  // Expected field value (the object in which we read the value is ignored).
  JVariant expected_value_;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_FAKE_STATIC_FIELD_READER_H_

