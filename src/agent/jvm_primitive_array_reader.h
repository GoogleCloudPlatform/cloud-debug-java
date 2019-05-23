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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_PRIMITIVE_ARRAY_READER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_PRIMITIVE_ARRAY_READER_H_

#include <memory>

#include "array_reader.h"
#include "common.h"
#include "jni_utils.h"
#include "jvariant.h"
#include "messages.h"
#include "method_call_result.h"
#include "model.h"
#include "model_util.h"

namespace devtools {
namespace cdbg {

// Reads primitive array element.
template <typename TElement>
class JvmPrimitiveArrayReader : public ArrayReader {
 public:
  ErrorOr<JVariant> ReadValue(
      const JVariant& source,
      const JVariant& index) const override {
    jobject obj = 0;
    if (!source.get<jobject>(&obj)) {
      return INTERNAL_ERROR_MESSAGE;
    }

    if (obj == nullptr) {
      return FormatMessageModel { NullPointerDereference };
    }

    jlong index_value = 0;
    if (!index.get<jlong>(&index_value)) {
      return INTERNAL_ERROR_MESSAGE;
    }

    TElement data = TElement();
    GetPrimitiveArrayRegion(obj, index_value, &data);

    if (jni()->ExceptionCheck()) {
      return MethodCallResult::PendingJniException().format_exception();
    }

    return JVariant::Primitive<TElement>(data);
  }

 private:
  static void GetPrimitiveArrayRegion(
      jobject obj,
      jlong index,
      TElement* data);
};


template <>
inline void JvmPrimitiveArrayReader<jboolean>::GetPrimitiveArrayRegion(
    jobject obj,
    jlong index,
    jboolean* data) {
  jni()->GetBooleanArrayRegion(static_cast<jbooleanArray>(obj), index, 1, data);
}


template <>
inline void JvmPrimitiveArrayReader<jbyte>::GetPrimitiveArrayRegion(
    jobject obj,
    jlong index,
    jbyte* data) {
  jni()->GetByteArrayRegion(static_cast<jbyteArray>(obj), index, 1, data);
}


template <>
inline void JvmPrimitiveArrayReader<jchar>::GetPrimitiveArrayRegion(
    jobject obj,
    jlong index,
    jchar* data) {
  jni()->GetCharArrayRegion(static_cast<jcharArray>(obj), index, 1, data);
}


template <>
inline void JvmPrimitiveArrayReader<jshort>::GetPrimitiveArrayRegion(
    jobject obj,
    jlong index,
    jshort* data) {
  jni()->GetShortArrayRegion(static_cast<jshortArray>(obj), index, 1, data);
}


template <>
inline void JvmPrimitiveArrayReader<jint>::GetPrimitiveArrayRegion(
    jobject obj,
    jlong index,
    jint* data) {
  jni()->GetIntArrayRegion(static_cast<jintArray>(obj), index, 1, data);
}


template <>
inline void JvmPrimitiveArrayReader<jlong>::GetPrimitiveArrayRegion(
    jobject obj,
    jlong index,
    jlong* data) {
  jni()->GetLongArrayRegion(static_cast<jlongArray>(obj), index, 1, data);
}


template <>
inline void JvmPrimitiveArrayReader<jfloat>::GetPrimitiveArrayRegion(
    jobject obj,
    jlong index,
    jfloat* data) {
  jni()->GetFloatArrayRegion(static_cast<jfloatArray>(obj), index, 1, data);
}


template <>
inline void JvmPrimitiveArrayReader<jdouble>::GetPrimitiveArrayRegion(
    jobject obj,
    jlong index,
    jdouble* data) {
  jni()->GetDoubleArrayRegion(static_cast<jdoubleArray>(obj), index, 1, data);
}


}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_PRIMITIVE_ARRAY_READER_H_



