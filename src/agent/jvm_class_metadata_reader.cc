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

#include "jvm_class_metadata_reader.h"

#include <algorithm>

#include "jni_proxy_object.h"
#include "jni_utils.h"
#include "jvm_instance_field_reader.h"
#include "jvm_static_field_reader.h"
#include "jvmti_buffer.h"

namespace devtools {
namespace cdbg {

static void ReleaseEntryRefs(jobject cls, ClassMetadataReader::Entry* entry) {
  for (auto& static_field_reader : entry->static_fields) {
    static_field_reader->ReleaseRef();
  }
}

// Adjusts the names of auto-generated fields by removing the "val$" prefix.
//
// The most important use case is when a local variable "foo" in an outer scope
// is captured in an inner scope. When using Java 8, the variable is added to
// the inner class as a "val$foo" field, but the user expects to see/use it as
// "foo". Therefore, we remove the "val$" prefix. In Java 7, the debugger
// doesn't know about foo at all, so we cannot do anything about it.
//
// Example:
//    String foo;
//    new SomeClass() {
//      @Override
//      String getFoo () {
//        return foo;  // In Java 8, variable foo becomes SomeClass.this.val$foo
//                     // but here we convert it to SomeClass.this.foo
//      }
//    }
static std::string ProcessFieldName(std::string name) {
  constexpr char kPrefix[] = "val$";
  constexpr int kPrefixLength = sizeof(kPrefix) / sizeof(kPrefix[0]) - 1;

  if (name.compare(0, kPrefixLength, kPrefix) == 0) {
    return name.substr(kPrefixLength);
  }

  return name;
}

JvmClassMetadataReader::JvmClassMetadataReader(
    DataVisibilityPolicy* data_visibility_policy)
    : data_visibility_policy_(data_visibility_policy),
      cls_cache_(ReleaseEntryRefs) {
}


JvmClassMetadataReader::~JvmClassMetadataReader() {
}


const ClassMetadataReader::Entry&
JvmClassMetadataReader::GetClassMetadata(jclass cls) {
  // Safeguard against null references.
  DCHECK(cls != nullptr);
  if (cls == nullptr) {
    static Entry empty_class_metadata;
    return empty_class_metadata;
  }

  // Case 1: the class information is cached.
  {
    absl::MutexLock reader_lock(&mu_);

    const Entry* metadata = cls_cache_.Find(cls);
    if (metadata != nullptr) {
      // It is safe to return reference to "jobject_map" entry. "jobject_map"
      // guarantees that the elements are not moved when data structure is
      // expanded or contracted.
      return *metadata;
    }
  }

  // Case 2: we need to load the class information.
  {
    Entry metadata;
    LoadClassMetadata(cls, &metadata);

    {
      absl::MutexLock writer_lock(&mu_);

      cls_cache_.Insert(cls, std::move(metadata));

      // Return currently or previously inserted entry. Returning reference to
      // "jobject_map" entry as in case 1.
      return *cls_cache_.Find(cls);
    }
  }
}


void JvmClassMetadataReader::LoadClassMetadata(jclass cls, Entry* metadata) {
  std::string signature = GetClassSignature(cls);
  if (signature.empty()) {
    return;
  }

  metadata->signature = JSignatureFromSignature(signature);

  // Start from the current class and go down the inheritance chain.
  JniLocalRef current_class_ref = JniNewLocalRef(cls);

  // Maintain set of methods we discovered in superclasses to ignore those
  // in base classes. The pair stores method name and signature.
  std::set<std::pair<std::string, std::string>> registered_methods;

  while (current_class_ref != nullptr) {
    LoadSingleClassMetadata(
        static_cast<jclass>(current_class_ref.get()),
        &registered_methods,
        metadata);

    LoadImplementedInterfacesMetadata(
        static_cast<jclass>(current_class_ref.get()),
        &registered_methods,
        metadata);

    // Free the current local reference and allocate a new local reference
    // corresponding to the superclass of "current_class_ref".
    JniLocalRef next(
        jni()->GetSuperclass(static_cast<jclass>(current_class_ref.get())));

    // Don't skip "java.lang.Object" even if "cls" is an interface.
    if ((next == nullptr) &&
        !jni()->IsSameObject(current_class_ref.get(),
                             jniproxy::Object()->GetClass())) {
      LoadSingleClassMetadata(
          jniproxy::Object()->GetClass(),
          &registered_methods,
          metadata);
    }

    current_class_ref = std::move(next);
  }

  // Reverse the lists to accomodate for LoadFieldInfo appending new elements to
  // the end of the list.
  std::reverse(
      metadata->instance_fields.begin(),
      metadata->instance_fields.end());
  std::reverse(
      metadata->static_fields.begin(),
      metadata->static_fields.end());
}

void JvmClassMetadataReader::LoadImplementedInterfacesMetadata(
    jclass parent,
    std::set<std::pair<std::string, std::string>>* registered_methods,
    Entry* metadata) {
  jvmtiError err = JVMTI_ERROR_NONE;

  jint count = 0;
  JvmtiBuffer<jclass> interfaces;
  err = jvmti()->GetImplementedInterfaces(parent, &count, interfaces.ref());
  if (err != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "GetImplementedInterfaces failed, error: " << err;
    return;
  }

  for (int i = 0; i < count; ++i) {
    jclass interface = interfaces.get()[i];
    LoadSingleClassMetadata(interface, registered_methods, metadata);
    LoadImplementedInterfacesMetadata(interface, registered_methods, metadata);
  }
}

void JvmClassMetadataReader::LoadSingleClassMetadata(
    jclass cls,
    std::set<std::pair<std::string, std::string>>* registered_methods,
    Entry* metadata) {
  jvmtiError err = JVMTI_ERROR_NONE;

  std::string class_signature = GetClassSignature(cls);
  if (class_signature.empty()) {
    return;
  }

  // Get the visibility policy for the current class.
  std::unique_ptr<DataVisibilityPolicy::Class> class_visibility =
      data_visibility_policy_->GetClassVisibility(cls);

  // Load list of all the fields of the class. This includes both member
  // and static fields.
  jint cls_fields_count = 0;
  JvmtiBuffer<jfieldID> cls_fields;
  err = jvmti()->GetClassFields(cls, &cls_fields_count, cls_fields.ref());
  if (err != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "GetClassFields failed, error: " << err;
  } else {
    // Walk fields in reverse order so that the fields from the base class
    // show up before the fields from the subclass.
    for (int i = cls_fields_count - 1; i >= 0; --i) {
      LoadFieldInfo(
          cls,
          class_signature,
          cls_fields.get()[i],
          class_visibility.get(),
          metadata);
    }
  }

  // Load list of all the methods of the class.
  jint methods_count = 0;
  JvmtiBuffer<jmethodID> methods;
  err = jvmti()->GetClassMethods(
      cls,
      &methods_count,
      methods.ref());
  if (err != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "GetClassMethods failed, error: " << err;
  } else {
    // Load metadata of each method. We don't care about the order of methods
    // in the list.
    for (int i = 0; i < methods_count; ++i) {
      Method method_metadata = LoadMethodInfo(
          cls,
          class_signature,
          methods.get()[i],
          class_visibility.get());

      if (method_metadata.name.empty()) {
        continue;
      }

      // If two instance methods have the same arguments, the one in the
      // superclass overrides the one in the base class. This will be true
      // even if return types are difference (this is called covariance).
      std::pair<std::string, std::string> key;
      if (method_metadata.is_static()) {
        key = {
          method_metadata.name,
          method_metadata.signature
        };
      } else {
        key = {
          method_metadata.name,
          TrimReturnType(method_metadata.signature)
        };
      }

      // Skip base class methods that the inherited classes overwrote.
      if (registered_methods->find(key) != registered_methods->end()) {
        continue;
      }

      // Add the method to the registry.
      registered_methods->insert(std::move(key));
      metadata->methods.push_back(std::move(method_metadata));
    }
  }
}

void JvmClassMetadataReader::LoadFieldInfo(
    jclass cls, const std::string& class_signature, jfieldID field_id,
    DataVisibilityPolicy::Class* class_visibility, Entry* metadata) {
  int err = JVMTI_ERROR_NONE;

  jint field_modifiers = 0;
  err = jvmti()->GetFieldModifiers(cls, field_id, &field_modifiers);
  if (err != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "GetFieldModifiers failed, error: " << err;
    return;
  }

  JvmtiBuffer<char> field_name_buffer;
  JvmtiBuffer<char> field_signature_buffer;
  err = jvmti()->GetFieldName(
      cls,
      field_id,
      field_name_buffer.ref(),
      field_signature_buffer.ref(),
      nullptr);
  if (err != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "GetFieldName failed, error: " << err;
    return;
  }

  if ((field_signature_buffer.get() == nullptr) ||
      (field_signature_buffer.get()[0] == '\0')) {
    LOG(ERROR) << "Empty field signature is unexpected";
    return;
  }

  std::string field_name = ProcessFieldName(field_name_buffer.get());
  std::string field_signature = field_signature_buffer.get();

  if ((class_visibility != nullptr) &&
      !class_visibility->IsFieldVisible(field_name, field_modifiers)) {
    if ((field_modifiers & JVM_ACC_STATIC) == 0) {
      metadata->instance_fields_omitted = true;
    }
    return;  // Field is invisible.
  }

  // Determine if the data for this field is visible.  If not,
  // populate data_invisible with the appropriate error message
  FormatMessageModel data_invisible_message;
  const bool is_data_visible = class_visibility != nullptr ?
      class_visibility->IsFieldDataVisible(
          field_name,
          field_modifiers,
          &data_invisible_message.format) :
      true;

  if ((field_modifiers & JVM_ACC_STATIC) == 0) {
    // Instance field.
    std::unique_ptr<InstanceFieldReader> reader(
        new JvmInstanceFieldReader(
            field_name,
            field_id,
            JSignatureFromSignature(field_signature),
            !is_data_visible,
            data_invisible_message));
    metadata->instance_fields.push_back(std::move(reader));
  } else {
    // Static field.
    std::unique_ptr<StaticFieldReader> reader(
        new JvmStaticFieldReader(
            cls,
            field_name,
            field_id,
            JSignatureFromSignature(field_signature),
            !is_data_visible,
            data_invisible_message));
    metadata->static_fields.push_back(std::move(reader));
  }
}

JvmClassMetadataReader::Method JvmClassMetadataReader::LoadMethodInfo(
    jclass cls, const std::string& class_signature, jmethodID method_id,
    DataVisibilityPolicy::Class* class_visibility) {
  int err = JVMTI_ERROR_NONE;

  JvmtiBuffer<char> method_name;
  JvmtiBuffer<char> method_signature;
  err = jvmti()->GetMethodName(
      method_id,
      method_name.ref(),
      method_signature.ref(),
      nullptr);
  if (err != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "GetMethodName failed, error: " << err;
    return Method();
  }

  jint method_modifiers = 0;
  err = jvmti()->GetMethodModifiers(method_id, &method_modifiers);
  if (err != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "GetMethodModifiers failed, error: " << err;
    return Method();
  }

  Method method;
  method.class_signature = JSignatureFromSignature(class_signature);
  method.name = method_name.get();
  method.signature = method_signature.get();
  method.modifiers = method_modifiers;

  if ((class_visibility != nullptr) &&
      !class_visibility->IsMethodVisible(
          method.name,
          method.signature,
          method_modifiers)) {
    return Method();  // Method is invisible.
  }

  return method;
}

}  // namespace cdbg
}  // namespace devtools


