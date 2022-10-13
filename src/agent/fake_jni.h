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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_FAKE_JNI_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_FAKE_JNI_H_

#include <list>
#include <map>
#include <set>
#include <vector>

#include "class_metadata_reader.h"
#include "common.h"
#include "jvariant.h"


namespace devtools {
namespace cdbg {

class MockJNIEnv;
class MockJvmtiEnv;

// Implementation of fake JNI to simplify management of object references and
// decrease build time for simple unit tests (mock JNI takes 1 minute to
// compile).
class FakeJni {
 public:
  // Metadata of a fake Java method.
  struct MethodMetadata {
    // Fake method ID to be returned by GetClassMethods function.
    jmethodID id;

    // Method metadata.
    ClassMetadataReader::Method metadata;

    // Table of source line numbers and the corresponding statement address.
    std::vector<jvmtiLineNumberEntry> line_number_table;
  };

  // Metadata of a fake Java class.
  struct ClassMetadata {
    // File name corresponding to the object returned by "GetSourceFileName".
    std::string file_name;

    // Class signature.
    std::string signature;

    // Class generic signature.
    std::string generic;

    // List of methods that the class has.
    std::vector<MethodMetadata> methods;
  };

  // Built-in fake classes that unit tests don't need to setup.
  enum class StockClass {
    Object,
    String,
    StringArray,
    IntArray,
    BigDecimal,
    BigInteger,
    Iterable,
    Map,
    MapEntry,
    MyClass1,
    MyClass2,
    MyClass3
  };

  // Constructs fake JNI using internal JNI mock.
  FakeJni();

  // Constructs fake JNI using provided JNI mock.
  explicit FakeJni(MockJNIEnv* external_jni);

  // Constructs fake JNI using provided JVMTI mock.
  explicit FakeJni(
      MockJvmtiEnv* external_jvmti);

  // Constructs fake JNI using provided JNI and JVMTI mocks.
  explicit FakeJni(
      MockJvmtiEnv* external_jvmti,
      MockJNIEnv* external_jni);

  // Verifies that all references have been released.
  ~FakeJni();

  // Gets pointer to the mock JNI.
  JNIEnv* jni() const;

  // Gets pointer to the mock JVMTI.
  jvmtiEnv* jvmti() const;

  // Gets internal reference to built-in fake string class object. The caller
  // doesn't need to release the reference returned by "GetStockClass".
  jclass GetStockClass(StockClass stock_class);

  // Defines new fake class object and returns a new local reference to it.
  jclass CreateNewClass(const ClassMetadata& cls_metadata);

  // Gets class metadata that the caller can change. These changes will be
  // reflected in mocks.
  ClassMetadata& MutableClassMetadata(jclass cls);

  // Gets class metadata for predefined class. These changes will be
  // reflected in mocks.
  ClassMetadata& MutableStockClassMetadata(StockClass stock_class);

  // Gets the method metadata that the caller can change. These changes will
  // be reflected in mocks.
  MethodMetadata& MutableMethodMetadata(jmethodID method);

  // Searches class objects by its type signature
  // (for example: "Ljava/lang/String;"). If found a new local reference
  // is returned. Otherwise the function returns nullptr.
  jclass FindClassBySignature(const std::string& class_signature);

  // Searches class objects by its signature without 'L' and ';'
  // (for example: "java/lang/String"). If found a new local reference
  // is returned. Otherwise the function returns nullptr.
  jclass FindClassByShortSignature(const std::string& class_signature);

  // Creates new fake object and returns a local reference to it.
  jobject CreateNewObject(jclass cls);

  // Shortcut to CreateNewObject with stock class.
  jobject CreateNewObject(StockClass stock_class);

  // Creates a fake java.lang.String object with the specified content (ASCII
  // characters only) and returns local reference to it.
  jstring CreateNewJavaString(const std::string& content);

  // Creates a fake java.lang.String object with the specified Unicode content
  // and returns local reference to it.
  jstring CreateNewJavaString(std::vector<jchar> content);

  // Simulates reclaimed object through a weak global reference.
  void InvalidateObject(jobject ref);

 private:
  // Represents metadata of a fake Java object.
  struct FakeObject {
    // Object becomes invalid the moment it is not referenced.
    bool is_valid;

    // Reference count of the fake object.
    int reference_count;

    // Class of this object.
    FakeObject* cls { nullptr };

    // Simulate weak global reference to a garbage-collected object.
    bool reclaimed { false };
  };

  // Metadata of every fake reference.
  struct FakeRef {
    // Type of the reference (local, global or weak global).
    JVariant::ReferenceKind reference_kind;

    // Index into obj_.
    FakeObject* obj { nullptr };

    // Internals references are allowed to leak and can't be deleted.
    bool is_internal { false };
  };

  // Install mocks.
  void SetUp();

  // Install JNI mocks.
  void SetUpJniMocks();

  // Install JVMTI mocks.
  void SetUpJvmtiMocks();

  // Creates new reference to fake Java object.
  jobject CreateNewRef(
      JVariant::ReferenceKind reference_kind,
      FakeObject* obj);

  // Accesses the referenced object (verifying everything is valid).
  FakeObject* Dereference(jobject ref);

  // Accesses the referenced class metadata (verifying everything is valid).
  ClassMetadata& DereferenceClass(jclass ref);

  // Deletes the specified reference.
  void DeleteRef(JVariant::ReferenceKind reference_kind, jobject ref);

 private:
  // Internally used mock JVMTI.
  MockJvmtiEnv* const jvmti_;

  // Internally used mock JNI.
  MockJNIEnv* const jni_;

  // Flag determining whether this instance owns "jvmti_".
  const bool is_external_jvmti_;

  // Flag determining whether this instance owns "jni_".
  const bool is_external_jni_;

  // Maps metadata to each class object.
  std::map<FakeObject*, ClassMetadata> cls_;

  // Maps stock class to local reference to fake class object.
  std::map<StockClass, jclass> stock_;

  // List of fake Java objects.
  std::set<FakeObject*> obj_;

  // Map of all the allocated references.
  std::set<FakeRef*> refs_;

  // Buffers for fake Java string content.
  std::map<FakeObject*, std::vector<jchar>> jstring_data_;

  // Simulated pending exception (or nullptr if no exception).
  JniLocalRef pending_exception_;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_FAKE_JNI_H_

