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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_STATIC_FIELD_READER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_STATIC_FIELD_READER_H_

#include "common.h"
#include "static_field_reader.h"

namespace devtools {
namespace cdbg {

// Reads specific static field from Java object.
class JvmStaticFieldReader : public StaticFieldReader {
 public:
  // Construct a static field reader for the given field_id.
  //
  // If is_read_error == true, then read_error will be returned on any calls to
  // ReadValue().
  JvmStaticFieldReader(jclass cls, const std::string& name, jfieldID field_id,
                       const JSignature& signature, bool is_read_error,
                       const FormatMessageModel& read_error);

  JvmStaticFieldReader(
      const JvmStaticFieldReader& jvm_static_field_reader);

  ~JvmStaticFieldReader() override;

  void ReleaseRef() override;

  std::unique_ptr<StaticFieldReader> Clone() const override;

  const std::string& GetName() const override { return name_; }

  const JSignature& GetStaticType() const override { return signature_; }

  bool ReadValue(JVariant* result, FormatMessageModel* error) const override;

 private:
  // Global reference to Java class object to which the static field belongs.
  jclass cls_;

  // Name of the member variable.
  const std::string name_;

  // Member variable type.
  const JSignature signature_;

  // JVMTI specific field ID. The value of "jfieldID" remains valid as long as
  // the class containing this field is loaded.
  const jfieldID field_id_;

  // If is_read_error_ is true, read_error_ is returned whenever ReadValue is
  // called.
  const bool is_read_error_;
  const FormatMessageModel read_error_;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_STATIC_FIELD_READER_H_
