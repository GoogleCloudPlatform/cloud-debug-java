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

#include "jvm_class_indexer.h"

#include "jni_proxy_class.h"
#include "jni_utils.h"
#include "jvmti_buffer.h"
#include "type_util.h"

namespace devtools {
namespace cdbg {

class JvmClassReference : public ClassIndexer::Type {
 public:
  JvmClassReference(ClassIndexer* class_indexer, const std::string& signature)
      : class_indexer_(class_indexer), signature_(signature) {}

  JType GetType() const override {
    return JType::Object;
  }

  const std::string& GetSignature() const override { return signature_; }

  jclass FindClass() override {
    {
      absl::MutexLock lock(&mu_);
      if (cls_ != nullptr) {
        return static_cast<jclass>(cls_.get());
      }
    }

    JniGlobalRef cls;
    if (IsArrayObjectSignature(signature_)) {
      JSignature element_signature = { JType::Object, signature_ };
      do {
        element_signature = GetArrayElementJSignature(element_signature);
      } while (IsArrayObjectType(element_signature));

      if ((element_signature.type == JType::Object) &&
          (class_indexer_->FindClassBySignature(
              element_signature.object_signature) == nullptr)) {
        return nullptr;  // Element class hasn't been loaded yet.
      }

      const std::string binary_name =
          BinaryNameFromJObjectSignature(signature_);
      cls = JniNewGlobalRef(
          jniproxy::Class()->forName(binary_name)
          .Release(ExceptionAction::LOG_AND_IGNORE)
          .get());
      if (cls == nullptr) {
        LOG(ERROR) << "Failed to load array class " << binary_name;
      }
    } else {
      cls = JniNewGlobalRef(
          class_indexer_->FindClassBySignature(signature_).get());
    }

    if (cls == nullptr) {
      return nullptr;
    }

    {
      absl::MutexLock lock(&mu_);
      if (cls_ == nullptr) {
        cls_ = std::move(cls);
      }

      return static_cast<jclass>(cls_.get());
    }
  }

  jfieldID FindField(bool is_static, const std::string& name,
                     const std::string& signature) override {
    jclass cls = FindClass();
    if (cls == nullptr) {
      return nullptr;
    }

    std::string key;
    key += (is_static ? 'S' : 'I');
    key += '/';
    key += name;
    key += '/';
    key += signature;

    {
      absl::MutexLock lock(&mu_);
      auto it = fields_.find(key);
      if (it != fields_.end()) {
        return it->second;
      }
    }

    jfieldID field_id;
    if (is_static) {
      field_id = jni()->GetStaticFieldID(cls, name.c_str(), signature.c_str());
    } else {
      field_id = jni()->GetFieldID(cls, name.c_str(), signature.c_str());
    }

    if (!JniCheckNoException(name.c_str())) {
      // TODO: propagate the exception to the caller.
      return nullptr;
    }

    if (field_id == nullptr) {
      return nullptr;
    }

    {
      absl::MutexLock lock(&mu_);
      fields_.insert(std::make_pair(key, field_id));
    }

    return field_id;
  }

 private:
  ClassIndexer* const class_indexer_;
  const std::string signature_;
  JniGlobalRef cls_;
  std::map<std::string, jfieldID> fields_;
  absl::Mutex mu_;

  DISALLOW_COPY_AND_ASSIGN(JvmClassReference);
};


class JvmPrimitiveType : public ClassIndexer::Type {
 public:
  explicit JvmPrimitiveType(JType type)
      : type_(type),
        signature_(SignatureFromJSignature({ type })) {
  }

  JType GetType() const override {
    return type_;
  }

  const std::string& GetSignature() const override { return signature_; }

  jclass FindClass() override {
    // TODO: implement for completeness.
    DCHECK(false);
    return nullptr;
  }

  jfieldID FindField(bool is_static, const std::string& name,
                     const std::string& signature) override {
    return nullptr;  // Primitive types have no fields.
  }

 private:
  const JType type_;
  const std::string signature_;

  DISALLOW_COPY_AND_ASSIGN(JvmPrimitiveType);
};


JvmClassIndexer::JvmClassIndexer()
    : primitive_void_(new JvmPrimitiveType(JType::Void)),
      primitive_boolean_(new JvmPrimitiveType(JType::Boolean)),
      primitive_byte_(new JvmPrimitiveType(JType::Byte)),
      primitive_char_(new JvmPrimitiveType(JType::Char)),
      primitive_short_(new JvmPrimitiveType(JType::Short)),
      primitive_int_(new JvmPrimitiveType(JType::Int)),
      primitive_long_(new JvmPrimitiveType(JType::Long)),
      primitive_float_(new JvmPrimitiveType(JType::Float)),
      primitive_double_(new JvmPrimitiveType(JType::Double)) {
}


void JvmClassIndexer::Initialize() {
  // Keep track of already loaded classes.
  jint classes_count = 0;
  JvmtiBuffer<jclass> classes;
  int err = jvmti()->GetLoadedClasses(&classes_count, classes.ref());
  if (err != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "GetLoadedClasses failed, error: " << err;
    return;
  }

  for (int i = 0; i < classes_count; ++i) {
    // Retrieve the class status. Ignore classes that have not been prepared
    // since list of methods will not be available for these classes.
    jint class_status = 0;
    err = jvmti()->GetClassStatus(classes.get()[i], &class_status);
    if (err != JVMTI_ERROR_NONE) {
      LOG(ERROR) << "GetClassStatus failed, error: " << err;
      continue;
    }

    if ((class_status & JVMTI_CLASS_STATUS_PREPARED) == 0) {
      // The class has not been prepared yet. Ignore it for now.
      // JVMTI_EVENT_CLASS_PREPARE event will fire later when the class is
      // ready to be indexed.
      continue;
    }

    JvmtiOnClassPrepare(classes.get()[i]);
  }
}


void JvmClassIndexer::Cleanup() {
  // No other threads should be active at this point, but take the lock
  // just in case.
  absl::MutexLock lock(&mu_);

  classes_.RemoveAll();
  name_map_.clear();
}


void JvmClassIndexer::JvmtiOnClassPrepare(jclass cls) {
  jvmtiError err = JVMTI_ERROR_NONE;

  JvmtiBuffer<char> class_signature_buffer;
  err = jvmti()->GetClassSignature(
      cls,
      class_signature_buffer.ref(),
      nullptr);
  if (err != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "GetClassSignature failed, error: " << err;
    return;
  }

  if (class_signature_buffer.get() == nullptr) {
    LOG(ERROR) << "Class signature not available";
    return;
  }

  std::string class_signature(class_signature_buffer.get());

  // Try to insert the class into the set of all loaded classes. If it fails
  // the class was already discovered and no further action is necessary.
  jobject ref = nullptr;
  {
    absl::MutexLock lock(&mu_);

    std::pair<jobject, Empty>* inserted = nullptr;
    if (!classes_.Insert(cls, Empty(), &inserted)) {
      return;
    }

    ref = inserted->first;
    DCHECK(ref != nullptr);
  }

  std::string type_name =
      TypeNameFromJObjectSignature(class_signature_buffer.get());

  VLOG(1) << "Java class loaded, type name = " << type_name
          << ", signature: " << class_signature_buffer.get()
          << ", weak global reference to jclass: " << ref;

  {
    absl::MutexLock lock(&mu_);

    std::hash<std::string> string_hash;
    name_map_.insert(std::make_pair(string_hash(type_name), ref));
  }

  // Notify all interested parties that a new class has been prepared (i.e.
  // loaded and initialized). Invoke callbacks outside of any locks to prevent
  // potential deadlocks.
  on_class_prepared_.Fire(type_name, class_signature);
}

JniLocalRef JvmClassIndexer::FindClassBySignature(
    const std::string& class_signature) {
  std::hash<std::string> string_hash;
  return FindClassByHashCode(
      string_hash(TypeNameFromJObjectSignature(class_signature)),
      [class_signature](const std::string& signature) {
        return class_signature == signature;
      });
}

JniLocalRef JvmClassIndexer::FindClassByName(const std::string& class_name) {
  std::hash<std::string> string_hash;
  return FindClassByHashCode(
      string_hash(class_name), [class_name](const std::string& signature) {
        return class_name == TypeNameFromJObjectSignature(signature);
      });
}

JniLocalRef JvmClassIndexer::FindClassByHashCode(
    size_t hash_code,
    std::function<bool(const std::string&)> fn_check_signature) {
  jvmtiError err = JVMTI_ERROR_NONE;

  absl::MutexLock lock(&mu_);

  auto it = name_map_.lower_bound(hash_code);
  while ((it != name_map_.end()) && (it->first == hash_code)) {
    // Convert local reference to a weak reference.
    JniLocalRef ref;
    ref.reset(jni()->NewLocalRef(it->second));

    if (ref == nullptr) {
      // The class has been unloaded. We can remove it from the index now.
      classes_.Remove(it->second);
      it = name_map_.erase(it);
      continue;
    }

    ++it;

    JvmtiBuffer<char> class_signature_buffer;
    err = jvmti()->GetClassSignature(
        static_cast<jclass>(ref.get()),
        class_signature_buffer.ref(),
        nullptr);
    if (err != JVMTI_ERROR_NONE) {
      LOG(ERROR) << "GetClassSignature failed, error: " << err;
      continue;
    }

    if (fn_check_signature(class_signature_buffer.get())) {
      return ref;
    }
  }

  return nullptr;
}

std::shared_ptr<ClassIndexer::Type> JvmClassIndexer::GetPrimitiveType(
    JType type) {
  switch (type) {
    case JType::Void:
      return primitive_void_;

    case JType::Boolean:
      return primitive_boolean_;

    case JType::Char:
      return primitive_char_;

    case JType::Byte:
      return primitive_byte_;

    case JType::Short:
      return primitive_short_;

    case JType::Int:
      return primitive_int_;

    case JType::Long:
      return primitive_long_;

    case JType::Float:
      return primitive_float_;

    case JType::Double:
      return primitive_double_;

    case JType::Object:
      break;
  }

  DCHECK(false) << "Not a primitive type";
  return nullptr;
}

std::shared_ptr<ClassIndexer::Type> JvmClassIndexer::GetReference(
    const std::string& signature) {
  absl::MutexLock lock(&mu_);

  auto it = ref_cache_.find(signature);
  if (it != ref_cache_.end()) {
    std::shared_ptr<Type> type_reference = it->second.lock();
    if (type_reference != nullptr) {
      return type_reference;
    }

    ref_cache_.erase(it);
  }

  std::shared_ptr<Type> type_reference =
      std::make_shared<JvmClassReference>(this, signature);
  ref_cache_.insert(std::make_pair(signature, type_reference));
  return type_reference;
}

}  // namespace cdbg
}  // namespace devtools


