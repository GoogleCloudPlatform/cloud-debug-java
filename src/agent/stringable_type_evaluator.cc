/**
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "stringable_type_evaluator.h"

#include "jni_proxy_bigdecimal.h"
#include "jni_proxy_biginteger.h"

namespace devtools {
namespace cdbg {

StringableTypeEvaluator::StringableTypeEvaluator()
    : to_string_method_(
        InstanceMethod(
            "Ljava/lang/Object;",
            "toString",
            "()Ljava/lang/String;")) {
}

bool StringableTypeEvaluator::IsSupported(jclass cls) const {
  if (cls == nullptr) {
    return false;
  }

  std::vector<jclass> stringable_classes = {
    jniproxy::BigDecimal()->GetClass(),
    jniproxy::BigInteger()->GetClass(),
  };

  for (auto& stringable_cls : stringable_classes) {
    if (jni()->IsAssignableFrom(cls, stringable_cls)) {
      return true;
    }
  }

  return false;
}

void StringableTypeEvaluator::Evaluate(
    MethodCaller* method_caller,
    const ClassMetadataReader::Entry& class_metadata,
    jobject obj,
    bool is_watch_expression,
    std::vector<NamedJVariant>* members) {

  members->clear();

  ErrorOr<JVariant> result = method_caller->Invoke(
      to_string_method_,
      JVariant::BorrowedRef(obj),
      {});

  if (result.is_error()) {
    members->push_back(NamedJVariant::ErrorStatus(result.error_message()));
    return;
  }

  NamedJVariant item;
  item.name = "toString";
  item.value = ErrorOr<JVariant>::detach_value(std::move(result));
  item.value.change_ref_type(JVariant::ReferenceKind::Global);

  members->push_back(std::move(item));
}

}  // namespace cdbg
}  // namespace devtools
