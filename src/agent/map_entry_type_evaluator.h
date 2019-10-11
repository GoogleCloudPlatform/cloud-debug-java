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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_MAP_ENTRY_TYPE_EVALUATOR_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_MAP_ENTRY_TYPE_EVALUATOR_H_

#include <memory>

#include "common.h"
#include "type_evaluator.h"

namespace devtools {
namespace cdbg {

// Pretty printer for Map.Entry<K,V> class.
class MapEntryTypeEvaluator : public TypeEvaluator {
 public:
  MapEntryTypeEvaluator();

  // Checks whether the specified class implements "java.lang.Map.Entry"
  // interface.
  bool IsMapEntry(jclass cls) const;

  std::string GetEvaluatorName() override { return "MapEntryTypeEvaluator"; }

  void Evaluate(
      MethodCaller* method_caller,
      const ClassMetadataReader::Entry& class_metadata,
      jobject obj,
      bool is_watch_expression,
      std::vector<NamedJVariant>* members) override {
    Evaluate(method_caller, obj, members);
  }

  void Evaluate(
      MethodCaller* method_caller,
      jobject obj,
      std::vector<NamedJVariant>* members);

  void Evaluate(
      MethodCaller* method_caller,
      jobject obj,
      NamedJVariant* key,
      NamedJVariant* value);

  // Checks class of the key object. Returns "WellKnownJClass::Unknown" if
  // "obj" is null, if method call to "getKey()" threw an exception, if
  // the key is null or in case of any error.
  WellKnownJClass GetKeyWellKnownJClass(
      MethodCaller* method_caller,
      jobject obj);

 private:
  // Method metadata for the Java methods this pretty printer is using.
  const ClassMetadataReader::Method map_entry_get_key_;
  const ClassMetadataReader::Method map_entry_get_value_;

  DISALLOW_COPY_AND_ASSIGN(MapEntryTypeEvaluator);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_MAP_ENTRY_TYPE_EVALUATOR_H_

