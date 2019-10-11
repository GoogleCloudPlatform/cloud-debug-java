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

#include "map_entry_type_evaluator.h"

#include "jni_proxy_ju_map_entry.h"
#include "jvmti_buffer.h"
#include "messages.h"
#include "model.h"
#include "model_util.h"

namespace devtools {
namespace cdbg {


MapEntryTypeEvaluator::MapEntryTypeEvaluator()
    : map_entry_get_key_(InstanceMethod(
          "Ljava/util/Map$Entry;",
          "getKey",
          "()Ljava/lang/Object;")),
      map_entry_get_value_(InstanceMethod(
          "Ljava/util/Map$Entry;",
          "getValue",
          "()Ljava/lang/Object;")) {
}


bool MapEntryTypeEvaluator::IsMapEntry(jclass cls) const {
  if (cls == nullptr) {
    return false;
  }

  return jni()->IsAssignableFrom(cls, jniproxy::Map_Entry()->GetClass());
}


WellKnownJClass MapEntryTypeEvaluator::GetKeyWellKnownJClass(
    MethodCaller* method_caller,
    jobject obj) {
  if (obj == nullptr) {
    return WellKnownJClass::Unknown;
  }

  ErrorOr<JVariant> key = method_caller->Invoke(
      map_entry_get_key_,
      JVariant::BorrowedRef(obj),
      {});
  if (key.is_error()) {
    return WellKnownJClass::Unknown;
  }

  jobject key_obj = nullptr;
  key.value().get<jobject>(&key_obj);

  std::string signature = GetObjectClassSignature(key_obj);
  if (signature.empty()) {
    return WellKnownJClass::Unknown;
  }

  auto jsignature = JSignatureFromSignature(signature);
  return WellKnownJClassFromSignature(jsignature);
}


void MapEntryTypeEvaluator::Evaluate(
    MethodCaller* method_caller,
    jobject obj,
    std::vector<NamedJVariant>* members) {
  *members = std::vector<NamedJVariant>(2);
  Evaluate(method_caller, obj, &(*members)[0], &(*members)[1]);
}


void MapEntryTypeEvaluator::Evaluate(
    MethodCaller* method_caller,
    jobject obj,
    NamedJVariant* key,
    NamedJVariant* value) {
  const struct {
    NamedJVariant* member;
    const char* member_name;
    const ClassMetadataReader::Method& method;
  } member_evaluators[] = {
    { key, "key", map_entry_get_key_ },
    { value, "value", map_entry_get_value_ }
  };

  for (int i = 0; i < arraysize(member_evaluators); ++i) {
    member_evaluators[i].member->name = member_evaluators[i].member_name;

    ErrorOr<JVariant> result = method_caller->Invoke(
        member_evaluators[i].method,
        JVariant::BorrowedRef(obj),
        {});
    if (result.is_error()) {
      member_evaluators[i].member->status.is_error = true;
      member_evaluators[i].member->status.refers_to =
          StatusMessageModel::Context::VARIABLE_VALUE;
      member_evaluators[i].member->status.description = result.error_message();
    } else {
      member_evaluators[i].member->value =
          ErrorOr<JVariant>::detach_value(std::move(result));
      member_evaluators[i].member->value.change_ref_type(
          JVariant::ReferenceKind::Global);
    }
  }
}

}  // namespace cdbg
}  // namespace devtools


