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
#include <set>
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


JvmClassMetadataReader::JvmClassMetadataReader(
    MemberVisibilityPolicy* member_visibility_policy)
    : member_visibility_policy_(member_visibility_policy),
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
    MutexLock reader_lock(&mu_);

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
      MutexLock writer_lock(&mu_);

      cls_cache_.Insert(cls, std::move(metadata));

      // Return currently or previously inserted entry. Returning reference to
      // "jobject_map" entry as in case 1.
      return *cls_cache_.Find(cls);
    }
  }
}


void JvmClassMetadataReader::LoadClassMetadata(jclass cls, Entry* metadata) {
  int err = JVMTI_ERROR_NONE;

  JvmtiBuffer<char> signature_buffer;
  err = jvmti()->GetClassSignature(cls, signature_buffer.ref(), nullptr);
  if (err != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "GetClassSignature failed, error: " << err;
    return;
  }

  metadata->signature = JSignatureFromSignature(signature_buffer.get());

  JniLocalRef current_class_ref;

  // Start from the current class and go down the inheritance chain.
  current_class_ref.reset(jni()->NewLocalRef(cls));

  // Maintain set of methods we discovered in superclasses to ignore those
  // in base classes. The pair stores method name and signature.
  std::set<std::pair<string, string>> registered_methods;

  while (current_class_ref != nullptr) {
    JvmtiBuffer<char> class_signature_buffer;
    err = jvmti()->GetClassSignature(
        static_cast<jclass>(current_class_ref.get()),
        class_signature_buffer.ref(),
        nullptr);
    if (err != JVMTI_ERROR_NONE) {
      // If we cannot load some ancestors, we will simply stop at that level
      // and display all that we could retrieve.
      LOG(ERROR) << "GetClassSignature failed, error: " << err;
      break;
    }

    string class_signature = class_signature_buffer.get();

    // Load list of all the fields of the class. This includes both member
    // and static fields.
    jint cls_fields_count = 0;
    JvmtiBuffer<jfieldID> cls_fields;
    err = jvmti()->GetClassFields(
        static_cast<jclass>(current_class_ref.get()),
        &cls_fields_count,
        cls_fields.ref());

    if (err != JVMTI_ERROR_NONE) {
      LOG(ERROR) << "GetClassFields failed, error: " << err;
    } else {
      // Walk fields in reverse order so that the fields from the base class
      // show up before the fields from the subclass.
      for (int i = cls_fields_count - 1; i >= 0; --i) {
        LoadFieldInfo(
            static_cast<jclass>(current_class_ref.get()),
            class_signature,
            cls_fields.get()[i],
            metadata);
      }
    }

    // Load list of all the methods of the class.
    jint methods_count = 0;
    JvmtiBuffer<jmethodID> methods;
    err = jvmti()->GetClassMethods(
        static_cast<jclass>(current_class_ref.get()),
        &methods_count,
        methods.ref());
    if (err != JVMTI_ERROR_NONE) {
      LOG(ERROR) << "GetClassMethods failed, error: " << err;
    } else {
      // Load metadata of each method. We don't care about the order of methods
      // in the list.
      for (int i = 0; i < methods_count; ++i) {
        Method method_metadata = LoadMethodInfo(
            static_cast<jclass>(current_class_ref.get()),
            class_signature,
            methods.get()[i]);

        if (method_metadata.name.empty()) {
          continue;
        }

        // If two instance methods have the same arguments, the one in the
        // superclass overrides the one in the base class. This will be true
        // even if return types are difference (this is called covariance).
        std::pair<string, string> key;
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
        if (registered_methods.find(key) != registered_methods.end()) {
          continue;
        }

        // Add the method to the registry.
        registered_methods.insert(std::move(key));
        metadata->methods.push_back(std::move(method_metadata));
      }
    }

    // Free the current local reference and allocate a new local reference
    // corresponding to the superclass of "current_class_ref".
    current_class_ref.reset(
        jni()->GetSuperclass(static_cast<jclass>(current_class_ref.get())));
  }

  // Reverse the the lists to accomodate for LoadFieldInfo appending new
  // elements to the end of the list.
  std::reverse(
      metadata->instance_fields.begin(),
      metadata->instance_fields.end());
  std::reverse(
      metadata->static_fields.begin(),
      metadata->static_fields.end());
}


void JvmClassMetadataReader::LoadFieldInfo(
    jclass cls,
    const string& class_signature,
    jfieldID field_id,
    Entry* metadata) {
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

  string field_name = field_name_buffer.get();
  string field_signature = field_signature_buffer.get();

  if (!member_visibility_policy_->IsFieldDebuggerVisible(
          cls,
          class_signature,
          field_modifiers,
          field_name,
          field_signature)) {
    if ((field_modifiers & JVM_ACC_STATIC) == 0) {
      metadata->instance_fields_omitted = true;
    }
    return;  // Field is invisible.
  }

  if ((field_modifiers & JVM_ACC_STATIC) == 0) {
    // Instance field.
    std::unique_ptr<InstanceFieldReader> reader(
        new JvmInstanceFieldReader(
            field_name,
            field_id,
            JSignatureFromSignature(field_signature)));
    metadata->instance_fields.push_back(std::move(reader));
  } else {
    // Static field.
    std::unique_ptr<StaticFieldReader> reader(
        new JvmStaticFieldReader(
            cls,
            field_name,
            field_id,
            JSignatureFromSignature(field_signature)));
    metadata->static_fields.push_back(std::move(reader));
  }
}


JvmClassMetadataReader::Method JvmClassMetadataReader::LoadMethodInfo(
    jclass cls,
    const string& class_signature,
    jmethodID method_id) {
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

  if (!member_visibility_policy_->IsMethodDebuggerVisible(
          cls,
          class_signature,
          method_modifiers,
          method.name,
          method.signature)) {
    return Method();  // Method is invisible.
  }

  return method;
}


}  // namespace cdbg
}  // namespace devtools


