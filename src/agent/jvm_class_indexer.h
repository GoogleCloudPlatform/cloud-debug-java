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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_CLASS_INDEXER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_CLASS_INDEXER_H_

#include <list>
#include <map>

#include "class_indexer.h"
#include "common.h"
#include "jobject_map.h"
#include "mutex.h"

namespace devtools {
namespace cdbg {

// Listens for JVMTI notifications and maps a source file to a Java class.
class JvmClassIndexer : public ClassIndexer {
 public:
  JvmClassIndexer();

  // Indexes the classes already loaded into the JVM.
  void Initialize();

  // Releases all the resources before the class destruction.
  void Cleanup();

  // Indicates that a new class has been loaded and prepared.
  void JvmtiOnClassPrepare(jclass cls);

  OnClassPreparedEvent::Cookie SubscribeOnClassPreparedEvents(
      OnClassPreparedEvent::Callback fn) override {
    return on_class_prepared_.Subscribe(fn);
  }

  void UnsubscribeOnClassPreparedEvents(
      OnClassPreparedEvent::Cookie cookie) override {
    on_class_prepared_.Unsubscribe(std::move(cookie));
  }

  JniLocalRef FindClassBySignature(const std::string& class_signature) override;

  JniLocalRef FindClassByName(const std::string& class_name) override;

  std::shared_ptr<Type> GetPrimitiveType(JType type) override;

  std::shared_ptr<Type> GetReference(const std::string& signature) override;

 private:
  // Looks up the loaded class object by hash code of a class type name.
  // "fn_check_signature" is used to ignore class objects with colliding
  // hash code.
  JniLocalRef FindClassByHashCode(
      size_t hash_code,
      std::function<bool(const std::string&)> fn_check_signature);

 private:
  // We want to use JobjectMap as a set, so we map key to empty structure.
  struct Empty { };

  // Locks access to all data structures in this class.
  absl::Mutex mu_;

  // Keeps a set of loaded Java classes.
  JobjectMap<JObject_WeakRef, Empty> classes_;

  // Maps hash code of a type name of the class signature to the weak
  // reference to the Java class object. Using a type name also allows lookup
  // by either class signature (which can be converted to type name with
  // "TypeNameFromJObjectSignature" method).
  //
  // The lookup methods compare the class signature to handle the case
  // of hash code collision.
  std::multimap<size_t, jobject> name_map_;

  // Allows other objects to subscribe to OnClassPrepared event. This event is
  // fired when a new class has been prepared (i.e. loaded and initialized)
  // in JVM.
  OnClassPreparedEvent on_class_prepared_;

  // Primitive types.
  const std::shared_ptr<ClassIndexer::Type> primitive_void_;
  const std::shared_ptr<ClassIndexer::Type> primitive_boolean_;
  const std::shared_ptr<ClassIndexer::Type> primitive_byte_;
  const std::shared_ptr<ClassIndexer::Type> primitive_char_;
  const std::shared_ptr<ClassIndexer::Type> primitive_short_;
  const std::shared_ptr<ClassIndexer::Type> primitive_int_;
  const std::shared_ptr<ClassIndexer::Type> primitive_long_;
  const std::shared_ptr<ClassIndexer::Type> primitive_float_;
  const std::shared_ptr<ClassIndexer::Type> primitive_double_;

  // Cache of type references.
  // TODO: implement some sort of LRU cache to keep "Type" alive.
  std::map<std::string, std::weak_ptr<Type>> ref_cache_;

  DISALLOW_COPY_AND_ASSIGN(JvmClassIndexer);
};


}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_CLASS_INDEXER_H_
