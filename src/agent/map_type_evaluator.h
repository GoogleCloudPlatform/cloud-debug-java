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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_MAP_TYPE_EVALUATOR_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_MAP_TYPE_EVALUATOR_H_

#include <memory>

#include "common.h"
#include "iterable_type_evaluator.h"
#include "map_entry_type_evaluator.h"
#include "type_evaluator.h"

namespace devtools {
namespace cdbg {

class ClassMetadataReader;
struct NamedJVariant;

// Captures elements of a Java class that implements Map interface.
// This class doesn't verify that the object is safe for method calls.
// The iteration of a map starts with iteration of the return value of
// Map.entrySet() call. Then "MapTypeEvaluator" formats the map entries
// as either "[key] = value" for well known key types or as
// "[i] = { key = ..., value = ... }" for complex key types.
class MapTypeEvaluator : public TypeEvaluator {
 public:
  MapTypeEvaluator();

  // Checks whether the specified class implements "java.lang.Map"
  // interface.
  bool IsMap(jclass cls) const;

  std::string GetEvaluatorName() override { return "MapTypeEvaluator"; }

  void Evaluate(
      MethodCaller* method_caller,
      const ClassMetadataReader::Entry& class_metadata,
      jobject obj,
      bool is_watch_expression,
      std::vector<NamedJVariant>* members) override;

 private:
  // If the map has value type keys (like primitive types or strings),
  // transform the representation of a map from array of entries:
  //   [0] = object
  //     key = 3
  //     value = ...
  //   [1] = object
  //     key = 8
  //     value = ...
  // to a more natural representation of a dictionary:
  //   [3] = ...
  //   [8] = ...
  // If the map has complex objects as keys, retains "members" as is.
  void TryInlineMap(
      MethodCaller* method_caller,
      std::vector<NamedJVariant>* members);

  // Inlines a single map entry. Changes "key" and "value".
  void InlineMapEntry(
      NamedJVariant* member,
      NamedJVariant* key,
      NamedJVariant* value);

 private:
  // Used to evaluate Set<Map.Entry<K,V>> returned by Map.entrySet().
  IterableTypeEvaluator iterable_evaluator_;

  // Used to inline the map.
  MapEntryTypeEvaluator map_entry_evaluator_;

  // Method metadata for the Java methods this pretty printer is using.
  const ClassMetadataReader::Method map_entry_set_;

  DISALLOW_COPY_AND_ASSIGN(MapTypeEvaluator);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_MAP_TYPE_EVALUATOR_H_

