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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_CLASS_METADATA_READER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_CLASS_METADATA_READER_H_

#include <functional>
#include <memory>
#include <set>
#include "common.h"
#include "class_metadata_reader.h"
#include "jobject_map.h"
#include "jvariant.h"
#include "mutex.h"

namespace devtools {
namespace cdbg {

// Loads and cache class metadata. This includes class signature and its fields.
// This class is thread safe.
class JvmClassMetadataReader : public ClassMetadataReader {
 public:
  // Callback to filter out fields and methods that should not be visible by
  // the debugger.
  class MemberVisibilityPolicy {
   public:
    virtual ~MemberVisibilityPolicy() {}

    // Returns false if the specified field should be filtered out or true
    // to display it to the end user.
    // Possible bit values for "field_modifiers" are defined in
    // classfile_constants.h (e.g. JVM_ACC_PUBLIC).
    virtual bool IsFieldDebuggerVisible(
        jclass cls,
        const string& class_signature,
        jint field_modifiers,
        const string& field_name,
        const string& field_signature) = 0;

    // Returns false if the specified method should be filtered out or true
    // to display it to the end user.
    // Possible bit values for "method_modifiers" are defined in
    // classfile_constants.h (e.g. JVM_ACC_PUBLIC).
    virtual bool IsMethodDebuggerVisible(
        jclass cls,
        const string& class_signature,
        jint method_modifiers,
        const string& method_name,
        const string& method_signature) = 0;
  };

  // "field_visibility_policy" is not owned by this class and must outlive it.
  explicit JvmClassMetadataReader(
      MemberVisibilityPolicy* member_visibility_policy);

  ~JvmClassMetadataReader() override;

  // Loads metadata of Java class "cls" or retrieves it from cache.
  const Entry& GetClassMetadata(jclass cls) override;

 private:
  // Loads metadata of Java class and its superclasses into "metadata". The
  // function assumes previously uninitialized structure.
  void LoadClassMetadata(jclass cls, Entry* metadata);

  // Loads metadata of all implemented interfaces of a class.
  void LoadImplementedInterfacesMetadata(
      jclass parent,
      std::set<std::pair<string, string>>* registered_methods,
      Entry* metadata);

  // Loads metadata of a single Java class ignoring overloaded methods.
  void LoadSingleClassMetadata(
      jclass cls,
      std::set<std::pair<string, string>>* registered_methods,
      Entry* metadata);

  // Loads class field and appends it to the appropriate list in "metadata".
  // In case of error, the field is skipped and "metadata" is not changed.
  void LoadFieldInfo(
      jclass cls,
      const string& class_signature,
      jfieldID field_id,
      Entry* metadata);

  // Loads metadata of a method. In case of error returns "Method" with empty
  // name.
  Method LoadMethodInfo(
      jclass cls,
      const string& class_signature,
      jmethodID method_id);

 private:
  // Optional callback to decide whether a particular field or a method should
  // be visible. The visibility applies to object exploration in
  // "variables_table" and to expressions. If nullptr, all fields and methods
  // are visible.
  // Not owned by this class.
  MemberVisibilityPolicy* const member_visibility_policy_;

  // Locks access to object fields cache.
  Mutex mu_;

  // Cache of fields in classes we evaluated so far.
  JobjectMap<JObject_WeakRef, Entry> cls_cache_;

  DISALLOW_COPY_AND_ASSIGN(JvmClassMetadataReader);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_CLASS_METADATA_READER_H_

