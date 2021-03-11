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

#include "safe_caller_proxies.h"

#include <cstdint>

#include "class_metadata_reader.h"
#include "jni_proxy_object.h"
#include "safe_method_caller.h"
#include "type_util.h"

ABSL_DECLARE_FLAG(int32, safe_caller_max_array_elements);

namespace devtools {
namespace cdbg {

// Called just before Object.clone is invoked. Verifies that the source array
// is not too big.
//
// Method signature:
//     protected native Object clone();
MethodCallResult ObjectClonePre(
    SafeMethodCaller* caller,
    jobject instance,
    std::vector<JVariant>* arguments) {
  if (instance == nullptr) {
    // Bad argument to System.arraycopy. Let the rest of the code handle that.
    return MethodCallResult::Success(JVariant());
  }

  std::string signature = GetObjectClassSignature(instance);
  if (IsArrayObjectSignature(signature)) {
    jsize length = jni()->GetArrayLength(static_cast<jarray>(instance));
    if (length > absl::GetFlag(FLAGS_safe_caller_max_array_elements)) {
      return MethodCallResult::Error({
        MethodNotSafeNewArrayTooLarge,
        { caller->GetCurrentMethodName(), { std::to_string(length) } }
      });
    }
  }

  return MethodCallResult::Success(JVariant());
}


// Called just before System.arraycopy is invoked. Verifies that the code
// is not copying excessively large array blocks.
//
// Method signature:
//     public static void arraycopy(
//         Object src,
//         int srcPos,
//         Object dest,
//         int destPos,
//         int length);
MethodCallResult SystemArraycopyPre(
    SafeMethodCaller* caller,
    jobject source,
    std::vector<JVariant>* arguments) {
  jint length = 0;
  jobject dest = nullptr;
  if ((arguments->size() != 5) ||
      !(*arguments)[2].get<jobject>(&dest) ||
      !(*arguments)[4].get<jint>(&length)) {
    // Bad argument to System.arraycopy. Let the rest of the code handle that.
    return MethodCallResult::Success(JVariant());
  }

  if (length > absl::GetFlag(FLAGS_safe_caller_max_array_elements)) {
    return MethodCallResult::Error({
      MethodNotSafeCopyArrayTooLarge,
      { caller->GetCurrentMethodName(), { std::to_string(length) } }
    });
  }

  if (length > 0) {
    std::unique_ptr<FormatMessageModel> error_message =
        caller->IsArrayModifyAllowed(dest);
    if (error_message != nullptr) {
      return MethodCallResult::Error(std::move(*error_message));
    }
  }

  return MethodCallResult::Success(JVariant());
}


// Called just before String.format is invoked. Converts object arguments to
// string, so that "toString()" is safely interpreted.
//
// Supported method signatures:
//     public static String format(
//         String format,
//         Object... args);
//     public static String format(Locale l,
//         String format,
//         Object... args);
MethodCallResult StringFormatPre(
    SafeMethodCaller* caller,
    jobject unused_instance,
    std::vector<JVariant>* arguments) {
  jobject source;
  if (arguments->empty() ||
      !arguments->back().get<jobject>(&source) ||
      (source == nullptr)) {
    // Bad argument to String.format. Let the rest of the code handle that.
    return MethodCallResult::Success(JVariant());
  }

  JSignature signature = { JType::Object, GetObjectClassSignature(source) };
  if (!IsArrayObjectType(signature) ||
      (GetArrayElementJSignature(signature).type != JType::Object)) {
    // Bad argument to String.format. Let the rest of the code handle that.
    return MethodCallResult::Success(JVariant());
  }

  jsize size = jni()->GetArrayLength(static_cast<jobjectArray>(source));
  if (size > absl::GetFlag(FLAGS_safe_caller_max_array_elements)) {
    return MethodCallResult::Error({
      MethodNotSafeNewArrayTooLarge,
      { "java.lang.String.format", { std::to_string(size) } }
    });
  }

  JniLocalRef replacement(jni()->NewObjectArray(
      size,
      jniproxy::Object()->GetClass(),
      nullptr));
  for (int i = 0; i < size; ++i) {
    JniLocalRef ref(jni()->GetObjectArrayElement(
        static_cast<jobjectArray>(source),
        i));
    MethodCallResult rc;
    jobject replacement_element = ref.get();
    if (ref != nullptr) {
      std::string signature = GetObjectClassSignature(ref.get());
      if ((signature != "Ljava/lang/Boolean;") &&
          (signature != "Ljava/lang/Byte;") &&
          (signature != "Ljava/lang/Character;") &&
          (signature != "Ljava/lang/Double;") &&
          (signature != "Ljava/lang/Float;") &&
          (signature != "Ljava/lang/Integer;") &&
          (signature != "Ljava/lang/Long;") &&
          (signature != "Ljava/lang/Short;") &&
          (signature != "Ljava/lang/String;")) {
        ClassMetadataReader::Method to_string;
        to_string.class_signature = { JType::Object, "Ljava/lang/Object;" };
        to_string.name = "toString";
        to_string.signature = "()Ljava/lang/String;";

        rc = caller->InvokeInternal(
            false,
            to_string,
            ref.get(),
            {});
        if (rc.result_type() != MethodCallResult::Type::Success) {
          return rc;
        }

        if (!rc.return_value().get<jobject>(&replacement_element)) {
          return MethodCallResult::Error(INTERNAL_ERROR_MESSAGE);
        }
      }
    }

    jni()->SetObjectArrayElement(
        static_cast<jobjectArray>(replacement.get()),
        i,
        replacement_element);
  }

  arguments->back() = JVariant::LocalRef(std::move(replacement));

  return MethodCallResult::Success(JVariant());
}


}  // namespace cdbg
}  // namespace devtools
