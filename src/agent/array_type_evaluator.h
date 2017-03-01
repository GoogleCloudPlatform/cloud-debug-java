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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_ARRAY_TYPE_EVALUATOR_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_ARRAY_TYPE_EVALUATOR_H_

#include "common.h"
#include "instance_field_reader.h"
#include "jni_utils.h"
#include "jvariant.h"
#include "type_evaluator.h"

namespace devtools {
namespace cdbg {

static constexpr char kArrayLengthName[] = "length";

class ClassMetadataReader;
struct NamedJVariant;

// Captures content of Java native array.
template <typename TArrayType>
class ArrayTypeEvaluator : public TypeEvaluator {
 public:
  ArrayTypeEvaluator() {}

  string GetEvaluatorName() override;

  // "method_caller" is not used.
  void Evaluate(
      MethodCaller* method_caller,
      const ClassMetadataReader::Entry& class_metadata,
      jobject obj,
      std::vector<NamedJVariant>* members) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArrayTypeEvaluator);
};


template <>
void ArrayTypeEvaluator<jobject>::Evaluate(
    MethodCaller* method_caller,
    const ClassMetadataReader::Entry& class_metadata,
    jobject obj,
    std::vector<NamedJVariant>* members) {
  DCHECK(IsArrayObjectType(class_metadata.signature));

  WellKnownJClass element_well_known_jclass =
      WellKnownJClassFromSignature(
          GetArrayElementJSignature(class_metadata.signature));

  // Evaluate the array.
  const jsize array_len = jni()->GetArrayLength(static_cast<jarray>(obj));
  const int count =
      std::min<int>(kMaxCaptureObjectElements, array_len);

  const bool is_trimmed = count < array_len;

  // We reserve 1st element for array length, and last element for
  // status message of trimmed array. If array is not trimmed, we don't
  // need the message.
  *members = std::vector<NamedJVariant>(is_trimmed ? count + 2 : count + 1);

  (*members)[0].name = kArrayLengthName;
  (*members)[0].value = JVariant::Int(array_len);

  for (int i = 0; i < count; ++i) {
    JniLocalRef jitem(
        jni()->GetObjectArrayElement(static_cast<jobjectArray>(obj), i));

    (*members)[i + 1].name = FormatArrayIndexName(i);
    (*members)[i + 1].value.assign_new_ref(
        JVariant::ReferenceKind::Global,
        jitem.get());
    (*members)[i + 1].well_known_jclass = element_well_known_jclass;
  }

  // For trimmed array we reserved one extra space for status message
  if (is_trimmed) {
    (*members)[count + 1] = NamedJVariant::InfoStatus({
      CollectionNotAllItemsCaptured,
      { std::to_string(count) }
    });
  }
}


template <typename TArrayType>
void ArrayTypeEvaluator<TArrayType>::Evaluate(
    MethodCaller* method_caller,
    const ClassMetadataReader::Entry& class_metadata,
    jobject obj,
    std::vector<NamedJVariant>* members) {
  DCHECK(IsArrayObjectType(class_metadata.signature));

  const jsize array_len = jni()->GetArrayLength(static_cast<jarray>(obj));

  //
  // Note: the function must not block or make any calls to JNI in
  // between GetPrimitiveArrayCritical and ReleasePrimitiveArrayCritical.
  //

  if (array_len > 0) {
    const TArrayType* array_data = static_cast<TArrayType*>(
        jni()->GetPrimitiveArrayCritical(static_cast<jarray>(obj), nullptr));

    if (array_data != nullptr) {
      const int count =
          std::min<int>(kMaxCapturePrimitiveElements, array_len);

      const bool is_trimmed = count < array_len;

      // We reserve 1st element for array name, and last element for
      // status message of trimmed array. If array is not trimmed, we don't
      // need the message.
      *members = std::vector<NamedJVariant>(is_trimmed ? count + 2 : count + 1);

      for (int i = 0; i < count; ++i) {
        (*members)[i + 1].name = FormatArrayIndexName(i);
        (*members)[i + 1].value =
            JVariant::Primitive<TArrayType>(array_data[i]);
      }

      // For trimmed array we reserved one extra space for status message
      if (is_trimmed) {
        (*members)[count + 1] = NamedJVariant::InfoStatus({
          CollectionNotAllItemsCaptured,
          { std::to_string(count) }
        });
      }

      jni()->ReleasePrimitiveArrayCritical(
          static_cast<jarray>(obj),
          const_cast<TArrayType*>(array_data),
          0);
    }
  } else {
    *members = std::vector<NamedJVariant>(1);
  }

  (*members)[0].name = kArrayLengthName;
  (*members)[0].value = JVariant::Int(array_len);
}


template <>
inline string ArrayTypeEvaluator<jboolean>::GetEvaluatorName() {
  return "ArrayTypeEvaluator<jboolean>";
}


template <>
inline string ArrayTypeEvaluator<jchar>::GetEvaluatorName() {
  return "ArrayTypeEvaluator<jchar>";
}


template <>
inline string ArrayTypeEvaluator<jbyte>::GetEvaluatorName() {
  return "ArrayTypeEvaluator<jbyte>";
}


template <>
inline string ArrayTypeEvaluator<jshort>::GetEvaluatorName() {
  return "ArrayTypeEvaluator<jshort>";
}


template <>
inline string ArrayTypeEvaluator<jint>::GetEvaluatorName() {
  return "ArrayTypeEvaluator<jint>";
}


template <>
inline string ArrayTypeEvaluator<jlong>::GetEvaluatorName() {
  return "ArrayTypeEvaluator<jlong>";
}


template <>
inline string ArrayTypeEvaluator<jfloat>::GetEvaluatorName() {
  return "ArrayTypeEvaluator<jfloat>";
}


template <>
inline string ArrayTypeEvaluator<jdouble>::GetEvaluatorName() {
  return "ArrayTypeEvaluator<jdouble>";
}

template <>
inline string ArrayTypeEvaluator<jobject>::GetEvaluatorName() {
  return "ArrayTypeEvaluator<jobject>";
}

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_ARRAY_TYPE_EVALUATOR_H_

