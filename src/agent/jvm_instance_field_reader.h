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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_INSTANCE_FIELD_READER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_INSTANCE_FIELD_READER_H_

#include "common.h"
#include "instance_field_reader.h"

namespace devtools {
namespace cdbg {

// Reads specific instance field from Java object.
class JvmInstanceFieldReader : public InstanceFieldReader {
 public:
  // Construct a field reader for the given field_id.
  //
  // If is_read_error == true, then read_error will be returned
  // on any calls to ReadValue()
  JvmInstanceFieldReader(const std::string& name, jfieldID field_id,
                         const JSignature& signature, bool is_read_error,
                         const FormatMessageModel& read_error);

  JvmInstanceFieldReader(
      const JvmInstanceFieldReader& jvm_instance_field_reader);

  std::unique_ptr<InstanceFieldReader> Clone() const override {
    return std::unique_ptr<InstanceFieldReader>(
        new JvmInstanceFieldReader(*this));
  }

  const std::string& GetName() const override { return name_; }

  const JSignature& GetStaticType() const override { return signature_; }

  bool ReadValue(
      jobject source_object,
      JVariant* result,
      FormatMessageModel* error) const override;

 private:
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

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_INSTANCE_FIELD_READER_H_
