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

#include "jvm_static_field_reader.h"

#include "jvariant.h"

namespace devtools {
namespace cdbg {

JvmStaticFieldReader::JvmStaticFieldReader(
    jclass cls,
    const string& name,
    jfieldID field_id,
    const JSignature& signature)
    : cls_(static_cast<jclass>(jni()->NewGlobalRef(cls))),
      name_(name),
      signature_(signature),
      field_id_(field_id) {
}


JvmStaticFieldReader::JvmStaticFieldReader(
    const JvmStaticFieldReader& jvm_static_field_reader)
    : cls_(static_cast<jclass>(
          jni()->NewGlobalRef(jvm_static_field_reader.cls_))),
      name_(jvm_static_field_reader.name_),
      signature_(jvm_static_field_reader.signature_),
      field_id_(jvm_static_field_reader.field_id_) {
}


JvmStaticFieldReader::~JvmStaticFieldReader() {
  ReleaseRef();
}


void JvmStaticFieldReader::ReleaseRef() {
  if (cls_ != nullptr) {
    jni()->DeleteGlobalRef(cls_);
    cls_ = nullptr;
  }
}


std::unique_ptr<StaticFieldReader> JvmStaticFieldReader::Clone() const {
  return std::unique_ptr<StaticFieldReader>(
      new JvmStaticFieldReader(*this));
}


bool JvmStaticFieldReader::ReadValue(JVariant* result) const {
  if (cls_ == nullptr) {
    LOG(WARNING) << "Java class not available to read static field";
    return false;
  }

  switch (signature_.type) {
    case JType::Void:
      LOG(ERROR) << "'void' type is unexpected";
      return false;

    case JType::Boolean:
      *result =
          JVariant::Boolean(jni()->GetStaticBooleanField(cls_, field_id_));
      return true;

    case JType::Char:
      *result =
          JVariant::Char(jni()->GetStaticCharField(cls_, field_id_));
      return true;

    case JType::Byte:
      *result =
          JVariant::Byte(jni()->GetStaticByteField(cls_, field_id_));
      return true;

    case JType::Short:
      *result =
          JVariant::Short(jni()->GetStaticShortField(cls_, field_id_));
      return true;

    case JType::Int:
      *result =
          JVariant::Int(jni()->GetStaticIntField(cls_, field_id_));
      return true;

    case JType::Long:
      *result =
          JVariant::Long(jni()->GetStaticLongField(cls_, field_id_));
      return true;

    case JType::Float:
      *result =
          JVariant::Float(jni()->GetStaticFloatField(cls_, field_id_));
      return true;

    case JType::Double:
      *result =
          JVariant::Double(jni()->GetStaticDoubleField(cls_, field_id_));
      return true;

    case JType::Object: {
      result->attach_ref(
          JVariant::ReferenceKind::Local,
          jni()->GetStaticObjectField(cls_, field_id_));
      return true;
    }
  }

  return false;
}

}  // namespace cdbg
}  // namespace devtools


