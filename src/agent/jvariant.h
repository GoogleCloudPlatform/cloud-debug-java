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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVARIANT_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVARIANT_H_

#include <memory>

#include "common.h"
#include "jni_utils.h"

namespace devtools {
namespace cdbg {

// Enumerates basic data types in Java. Object covers everything else
// including arrays, strings and boxed types (like Boolean).
enum class JType : uint8 {
  Void = 0,
  Boolean,
  Byte,
  Char,
  Short,
  Int,
  Long,
  Float,
  Double,
  Object  // Reference to an object (local or global).
};

constexpr int TotalJTypes = static_cast<int>(JType::Object) + 1;

// Stores Java value of a particular type. JVariant owns the stored
// references. Those references are released in destructor or may be
// preemptively released with "ReleaseRef".
class JVariant {
 public:
  // Reference type to Java object.
  enum class ReferenceKind : uint8 {
    Local = JNILocalRefType,
    Global = JNIGlobalRefType,
    WeakGlobal = JNIWeakGlobalRefType,
    Borrowed = 9  // "JVariant" is not managing lifetime of the ref.
  };

  JVariant() {
    data_type_ = JType::Void;
    u_.l = nullptr;
  }

  JVariant(const JVariant& source);

  // Move constructor approved by c-style-arbiters in CL 80120287.
  JVariant(JVariant&& other)  // NOLINT
      : data_type_(other.data_type_),
        reference_type_(other.reference_type_),
        u_(other.u_) {
    other.data_type_ = JType::Void;
  }

  ~JVariant() {
    ReleaseRef();
  }

  static JVariant Boolean(jboolean boolean_value) {
    JVariant rc;
    rc.data_type_ = JType::Boolean;
    rc.u_.z = boolean_value;

    return rc;
  }

  static JVariant Byte(jbyte byte_value) {
    JVariant rc;
    rc.data_type_ = JType::Byte;
    rc.u_.b = byte_value;

    return rc;
  }

  static JVariant Char(jchar char_value) {
    JVariant rc;
    rc.data_type_ = JType::Char;
    rc.u_.c = char_value;

    return rc;
  }

  static JVariant Short(jshort short_value) {
    JVariant rc;
    rc.data_type_ = JType::Short;
    rc.u_.s = short_value;

    return rc;
  }

  static JVariant Int(jint int_value) {
    JVariant rc;
    rc.data_type_ = JType::Int;
    rc.u_.i = int_value;

    return rc;
  }

  static JVariant Long(jlong long_value) {
    JVariant rc;
    rc.data_type_ = JType::Long;
    rc.u_.j = long_value;

    return rc;
  }

  static JVariant Float(jfloat float_value) {
    JVariant rc;
    rc.data_type_ = JType::Float;
    rc.u_.f = float_value;

    return rc;
  }

  static JVariant Double(jdouble double_value) {
    JVariant rc;
    rc.data_type_ = JType::Double;
    rc.u_.d = double_value;

    return rc;
  }

  template <typename T>
  static JVariant Primitive(T value);

  static JVariant LocalRef(JniLocalRef ref) {
    JVariant rc;
    rc.data_type_ = JType::Object;
    rc.reference_type_ = ReferenceKind::Local;
    rc.u_.l = ref.release();

    return rc;
  }

  // TODO: remove after all JNI methods are wrapped to return
  // JniLocalRef.
  static JVariant LocalRef(jobject ref) {
    JVariant rc;
    rc.data_type_ = JType::Object;
    rc.reference_type_ = ReferenceKind::Local;
    rc.u_.l = ref;

    return rc;
  }

  static JVariant GlobalRef(jobject ref) {
    JVariant rc;
    rc.data_type_ = JType::Object;
    rc.reference_type_ = ReferenceKind::Global;
    rc.u_.l = ref;

    return rc;
  }

  static JVariant BorrowedRef(jobject ref) {
    JVariant rc;
    rc.data_type_ = JType::Object;
    rc.reference_type_ = ReferenceKind::Borrowed;
    rc.u_.l = ref;

    return rc;
  }

  static JVariant Null() {
    JVariant rc;
    rc.data_type_ = JType::Object;
    rc.reference_type_ = ReferenceKind::Global;
    rc.u_.l = nullptr;

    return rc;
  }

  // Move assignment operator approved by c-style-arbiters in CL 80120287.
  JVariant& operator=(JVariant&& other) {  // NOLINT
    ReleaseRef();

    data_type_ = other.data_type_;
    reference_type_ = other.reference_type_;
    u_ = other.u_;

    other.data_type_ = JType::Void;

    return *this;
  }

  // Swaps this instance of "JVariant" with "other".
  void swap(JVariant* other) {
    std::swap(data_type_, other->data_type_);
    std::swap(reference_type_, other->reference_type_);
    std::swap(u_, other->u_);
  }

  // Releases the Java local reference if this instance contains one.
  void ReleaseRef();

  // Gets the type of the data stored in this instance.
  JType type() const { return data_type_; }

  // Tries to retrieve the particular data type from "JVariant". Returns false
  // if this instance stores some other type rather than "T". If "T" is
  // jobject, the function returns whatever reference is stored in this
  // "JVariant".
  // Example:
  //   jfloat value = 0;
  //   if (jvar.get<jfloat>(&value))
  //     printf("%f", value);
  template <typename T>
  bool get(T* value) const;

  // Gets value in the format suitable for JNI method calls.
  jvalue get_jvalue() const { return u_; }

  // Returns true if this instance holds a non-null reference to Java object.
  // This function doesn't verify whether weak global reference has been
  // disposed.
  bool has_non_null_object() const;

  // Creates a new reference to the specified Java object. This instance will
  // be responsible to release the new reference. The caller is still
  // responsible to release "obj".
  void assign_new_ref(
      JVariant::ReferenceKind reference_type,
      jobject obj);

  // Attaches a reference to the specified Java object. This instance will be
  // responsible to release the reference using policy determined by
  // "reference_type".
  void attach_ref(
      JVariant::ReferenceKind reference_type,
      jobject obj);

  // Duplicates instance of "JVariant". If "source" is of Java object type, this
  // function will create a new reference of the specified reference type.
  void assign(
      ReferenceKind new_reference_type,
      const JVariant& source);

  // Changes the reference type to Java object. If this instance is of a
  // primitive type or the reference type is already as expected, this function
  // does nothing.
  void change_ref_type(ReferenceKind new_reference_type);

  // Prints the content of this instance to string for debugging purposes.
  std::string ToString(bool concise) const;

 private:
  JVariant& operator= (const JVariant& source) = delete;

 private:
  // Data type.
  JType data_type_;

  // Object reference type.
  JVariant::ReferenceKind reference_type_ { JVariant::ReferenceKind::Local };

  // Data storage.
  jvalue u_;
};

template <>
bool JVariant::get<jboolean>(jboolean* value) const;

template <>
bool JVariant::get<jbyte>(jbyte* value) const;

template <>
bool JVariant::get<jchar>(jchar* value) const;

template <>
bool JVariant::get<jshort>(jshort* value) const;

template <>
bool JVariant::get<jint>(jint* value) const;

template <>
bool JVariant::get<jlong>(jlong* value) const;

template <>
bool JVariant::get<jfloat>(jfloat* value) const;

template <>
bool JVariant::get<jdouble>(jdouble* value) const;

template <>
bool JVariant::get<jobject>(jobject* value) const;


template <>
inline JVariant JVariant::Primitive<jboolean>(jboolean value) {
  return JVariant::Boolean(value);
}


template <>
inline JVariant JVariant::Primitive<jbyte>(jbyte value) {
  return JVariant::Byte(value);
}


template <>
inline JVariant JVariant::Primitive<jchar>(jchar value) {
  return JVariant::Char(value);
}


template <>
inline JVariant JVariant::Primitive<jshort>(jshort value) {
  return JVariant::Short(value);
}


template <>
inline JVariant JVariant::Primitive<jint>(jint value) {
  return JVariant::Int(value);
}


template <>
inline JVariant JVariant::Primitive<jlong>(jlong value) {
  return JVariant::Long(value);
}


template <>
inline JVariant JVariant::Primitive<jfloat>(jfloat value) {
  return JVariant::Float(value);
}


template <>
inline JVariant JVariant::Primitive<jdouble>(jdouble value) {
  return JVariant::Double(value);
}


}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVARIANT_H_


