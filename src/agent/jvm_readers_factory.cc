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

#include "jvm_readers_factory.h"

#include <algorithm>

#include "class_indexer.h"
#include "class_metadata_reader.h"
#include "class_path_lookup.h"
#include "jvm_object_array_reader.h"
#include "jvm_primitive_array_reader.h"
#include "jvmti_buffer.h"
#include "messages.h"
#include "method_locals.h"
#include "model.h"
#include "safe_method_caller.h"
#include "type_util.h"

namespace devtools {
namespace cdbg {

JvmReadersFactory::JvmReadersFactory(
    JvmEvaluators* evaluators,
    jmethodID method,
    jlocation location)
    : evaluators_(evaluators),
      method_(method),
      location_(location) {
}

std::string JvmReadersFactory::GetEvaluationPointClassName() {
  jvmtiError err = JVMTI_ERROR_NONE;

  jclass cls = nullptr;
  err = jvmti()->GetMethodDeclaringClass(method_, &cls);
  if (err != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "GetMethodDeclaringClass failed, error: " << err;
    return std::string();
  }

  JniLocalRef auto_cls(cls);

  JvmtiBuffer<char> class_signature_buffer;
  err = jvmti()->GetClassSignature(
      cls,
      class_signature_buffer.ref(),
      nullptr);
  if (err != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "GetClassSignature failed, error: " << err;
    return std::string();
  }

  return TypeNameFromJObjectSignature(class_signature_buffer.get());
}

JniLocalRef JvmReadersFactory::FindClassByName(
    const std::string& class_name, FormatMessageModel* error_message) {
  ClassIndexer* class_indexer = evaluators_->class_indexer;
  JniLocalRef cls;

  *error_message = {};

  // Case 1: class name is fully qualified (i.e. includes the package name)
  // and has been already loaded by the JVM.
  cls = class_indexer->FindClassByName(class_name);
  if (cls != nullptr) {
    return cls;
  }

  // Case 2: class name is relative to "java.lang" package. We assume that
  // the class has been already loaded in this case.
  cls = class_indexer->FindClassByName("java.lang." + class_name);
  if (cls != nullptr) {
    return cls;
  }

  // Case 3: class name is relative to the current scope.
  std::string current_class_name = GetEvaluationPointClassName();
  size_t name_pos = current_class_name.find_last_of('.');
  if ((name_pos > 0) && (name_pos != std::string::npos)) {
    cls = class_indexer->FindClassByName(
        current_class_name.substr(0, name_pos + 1) + class_name);
    if (cls != nullptr) {
      return cls;
    }
  }

  // Case 4: the class is either unqualified (i.e. doesn't include the package
  // name) or hasn't been loaded yet. Note that this will not include JDK
  // classes (like java.lang.String). These classes are usually loaded very
  // eary and we don't want to waste resources indexing all of them.
  std::vector<std::string> candidates =
      evaluators_->class_path_lookup->FindClassesByName(class_name);
  std::sort(candidates.begin(), candidates.end());
  switch (candidates.size()) {
    case 0:
      *error_message = { InvalidIdentifier, { class_name } };
      return nullptr;

    case 1:
      cls = class_indexer->FindClassBySignature(candidates[0]);
      if (cls != nullptr) {
        return cls;
      }

      *error_message = {
        ClassNotLoaded,
        {
         TypeNameFromJObjectSignature(candidates[0]),
         candidates[0]
        }
      };
      return nullptr;

    case 2:
      *error_message = {
        AmbiguousClassName2,
        {
          class_name,
          TypeNameFromJObjectSignature(candidates[0]),
          TypeNameFromJObjectSignature(candidates[1])
        }
      };
      return nullptr;

    case 3:
      *error_message = {
        AmbiguousClassName3,
        {
          class_name,
          TypeNameFromJObjectSignature(candidates[0]),
          TypeNameFromJObjectSignature(candidates[1]),
          TypeNameFromJObjectSignature(candidates[2])
        }
      };
      return nullptr;

    default:
      *error_message = {
        AmbiguousClassName4OrMore,
        {
          class_name,
          TypeNameFromJObjectSignature(candidates[0]),
          TypeNameFromJObjectSignature(candidates[1]),
          TypeNameFromJObjectSignature(candidates[2]),
          std::to_string(candidates.size() - 3)
        }
      };
      return nullptr;
  }
}

bool JvmReadersFactory::IsAssignable(const std::string& from_signature,
                                     const std::string& to_signature) {
  ClassIndexer* class_indexer = evaluators_->class_indexer;

  // Currently array types not supported in this function.
  if (IsArrayObjectSignature(from_signature) ||
      IsArrayObjectSignature(to_signature)) {
    return false;
  }

  // Get the class object corresponding to "from_signature".
  JniLocalRef from_cls = class_indexer->FindClassBySignature(from_signature);
  if (from_cls == nullptr) {
    return false;
  }

  // Get the class object corresponding to "to_signature".
  JniLocalRef to_cls = class_indexer->FindClassBySignature(to_signature);
  if (to_cls == nullptr) {
    return false;
  }

  return jni()->IsAssignableFrom(
      static_cast<jclass>(from_cls.get()),
      static_cast<jclass>(to_cls.get()));
}

std::unique_ptr<LocalVariableReader>
JvmReadersFactory::CreateLocalVariableReader(
    const std::string& variable_name, FormatMessageModel* error_message) {
  std::shared_ptr<const MethodLocals::Entry> method =
      evaluators_->method_locals->GetLocalVariables(method_);

  for (const std::unique_ptr<LocalVariableReader>& local : method->locals) {
    if (local->IsDefinedAtLocation(location_) &&
        (variable_name == local->GetName())) {
      return local->Clone();
    }
  }

  return nullptr;
}

std::unique_ptr<LocalVariableReader>
JvmReadersFactory::CreateLocalInstanceReader() {
  std::shared_ptr<const MethodLocals::Entry> method =
      evaluators_->method_locals->GetLocalVariables(method_);

  if (method->local_instance == nullptr) {
    return nullptr;
  }

  return method->local_instance->Clone();
}

std::unique_ptr<InstanceFieldReader>
JvmReadersFactory::CreateInstanceFieldReader(
    const std::string& class_signature, const std::string& field_name,
    FormatMessageModel* error_message) {
  JniLocalRef cls =
      evaluators_->class_indexer->FindClassBySignature(class_signature);
  if (cls == nullptr) {
    // JVM does not defer loading field types, so it should never happen.
    LOG(WARNING) << "Class not found: " << class_signature;

    *error_message = {
      ClassNotLoaded,
      { TypeNameFromJObjectSignature(class_signature), class_signature }
    };

    return nullptr;
  }

  const ClassMetadataReader::Entry& metadata =
      evaluators_->class_metadata_reader->GetClassMetadata(
          static_cast<jclass>(cls.get()));

  for (const auto& field : metadata.instance_fields) {
    if (field->GetName() == field_name) {
      return field->Clone();
    }
  }

  *error_message = {
    InstanceFieldNotFound,
    { field_name, TypeNameFromJObjectSignature(class_signature) }
  };

  return nullptr;  // No instance field named "field_name" found Java class.
}

std::unique_ptr<StaticFieldReader> JvmReadersFactory::CreateStaticFieldReader(
    const std::string& field_name, FormatMessageModel* error_message) {
  jvmtiError err = JVMTI_ERROR_NONE;

  jclass cls = nullptr;
  err = jvmti()->GetMethodDeclaringClass(method_, &cls);
  if (err != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "GetMethodDeclaringClass failed, error: " << err;
    return nullptr;
  }

  JniLocalRef auto_cls(cls);

  std::unique_ptr<StaticFieldReader> reader =
      CreateStaticFieldReader(cls, field_name);

  if (reader == nullptr) {
    *error_message = { InvalidIdentifier, { field_name } };
  }

  return reader;
}

std::unique_ptr<StaticFieldReader> JvmReadersFactory::CreateStaticFieldReader(
    const std::string& class_name, const std::string& field_name,
    FormatMessageModel* error_message) {
  JniLocalRef cls = FindClassByName(class_name, error_message);
  if (cls == nullptr) {
    return nullptr;
  }

  std::unique_ptr<StaticFieldReader> reader =
      CreateStaticFieldReader(static_cast<jclass>(cls.get()), field_name);

  if (reader == nullptr) {
    *error_message = { StaticFieldNotFound, { field_name, class_name } };
  }

  return reader;
}

std::unique_ptr<StaticFieldReader> JvmReadersFactory::CreateStaticFieldReader(
    jclass cls, const std::string& field_name) {
  const ClassMetadataReader::Entry& metadata =
      evaluators_->class_metadata_reader->GetClassMetadata(cls);

  for (const auto& field : metadata.static_fields) {
    if (field->GetName() == field_name) {
      return field->Clone();
    }
  }

  return nullptr;  // No static field named "field_name" found Java class.
}

std::vector<ClassMetadataReader::Method>
JvmReadersFactory::FindLocalInstanceMethods(const std::string& method_name) {
  std::shared_ptr<const MethodLocals::Entry> method =
      evaluators_->method_locals->GetLocalVariables(method_);

  if (method->local_instance == nullptr) {
    // The current class is expected to be loaded. So, we must be in a
    // static method with no instance methods.
    return {};
  }

  std::vector<ClassMetadataReader::Method> methods;
  FormatMessageModel unused_error_message;
  bool unused_success = FindInstanceMethods(
      method->local_instance->GetStaticType().object_signature,
      method_name,
      &methods,
      &unused_error_message);
  DCHECK(unused_success);

  return methods;
}

bool JvmReadersFactory::FindInstanceMethods(
    const std::string& class_signature, const std::string& method_name,
    std::vector<ClassMetadataReader::Method>* methods,
    FormatMessageModel* error_message) {
  JniLocalRef cls =
      evaluators_->class_indexer->FindClassBySignature(class_signature);
  if (cls == nullptr) {
    LOG(ERROR) << "Instance class not found: " << class_signature;
    *error_message = {
      ClassNotLoaded,
      { TypeNameFromJObjectSignature(class_signature), class_signature }
    };
    return false;
  }

  *methods =
      FindClassMethods(static_cast<jclass>(cls.get()), false, method_name);
  return true;
}

std::vector<ClassMetadataReader::Method> JvmReadersFactory::FindStaticMethods(
    const std::string& method_name) {
  JniLocalRef cls = GetMethodDeclaringClass(method_);
  if (cls == nullptr) {
    // This should not happen. The current class should always be loaded.
    return {};
  }

  return FindClassMethods(static_cast<jclass>(cls.get()), true, method_name);
}

bool JvmReadersFactory::FindStaticMethods(
    const std::string& class_name, const std::string& method_name,
    std::vector<ClassMetadataReader::Method>* methods,
    FormatMessageModel* error_message) {
  JniLocalRef cls = FindClassByName(class_name, error_message);
  if (cls == nullptr) {
    return false;
  }

  *methods =
      FindClassMethods(static_cast<jclass>(cls.get()), true, method_name);
  return true;
}

std::vector<ClassMetadataReader::Method> JvmReadersFactory::FindClassMethods(
    jclass cls, bool is_static, const std::string& method_name) {
  const ClassMetadataReader::Entry& class_metadata =
      evaluators_->class_metadata_reader->GetClassMetadata(cls);

  std::vector<ClassMetadataReader::Method> matches;
  for (const auto& class_method : class_metadata.methods) {
    if ((class_method.is_static() == is_static) &&
        (class_method.name == method_name)) {
      matches.push_back(class_method);
    }
  }

  return matches;
}

std::unique_ptr<ArrayReader> JvmReadersFactory::CreateArrayReader(
    const JSignature& array_signature) {
  if (!IsArrayObjectType(array_signature)) {
    return nullptr;
  }

  const JSignature array_element_signature =
      GetArrayElementJSignature(array_signature);

  switch (array_element_signature.type) {
    case JType::Void:
      return nullptr;  // Bad "array_signature".

    case JType::Boolean:
      return std::unique_ptr<ArrayReader>(
          new JvmPrimitiveArrayReader<jboolean>);

    case JType::Byte:
      return std::unique_ptr<ArrayReader>(
          new JvmPrimitiveArrayReader<jbyte>);

    case JType::Char:
      return std::unique_ptr<ArrayReader>(
          new JvmPrimitiveArrayReader<jchar>);

    case JType::Short:
      return std::unique_ptr<ArrayReader>(
          new JvmPrimitiveArrayReader<jshort>);

    case JType::Int:
      return std::unique_ptr<ArrayReader>(
          new JvmPrimitiveArrayReader<jint>);

    case JType::Long:
      return std::unique_ptr<ArrayReader>(
          new JvmPrimitiveArrayReader<jlong>);

    case JType::Float:
      return std::unique_ptr<ArrayReader>(
          new JvmPrimitiveArrayReader<jfloat>);

    case JType::Double:
      return std::unique_ptr<ArrayReader>(
          new JvmPrimitiveArrayReader<jdouble>);

    case JType::Object:
      return std::unique_ptr<ArrayReader>(new JvmObjectArrayReader);
  }

  return nullptr;
}


}  // namespace cdbg
}  // namespace devtools


