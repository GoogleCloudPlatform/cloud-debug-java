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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_READERS_FACTORY_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_READERS_FACTORY_H_

#include <map>
#include <memory>

#include "class_metadata_reader.h"
#include "common.h"
#include "method_caller.h"

namespace devtools {
namespace cdbg {

class ArrayReader;
class ClassIndexer;
class LocalVariableReader;
class InstanceFieldReader;
class StaticFieldReader;
struct FormatMessageModel;

// Exposes JVM to expression evaluation through set of mockable interfaces.
//
// Compared to interfaces exposed by JvmEvaluator, this interface adds
// implicit context of evaluation point. The compiled Java expression binds
// to the local variables at the location where the expression is evaluated
// (this is typically location of a breakpoint). The "ReadersFactory"
// interface exposes local variables at compile time and locates types and
// static variables. The associated code location is stored in each instance
// of this interface and is hidden from the caller.
// TODO: rename to something that better suits this class purpose.
class ReadersFactory {
 public:
  virtual ~ReadersFactory() { }

  // Gets the class type name (not signature) of the current class.
  virtual std::string GetEvaluationPointClassName() = 0;

  // Finds Java class by name. Returns appropriate error if not found,
  // if class not loaded yet or if the name is ambiguous.
  virtual JniLocalRef FindClassByName(const std::string& class_name,
                                      FormatMessageModel* error_message) = 0;

  // Checks whether an object of "from_signature" class can be assigned to
  // "to_signature" class without explicit casting. For example
  // "java.lang.String" is assignable to "java.lang.Object". If any of these
  // classes haven't been indexed yet, returns false.
  virtual bool IsAssignable(const std::string& from_signature,
                            const std::string& to_signature) = 0;

  // Creates the object capable of reading the value of the specified local
  // variable when the expression is going to be evaluated. The caller owns the
  // returned instance. Returns null if no local variable with this name exist
  // in the context represented by "ReadersFactory".
  virtual std::unique_ptr<LocalVariableReader> CreateLocalVariableReader(
      const std::string& variable_name, FormatMessageModel* error_message) = 0;

  // Factory method for reader of "this" local variable. While it is possible
  // to locate "this" by calling CreateLocalVariableReader("this"), that is not
  // the correct way of obtaining local instance reference.
  virtual std::unique_ptr<LocalVariableReader> CreateLocalInstanceReader() = 0;

  // Creates the object to read class instance variable. "class_signature"
  // determines the Java class. The caller owns the returned instance. Returns
  // null if the specified class does not have instance variable "field_name".
  virtual std::unique_ptr<InstanceFieldReader> CreateInstanceFieldReader(
      const std::string& class_signature, const std::string& field_name,
      FormatMessageModel* error_message) = 0;

  // Creates the object to read static field from the current evaluation point.
  // For example if the expression is evaluated for breakpoint in method f in
  // some class X, this function will search for a static field "field_name" in
  // class X. The caller owns the returned instance. Returns nullptr if no
  // static field "field_name" is found in the class containing the evaluation
  // point.
  virtual std::unique_ptr<StaticFieldReader> CreateStaticFieldReader(
      const std::string& field_name, FormatMessageModel* error_message) = 0;

  // Creates the object to read static field from the specified class.
  // "class_name" is a fully qualified name (like "com.my.Green"). Unlike
  // the overloaded version of this function, the evaluation point doesn't
  // matter here. Returns nullptr if no static field "field_name" is found in
  // "class_signature" class.
  virtual std::unique_ptr<StaticFieldReader> CreateStaticFieldReader(
      const std::string& class_name, const std::string& field_name,
      FormatMessageModel* error_message) = 0;

  // Finds signatures of all local instance methods named "method_name" in
  // the "this".
  virtual std::vector<ClassMetadataReader::Method> FindLocalInstanceMethods(
      const std::string& method_name) = 0;

  // Finds signatures of all instance methods named "method_name" in the
  // specified class.
  // Returns false if the class for class_signature is not loaded, true
  // otherwise.
  virtual bool FindInstanceMethods(
      const std::string& class_signature, const std::string& method_name,
      std::vector<ClassMetadataReader::Method>* methods,
      FormatMessageModel* error_message) = 0;

  // Finds signatures of all static methods named "method_name" in the current
  // class.
  virtual std::vector<ClassMetadataReader::Method> FindStaticMethods(
      const std::string& method_name) = 0;

  // Finds signatures of all static methods named "method_name" in the current
  // class. Populates "MethodCaller" instances for each such method.
  // Returns false if the class for class_name is not loaded, true otherwise.
  virtual bool FindStaticMethods(
      const std::string& class_name, const std::string& method_name,
      std::vector<ClassMetadataReader::Method>* methods,
      FormatMessageModel* error_message) = 0;

  // Creates an object to read native array in expression evaluation. Returns
  // nullptr if "array_signature" doesn't correspond to an array.
  virtual std::unique_ptr<ArrayReader> CreateArrayReader(
      const JSignature& array_signature) = 0;
};


// Defines the JVM specific parameters that define code context (i.e.
// location) in which the variables are being evaluated and methods are
// caller.
// TODO: move this struct elsewhere.
struct EvaluationContext {
  // Java thread in which the expression is being evaluated.
  jthread thread = nullptr;

  // Call frame in which the expression is being evaluated. The value of "0"
  // means topmost frame (the function executing right now). "1" is the
  // function that called the current function, and so on.
  jint frame_depth = 0;

  // Invokes methods referenced in an expression. Keeps quota to limit the
  // complexity of the interpreted methods.
  MethodCaller* method_caller = nullptr;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_READERS_FACTORY_H_


