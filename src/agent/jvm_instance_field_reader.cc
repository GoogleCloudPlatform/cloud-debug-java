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

#include "jvm_instance_field_reader.h"

#include "jvariant.h"

namespace devtools {
namespace cdbg {

JvmInstanceFieldReader::JvmInstanceFieldReader(
    const string& name,
    jfieldID field_id,
    const JSignature& signature)
    : name_(name),
      signature_(signature),
      field_id_(field_id) {
}


JvmInstanceFieldReader::JvmInstanceFieldReader(
    const JvmInstanceFieldReader& jvm_instance_field_reader)
    : name_(jvm_instance_field_reader.name_),
      signature_(jvm_instance_field_reader.signature_),
      field_id_(jvm_instance_field_reader.field_id_) {
}


bool JvmInstanceFieldReader::ReadValue(
    jobject source_object,
    JVariant* result) const {
  switch (signature_.type) {
    case JType::Void:
      LOG(ERROR) << "'void' type is unexpected";
      return false;

    case JType::Boolean:
      *result =
          JVariant::Boolean(jni()->GetBooleanField(source_object, field_id_));
      return true;

    case JType::Char:
      *result =
          JVariant::Char(jni()->GetCharField(source_object, field_id_));
      return true;

    case JType::Byte:
      *result =
          JVariant::Byte(jni()->GetByteField(source_object, field_id_));
      return true;

    case JType::Short:
      *result =
          JVariant::Short(jni()->GetShortField(source_object, field_id_));
      return true;

    case JType::Int:
      *result =
          JVariant::Int(jni()->GetIntField(source_object, field_id_));
      return true;

    case JType::Long:
      *result =
          JVariant::Long(jni()->GetLongField(source_object, field_id_));
      return true;

    case JType::Float:
      *result =
          JVariant::Float(jni()->GetFloatField(source_object, field_id_));
      return true;

    case JType::Double:
      *result =
          JVariant::Double(jni()->GetDoubleField(source_object, field_id_));
      return true;

    case JType::Object: {
      result->attach_ref(
          JVariant::ReferenceKind::Local,
          jni()->GetObjectField(source_object, field_id_));
      return true;
    }
  }

  return false;
}

}  // namespace cdbg
}  // namespace devtools


