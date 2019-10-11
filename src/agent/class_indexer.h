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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_CLASS_INDEXER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_CLASS_INDEXER_H_

#include "common.h"
#include "jni_utils.h"
#include "jvariant.h"
#include "observable.h"
#include "type_util.h"

namespace devtools {
namespace cdbg {

// Listens for JVMTI notifications and maps a type names of loaded classes
// to a Java class.
// This interface is thread safe.
class ClassIndexer {
 public:
  // References a single Java class caching results previous calls.
  class Type {
   public:
    virtual ~Type() {}

    // Gets the basic type (primitive or object).
    virtual JType GetType() const = 0;

    // Gets the JVMTI signature of the class (e.g. "Lcom/prod/MyClass$Inner").
    virtual const std::string& GetSignature() const = 0;

    // Finds the class object. Returns nullptr if the class hasn't been loaded
    // yet. The global reference to the class object is cached. The caller
    // does not own the returned reference.
    virtual jclass FindClass() = 0;

    // Searches field by name and signature. Returns nullptr if not found.
    // TODO: remove this method. It doesn't belong here.
    virtual jfieldID FindField(bool is_static, const std::string& name,
                               const std::string& signature) = 0;
  };

  // Event fired when a new class has been prepared in JVM (i.e. loaded and
  // initialized).
  typedef Observable<const std::string& /* type_name */,
                     const std::string& /* class_signature */>
      OnClassPreparedEvent;

  virtual ~ClassIndexer() { }

  // Subscribes to receive OnClassPrepared notifications.
  virtual OnClassPreparedEvent::Cookie SubscribeOnClassPreparedEvents(
      OnClassPreparedEvent::Callback fn) = 0;

  // Unsubscribes from OnClassPrepared notifications.
  virtual void UnsubscribeOnClassPreparedEvents(
      OnClassPreparedEvent::Cookie cookie) = 0;

  // Looks for a prepared Java class by class signature. A class is prepared
  // after it is first referenced and has its static fields initialized. If
  // the class is found, the function returns local reference to "jclass".
  virtual JniLocalRef FindClassBySignature(
      const std::string& class_signature) = 0;

  // Looks for a prepared Java class by fully qualified class name (e.g.
  // "com.google.util.SuperString.Nested"). A class is prepared after it is
  // first referenced and has its static fields initialized. If the class is
  // found, the function returns local reference to "jclass".
  virtual JniLocalRef FindClassByName(const std::string& class_name) = 0;

  // Gets reference to primitive type. The function returns "shared_ptr" for
  // consistency with "CreateReference".
  virtual std::shared_ptr<Type> GetPrimitiveType(JType type) = 0;

  // Creates a reference to the specified class. The reference can be resolved
  // into a class object. The lookup operation is cached. The cache can be
  // invalidated any time, hence the returned "shared_ptr".
  // The returned "Type" object should not outlive this instance.
  virtual std::shared_ptr<Type> GetReference(const std::string& signature) = 0;
};


// Gets reference to either a primitive type or a loaded class.
inline std::shared_ptr<ClassIndexer::Type> JSignatureToType(
    ClassIndexer* class_indexer,
    const JSignature& signature) {
  if (signature.type != JType::Object) {
    return class_indexer->GetPrimitiveType(signature.type);
  }

  return class_indexer->GetReference(signature.object_signature);
}

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_CLASS_INDEXER_H_
