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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_OBJECT_EVALUATOR_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_OBJECT_EVALUATOR_H_

#include "class_indexer.h"
#include "class_metadata_reader.h"
#include "common.h"
#include "config.h"
#include "iterable_type_evaluator.h"
#include "jvariant.h"
#include "map_entry_type_evaluator.h"
#include "map_type_evaluator.h"
#include "object_evaluator.h"
#include "stringable_type_evaluator.h"
#include "type_evaluator.h"

namespace devtools {
namespace cdbg {

class JvmObjectEvaluator : public ObjectEvaluator {
 public:
  struct Options {
    // Determines whether classes that implement the Iterable interface should
    // be pretty printed.
    bool pretty_print_iterable = true;

    // Determines whether classes that implement the Map interface should be
    // pretty printed.
    bool pretty_print_map = true;

    // Determines whether classes that implement the Map.Entry interface should
    // be pretty printed.
    bool pretty_print_map_entry = true;

    // Determines whether supported classes should be pretty printed using
    // "toString()". StringableTypeEvaluator::IsSupported() is used to determine
    // if a class is supported for pretty printing using "toString()".
    bool pretty_print_stringable = true;
  };

  explicit JvmObjectEvaluator(ClassMetadataReader* class_metadata_reader);

  ~JvmObjectEvaluator() override;

  void Initialize() { Initialize(Options()); }

  void Initialize(const Options& options);

  void Evaluate(
      MethodCaller* method_caller,
      jobject obj,
      bool is_watch_expression,
      std::vector<NamedJVariant>* members) override;

  // Selects the most appropriate pretty printer given the class metadata. If
  // the class doesn't have a specialized pretty printer, this function returns
  // reference to "GenericTypeEvaluator". Returns null in case of bad
  // metadata.
  // This function is public for unit testing purposes.
  TypeEvaluator* SelectEvaluator(
      jclass cls,
      const ClassMetadataReader::Entry& metadata) const;

 private:
  ClassMetadataReader* const class_metadata_reader_;

  std::unique_ptr<TypeEvaluator> generic_;
  std::unique_ptr<TypeEvaluator> array_[TotalJTypes];
  std::unique_ptr<IterableTypeEvaluator> iterable_;
  std::unique_ptr<MapEntryTypeEvaluator> map_entry_;
  std::unique_ptr<MapTypeEvaluator> map_;
  std::unique_ptr<StringableTypeEvaluator> stringable_;

  DISALLOW_COPY_AND_ASSIGN(JvmObjectEvaluator);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_OBJECT_EVALUATOR_H_
