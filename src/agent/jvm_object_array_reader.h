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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_OBJECT_ARRAY_READER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_OBJECT_ARRAY_READER_H_

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

// Reads object array element.
class JvmObjectArrayReader : public ArrayReader {
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

    JVariant result = JVariant::LocalRef(
        JniLocalRef(jni()->GetObjectArrayElement(
            static_cast<jobjectArray>(obj),
            index_value)));

    if (jni()->ExceptionCheck()) {
      return MethodCallResult::PendingJniException().format_exception();
    }

    result.change_ref_type(JVariant::ReferenceKind::Global);
    return std::move(result);
  }
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_OBJECT_ARRAY_READER_H_



