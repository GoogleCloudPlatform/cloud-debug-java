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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_CLASS_METADATA_READER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_CLASS_METADATA_READER_H_

#include <memory>
#include <vector>

#include "common.h"
#include "type_util.h"

namespace devtools {
namespace cdbg {

class InstanceFieldReader;
class StaticFieldReader;

// Loads and cache class metadata. This includes class signature and its fields.
// This interface is thread safe.
class ClassMetadataReader {
 public:
  // Cached Java method metadata.
  struct Method {
    // Signature of a class that defined the method.
    JSignature class_signature;

    // Name of the method (without arguments and return type).
    std::string name;

    // Java method signature. Argument types and return type can be deduced
    // from the signature.
    std::string signature;

    // Method modifiers. The most important is JVM_ACC_STATIC to distinguish
    // instance methods from static methods.
    jint modifiers { 0 };

    bool is_static() const { return (modifiers & JVM_ACC_STATIC) != 0; }
  };

  // Cached Java class metadata (including inherited classes).
  struct Entry {
    // Class signature.
    JSignature signature;

    // List of instance (non-static) class fields (aka member variables). Some
    // fields might be omitted due to external policy.
    std::vector<std::unique_ptr<InstanceFieldReader>> instance_fields;

    // List of static fields. Some fields might be omitted due to external
    // policy.
    std::vector<std::unique_ptr<StaticFieldReader>> static_fields;

    // List of instance and static methods of this class. Inherited methods
    // are also included. This list includes all methods, even methods that
    // the debugger is not allowed to invoke from expressions. The decision
    // whether a method is safe for calling is not a responsibility of this
    // class.
    std::vector<Method> methods;

    // Indicates whether one or more instance fields were filtered out due
    // to field visibility policy.
    bool instance_fields_omitted { false };
  };

  virtual ~ClassMetadataReader() { }

  // Loads metadata of Java class "cls" or retrieves it from cache.
  virtual const Entry& GetClassMetadata(jclass cls) = 0;
};


// Helper method to build ClassMetadataReader::Method for instance method.
inline ClassMetadataReader::Method InstanceMethod(
    std::string class_signature, std::string method_name,
    std::string method_signature) {
  ClassMetadataReader::Method metadata;
  metadata.class_signature = { JType::Object, std::move(class_signature) };
  metadata.name = std::move(method_name);
  metadata.signature = std::move(method_signature);

  DCHECK(!metadata.is_static());

  return metadata;
}

// Helper method to build ClassMetadataReader::Method for instance method.
inline ClassMetadataReader::Method StaticMethod(std::string class_signature,
                                                std::string method_name,
                                                std::string method_signature) {
  ClassMetadataReader::Method metadata;
  metadata.class_signature = { JType::Object, std::move(class_signature) };
  metadata.name = std::move(method_name);
  metadata.signature = std::move(method_signature);
  metadata.modifiers = JVM_ACC_STATIC;

  DCHECK(metadata.is_static());

  return metadata;
}

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_CLASS_METADATA_READER_H_

