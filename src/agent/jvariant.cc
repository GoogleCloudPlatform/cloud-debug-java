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

#include "jvariant.h"

#include <float.h>

#include <cstdint>
#include <iostream>  // NOLINT
#include <sstream>

namespace devtools {
namespace cdbg {

static_assert(sizeof(JVariant) <= sizeof(uint64_t) * 2, "size_of_JVariant");

// Number of floating point digits to print.
constexpr int kFloatPrecision = FLT_DIG;
constexpr int kDoublePrecision = 10;

JVariant::JVariant(const JVariant& source) {
  data_type_ = JType::Void;

  if ((source.data_type_ != JType::Object) ||
      (source.u_.l == nullptr) ||
      (source.reference_type_ == ReferenceKind::Borrowed)) {
    data_type_ = source.data_type_;
    reference_type_ = source.reference_type_;
    u_ = source.u_;
  } else {
    assign_new_ref(source.reference_type_, source.u_.l);
  }
}


void JVariant::ReleaseRef() {
  if ((data_type_ == JType::Object) && (u_.l != nullptr)) {
    switch (reference_type_) {
      case ReferenceKind::Local:
        jni()->DeleteLocalRef(u_.l);
        break;

      case ReferenceKind::Global:
        jni()->DeleteGlobalRef(u_.l);
        break;

      case ReferenceKind::WeakGlobal:
        jni()->DeleteWeakGlobalRef(u_.l);
        break;

      case ReferenceKind::Borrowed:
        break;
    }

    u_.l = nullptr;
  }

  data_type_ = JType::Void;
}


template <>
bool JVariant::get<jboolean>(jboolean* value) const {
  if (data_type_ == JType::Boolean) {
    *value = u_.z;
    return true;
  }

  return false;
}


template <>
bool JVariant::get<jbyte>(jbyte* value) const {
  if (data_type_ == JType::Byte) {
    *value = u_.b;
    return true;
  }

  return false;
}


template <>
bool JVariant::get<jchar>(jchar* value) const {
  if (data_type_ == JType::Char) {
    *value = u_.c;
    return true;
  }

  return false;
}


template <>
bool JVariant::get<jshort>(jshort* value) const {
  if (data_type_ == JType::Short) {
    *value = u_.s;
    return true;
  }

  return false;
}


template <>
bool JVariant::get<jint>(jint* value) const {
  if (data_type_ == JType::Int) {
    *value = u_.i;
    return true;
  }

  return false;
}


template <>
bool JVariant::get<jlong>(jlong* value) const {
  if (data_type_ == JType::Long) {
    *value = u_.j;
    return true;
  }

  return false;
}


template <>
bool JVariant::get<jfloat>(jfloat* value) const {
  if (data_type_ == JType::Float) {
    *value = u_.f;
    return true;
  }

  return false;
}


template <>
bool JVariant::get<jdouble>(jdouble* value) const {
  if (data_type_ == JType::Double) {
    *value = u_.d;
    return true;
  }

  return false;
}


template <>
bool JVariant::get<jobject>(jobject* value) const {
  if (data_type_ == JType::Object) {
    *value = u_.l;
    return true;
  }

  *value = nullptr;
  return false;
}


bool JVariant::has_non_null_object() const {
  return (data_type_ == JType::Object) && (u_.l != nullptr);
}


void JVariant::assign_new_ref(ReferenceKind reference_type, jobject obj) {
  // Create new reference to Java object.
  jobject new_ref = nullptr;
  if (obj != nullptr) {
    switch (reference_type) {
      case ReferenceKind::Local:
        new_ref = jni()->NewLocalRef(obj);
        break;

      case ReferenceKind::Global:
        new_ref = jni()->NewGlobalRef(obj);
        break;

      case ReferenceKind::WeakGlobal:
        new_ref = jni()->NewWeakGlobalRef(obj);
        break;

      case ReferenceKind::Borrowed:
        DCHECK(false) << "Borrowed references not allowed in assign_new_ref";
        break;
    }
  }

  // Attach the new reference.
  ReleaseRef();

  data_type_ = JType::Object;
  reference_type_ = reference_type;
  u_.l = new_ref;
}


void JVariant::attach_ref(ReferenceKind reference_type, jobject obj) {
  DCHECK((obj == nullptr) ||
         (jni()->GetObjectRefType(obj) == static_cast<int>(reference_type)))
      << "reference_type does not match the actual object reference type";

  ReleaseRef();

  data_type_ = JType::Object;
  reference_type_ = reference_type;
  u_.l = obj;
}


void JVariant::assign(
    ReferenceKind new_reference_type,
    const JVariant& source) {
  if ((source.data_type_ != JType::Object) || (source.u_.l == nullptr)) {
    ReleaseRef();

    data_type_ = source.data_type_;
    reference_type_ = new_reference_type;
    u_ = source.u_;
  } else {
    assign_new_ref(new_reference_type, source.u_.l);
  }
}


void JVariant::change_ref_type(ReferenceKind new_reference_type) {
  if ((data_type_ == JType::Object) &&
      (reference_type_ != new_reference_type)) {
    if (u_.l == nullptr) {
      reference_type_ = new_reference_type;
    } else {
      JVariant new_variant;
      new_variant.assign_new_ref(new_reference_type, u_.l);

      swap(&new_variant);

      new_variant.ReleaseRef();
    }
  }
}

std::string JVariant::ToString(bool concise) const {
  // Long formatting includes the type of the value. This option is only used
  // in unit tests, so we don't need to optimize it. The concise form on the
  // other hand is called a lot in product code and needs to be as optimal as
  // possible.

  if (!concise) {
    switch (data_type_) {
      case JType::Void:
        return "<void>";

      case JType::Boolean:
        return "<boolean>" + ToString(true);

      case JType::Byte:
        return "<byte>" + ToString(true);

      case JType::Char:
        return "<char>" + ToString(true);

      case JType::Short:
        return "<short>" + ToString(true);

      case JType::Int:
        return "<int>" + ToString(true);

      case JType::Long:
        return "<long>" + ToString(true);

      case JType::Float:
        return "<float>" + ToString(true);

      case JType::Double:
        return "<double>" + ToString(true);

      case JType::Object:
        return ToString(true);
    }

    return ToString(true);
  }

  std::ostringstream ss;
  switch (data_type_) {
    case JType::Void:
      return "<void>";

    case JType::Boolean:
      return (u_.z) ? "true" : "false";

    case JType::Byte:
      return std::to_string(u_.b);

    case JType::Char:
      return std::to_string(u_.c);

    case JType::Short:
      return std::to_string(u_.s);

    case JType::Int:
      return std::to_string(u_.i);

    case JType::Long:
      return std::to_string(u_.j);

    case JType::Float: {
      char str[25];
      snprintf(
          str,
          arraysize(str), "%.*g",
          kFloatPrecision,
          static_cast<double>(u_.f));
      return str;
    }

    case JType::Double: {
      char str[25];
      snprintf(
          str,
          arraysize(str),
          "%.*g",
          kDoublePrecision,
          u_.d);
      return str;
    }

    case JType::Object:
      return (u_.l == nullptr) ? "null" : "<Object>";
  }

  return std::string();
}

}  // namespace cdbg
}  // namespace devtools
