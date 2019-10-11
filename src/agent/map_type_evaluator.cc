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

#include "map_type_evaluator.h"

#include "jni_proxy_ju_map.h"
#include "messages.h"
#include "model.h"
#include "model_util.h"
#include "value_formatter.h"

namespace devtools {
namespace cdbg {

MapTypeEvaluator::MapTypeEvaluator()
    : map_entry_set_(InstanceMethod(
          "Ljava/util/Map;",
          "entrySet",
          "()Ljava/util/Set;")) {
}


bool MapTypeEvaluator::IsMap(jclass cls) const {
  if (cls == nullptr) {
    return false;
  }

  return jni()->IsAssignableFrom(cls, jniproxy::Map()->GetClass());
}


void MapTypeEvaluator::Evaluate(
    MethodCaller* method_caller,
    const ClassMetadataReader::Entry& class_metadata,
    jobject obj,
    bool is_watch_expression,
    std::vector<NamedJVariant>* members) {
  std::unique_ptr<FormatMessageModel> error_message;

  // Set<Map.Entry<K,V>> entries = obj.entrySet();
  ErrorOr<JVariant> entries = method_caller->Invoke(
      map_entry_set_,
      JVariant::BorrowedRef(obj),
      {});
  if (entries.is_error()) {
    members->push_back(NamedJVariant::ErrorStatus(entries.error_message()));
    return;
  }

  jobject entries_obj = nullptr;
  entries.value().get<jobject>(&entries_obj);
  if (obj == nullptr) {
    // This is highly unlikely to happen. It indicates some very rudimentary
    // problem with the map.
    members->push_back(
        NamedJVariant::ErrorStatus({ NullPointerDereference }));
    return;
  }

  iterable_evaluator_.Evaluate(
      method_caller, entries_obj, is_watch_expression, members);

  TryInlineMap(method_caller, members);
}


void MapTypeEvaluator::TryInlineMap(
    MethodCaller* method_caller,
    std::vector<NamedJVariant>* members) {
  // Do nothing if any of the members has a key that doesn't map to a value
  // evaluator.
  WellKnownJClass map_keys_well_known_jclass = WellKnownJClass::Unknown;
  for (const NamedJVariant& member : *members) {
    if (member.value.type() == JType::Void) {
      continue;
    }

    jobject obj = nullptr;
    member.value.get<jobject>(&obj);
    if (obj == nullptr) {
      return;
    }

    WellKnownJClass well_known_jclass =
        map_entry_evaluator_.GetKeyWellKnownJClass(method_caller, obj);
    if (!ValueFormatter::IsImmutableValueObject(well_known_jclass)) {
      return;
    }

    if (map_keys_well_known_jclass == WellKnownJClass::Unknown) {
      map_keys_well_known_jclass = well_known_jclass;
    } else if (map_keys_well_known_jclass != well_known_jclass) {
      return;  // Non-uniform keys.
    }
  }

  // Inline the map.
  for (NamedJVariant& member : *members) {
    if (member.value.type() == JType::Void) {
      continue;
    }

    jobject obj = nullptr;
    member.value.get<jobject>(&obj);
    if (obj == nullptr) {
      continue;
    }

    NamedJVariant entry_key;
    NamedJVariant entry_value;
    map_entry_evaluator_.Evaluate(
        method_caller,
        obj,
        &entry_key,
        &entry_value);

    entry_key.well_known_jclass = map_keys_well_known_jclass;

    // We don't need to include the type information as the type is only
    // associated with the value, never with the key.
    std::string key;
    ValueFormatter::Format(entry_key, ValueFormatter::Options(), &key, nullptr);

    member.name.clear();
    member.name.reserve(key.size() + 2);
    member.name += '[';
    member.name += key;
    member.name += ']';

    member.status = entry_value.status;
    member.value.swap(&entry_value.value);
  }
}

}  // namespace cdbg
}  // namespace devtools


