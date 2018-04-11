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

#include "jvm_object_evaluator.h"

#include "array_type_evaluator.h"
#include "config.h"
#include "generic_type_evaluator.h"
#include "iterable_type_evaluator.h"
#include "map_entry_type_evaluator.h"
#include "map_type_evaluator.h"
#include "messages.h"
#include "model.h"
#include "safe_method_caller.h"
#include "stringable_type_evaluator.h"
#include "value_formatter.h"

namespace devtools {
namespace cdbg {

// Checks if the result of object evaluator is error.
static bool HasEvaluatorFailed(const std::vector<NamedJVariant>& members) {
  if (members.empty()) {
    return true;
  }

  if (members.size() > 1) {
    return false;
  }

  const NamedJVariant& item = members[0];
  if (item.status.is_error &&
      item.status.description.format != MethodCallExceptionOccurred) {
    return true;
  }

  return false;
}


JvmObjectEvaluator::JvmObjectEvaluator(
    ClassMetadataReader* class_metadata_reader)
    : class_metadata_reader_(class_metadata_reader) {
}


JvmObjectEvaluator::~JvmObjectEvaluator() {
}


void JvmObjectEvaluator::Initialize(const Options& options) {
  generic_.reset(new GenericTypeEvaluator);

  array_[static_cast<int>(JType::Boolean)].reset(
      new ArrayTypeEvaluator<jboolean>);
  array_[static_cast<int>(JType::Byte)].reset(
      new ArrayTypeEvaluator<jbyte>);
  array_[static_cast<int>(JType::Char)].reset(
      new ArrayTypeEvaluator<jchar>);
  array_[static_cast<int>(JType::Short)].reset(
      new ArrayTypeEvaluator<jshort>);
  array_[static_cast<int>(JType::Int)].reset(
      new ArrayTypeEvaluator<jint>);
  array_[static_cast<int>(JType::Long)].reset(
      new ArrayTypeEvaluator<jlong>);
  array_[static_cast<int>(JType::Float)].reset(
      new ArrayTypeEvaluator<jfloat>);
  array_[static_cast<int>(JType::Double)].reset(
      new ArrayTypeEvaluator<jdouble>);
  array_[static_cast<int>(JType::Object)].reset(
      new ArrayTypeEvaluator<jobject>);

  if (options.pretty_print_iterable) {
    iterable_.reset(new IterableTypeEvaluator);
  }
  if (options.pretty_print_map_entry) {
    map_entry_.reset(new MapEntryTypeEvaluator);
  }
  if (options.pretty_print_map) {
    map_.reset(new MapTypeEvaluator);
  }
  if (options.pretty_print_stringable) {
    stringable_.reset(new StringableTypeEvaluator);
  }
}


void JvmObjectEvaluator::Evaluate(
    MethodCaller* method_caller,
    jobject obj,
    bool is_watch_expression,
    std::vector<NamedJVariant>* members) {
  members->clear();

  // Gets the class of the object we want to evaluate.
  JniLocalRef cls = GetObjectClass(obj);
  if (cls == nullptr) {
    // It would be more appropriate to set error status on the object variable,
    // but the interface doesn't support it. In any case this failure is
    // unlikely to happen in real life and if it does, the details about how
    // the error message is reflected is the least of our problems.
    LOG(ERROR) << "GetObjectClass failed";
    members->push_back(NamedJVariant::ErrorStatus(INTERNAL_ERROR_MESSAGE));
    return;
  }

  // Get the Java class metadata.
  const ClassMetadataReader::Entry& metadata =
      class_metadata_reader_->GetClassMetadata(static_cast<jclass>(cls.get()));

  const WellKnownJClass obj_well_known_jclass =
      WellKnownJClassFromSignature(metadata.signature);

  // Special treatment for Java strings. Usually Java strings will be formatted
  // as a value type based on the compile time signature of a variable. If,
  // however, the type of the local variable is "java.lang.Object", it is
  // possible that the variable will actually contain a string. For example:
  //     Object objString = "hippopotamus";
  // This scenario is very likely with generics where the compile time type of
  // class fields will be Object if no type constraints are specified.
  if (ValueFormatter::IsImmutableValueObject(
        WellKnownJClassFromSignature(metadata.signature))) {
    *members = std::vector<NamedJVariant>(1);
    NamedJVariant& entry = (*members)[0];

    entry.name.clear();  // Keep name empty to indicate it's not really a field.
    entry.value.assign_new_ref(JVariant::ReferenceKind::Global, obj);
    entry.well_known_jclass = obj_well_known_jclass;

    return;
  }

  TypeEvaluator* evaluator = SelectEvaluator(
      static_cast<jclass>(cls.get()),
      metadata);
  if (evaluator == nullptr) {
    // Should never happen. Failure of "SelectEvaluator" indicates some bug in
    // this class.
    LOG(ERROR) << "Failed to select pretty evaluator, signature: "
               << metadata.signature.object_signature;

    members->push_back(NamedJVariant::ErrorStatus(INTERNAL_ERROR_MESSAGE));
    return;
  }

  evaluator->Evaluate(
      method_caller,
      metadata,
      obj,
      is_watch_expression,
      members);

  if ((evaluator != generic_.get()) && HasEvaluatorFailed(*members)) {
    members->clear();
    generic_->Evaluate(
        method_caller, metadata, obj, is_watch_expression, members);
  }
}


TypeEvaluator* JvmObjectEvaluator::SelectEvaluator(
    jclass cls,
    const ClassMetadataReader::Entry& metadata) const {
  const WellKnownJClass obj_well_known_jclass =
      WellKnownJClassFromSignature(metadata.signature);

  // Java array object.
  if (obj_well_known_jclass == WellKnownJClass::Array) {
    const JSignature array_element_signature =
        GetArrayElementJSignature(metadata.signature);

    int i = static_cast<int>(array_element_signature.type);
    if ((i < 0) || (i >= arraysize(array_)) || (array_[i] == nullptr)) {
      LOG(ERROR) << "Invalid array type "
                 << static_cast<int>(array_element_signature.type);
      return nullptr;
    }

    return array_[i].get();
  }

  // Pretty printer for maps.
  if ((map_ != nullptr) && map_->IsMap(cls)) {
    return map_.get();
  }

  // Pretty printer for standard collections implementing "Iterable".
  if ((iterable_ != nullptr) && iterable_->IsIterable(cls)) {
    return iterable_.get();
  }

  // Pretty printer for Map.Entry.
  if ((map_entry_ != nullptr) && (map_entry_->IsMapEntry(cls))) {
    return map_entry_.get();
  }

  // Pretty printer for stringable objects.
  // Although any class supports 'toString()', not any class is supported by
  // StringableEvaluator. The reason is that for some classes/objects calling
  // 'toString' might be too expensive (for example for some exception with
  // long call stack).
  if ((stringable_ != nullptr) && (stringable_->IsSupported(cls))) {
    return stringable_.get();
  }

  // We don't have a specialized pretty evaluator for this class. Use generic
  // one.
  return generic_.get();
}

}  // namespace cdbg
}  // namespace devtools


