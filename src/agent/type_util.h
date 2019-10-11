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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_TYPE_UTIL_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_TYPE_UTIL_H_

#include <memory>

#include "common.h"
#include "jni_utils.h"
#include "jvariant.h"
#include "model.h"

namespace devtools {
namespace cdbg {

constexpr char kJavaSignatureNotAvailable[] = "__JSIGNATURE_NOT_AVAILABLE__";

constexpr char kJavaObjectClassSignature[] = "Ljava/lang/Object;";
constexpr char kJavaStringClassSignature[] = "Ljava/lang/String;";

// Represents signature of an expression as it is known at compile time.
struct JSignature {
  // Basic data type.
  JType type;

  // If "type" is Object, "object_signature" may also indicate the actual
  // object type. The format of this string is as per Java specifications
  // (e.g. "Ljava/lang/Object;"). "object_signature" is optional and it will
  // be set to empty if the actual type is not known.
  std::string object_signature;
};

// Parsed signature of a Java method. This struct only conveys the arguments
// and the return type. The signature of a class that defined the method and
// method modifiers are not part of this structure.
struct JMethodSignature {
  // Return type of a method or nullptr if the method is void.
  JSignature return_type;

  // Method arguments.
  std::vector<JSignature> arguments;
};

// Java classes that receive a special treatment by Cloud Debugger. Rather than
// pass along and deciper their class signatures, the signature is analyzed
// once.
enum class WellKnownJClass : uint8 {
  Unknown,    // Any Java class that is not listed here
  String,     // java.lang.String
  Array       // Either primitive array or array of objects.
};


// Name-value pair with JVariant as a value.
struct NamedJVariant {
  // Creates nameless instance of "NamedJVariant" that only has
  // "status" set.
  static NamedJVariant ErrorStatus(FormatMessageModel description) {
    NamedJVariant instance;
    instance.status.is_error = true;
    instance.status.refers_to = StatusMessageModel::Context::VARIABLE_VALUE;
    instance.status.description = description;

    return instance;
  }

  static NamedJVariant InfoStatus(FormatMessageModel description) {
    NamedJVariant instance;
    instance.status.is_error = false;
    instance.status.refers_to = StatusMessageModel::Context::VARIABLE_VALUE;
    instance.status.description = description;

    return instance;
  }

  // Name associated with the JVariant value.
  std::string name;

  // Value.
  JVariant value;

  // If "value" is an object of a specially well treated type,
  // "well_known_jclass" will identify the "value" type.
  WellKnownJClass well_known_jclass { WellKnownJClass::Unknown };

  // Formatted error message explaining why the value could not be
  // captured.
  StatusMessageModel status;
};


// Determines JType enum based on Java type signature.
JType JTypeFromSignature(char signature_prefix);
JType JTypeFromSignature(const char* signature);
JType JTypeFromSignature(const std::string& signature);

// Extension of "JTypeFromSignature" that also fills in "object_signature" when
// "type" is JType::Object.
JSignature JSignatureFromSignature(const char* signature);
JSignature JSignatureFromSignature(const std::string& signature);

// Converts back "JSignature" to Java type signature.
std::string SignatureFromJSignature(JSignature signature);

// Parses Java method signature. Returns false if the signature format is
// unexpected.
bool ParseJMethodSignature(const std::string& signature,
                           JMethodSignature* result);

// Removes return type from method signature. For example: "(IIJ)I" will become
// "(IIJ)". If the method signature is corrupted, returns original string.
std::string TrimReturnType(const std::string& signature);

// Gets the well know Java class type from the signature. Returns "Unknown" if
// the signature represents a primitive type or an class not listed in
// "WellKnownJClass".
WellKnownJClass WellKnownJClassFromSignature(const JSignature& signature);

// Gets the type name from Java signature. Examples:
// 1. { JType::Object, "Lcom/MyClass;" } => "com.MyClass"
// 2. { JType::Object, "[[Llang/java/String;" } => "lang.Java.String[][]"
// 3. { JType::Boolean } => "boolean"
std::string TypeNameFromSignature(const JSignature& signature);

// Gets the type name from Java object (non-array) type signature. For example
// calling with "Lcom/MyClass;" will return "com.MyClass".
std::string TypeNameFromJObjectSignature(std::string signature);

// Trims the signature of Java object (non-array) type signature
// by erasing leading 'L' and trailing ';' characters.
// For example calling with "Lcom/MyClass;" will return "com/MyClass".
std::string TrimJObjectSignature(std::string signature);

// Converts JVMTI signature (e.g. "Lcom/MyClass;") to a binary name.
// Binary names are used in all JDK methods (like "Class.forName").
// Example of a binary name: "com.prod.MyClass$MyInnerClass".
std::string BinaryNameFromJObjectSignature(const std::string& signature);

// Check whether a class signature represents a Java array (either primitive
// array or array of objects).
bool IsArrayObjectSignature(const std::string& object_signature);

// Check whether a class signature represents a Java array (either primitive
// array or array of objects).
bool IsArrayObjectType(const JSignature& signature);

// Gets signature of array elements of the specified Java array object.
JSignature GetArrayElementJSignature(const JSignature& array_signature);

// Returns a nullable string with the extra argument descriptor appended at the
// end. If the method_descriptor doesn't have exactly one closing parenthesis
// (')'), the descriptor is treated as invalid and nullptr is returned.
Nullable<std::string> AppendExtraArgumentToDescriptor(
    const std::string& method_descriptor,
    const std::string& extra_argument_descriptor);

// Returns a nullable string with the extra argument descriptor prepended at the
// beginning. If the method_descriptor doesn't have exactly one opening
// parenthesis ('('), the descriptor is treated as invalid and nullptr is
// returned.
Nullable<std::string> PrependExtraArgumentToDescriptor(
    const std::string& method_descriptor,
    const std::string& instance_descriptor);

// Creates a path to the source file given the class signature and the source
// file name (without the directory name). This function supports two main
// cases:
// 1. Regular classes (for example: signature = "Lcom/prod/MyClass;", file
//    name = "MyClass.java"). The full path to the source file from the project
//    root is "com/prod/MyClass.java". The class name (MyClass) is supposed to
//    be identical to "class_file_name" without extension.
// 2. Nested or static classes (for example: signature =
//    "Lcom/prod/MyClass$MyInnerClass", name = "MyClass.java"). The full path
//    should be constructed as "com/prod/MyClass.java".
// This function builds the full path by removing the class names from
// "class_signature" and concatenating "class_file_name".
std::string ConstructFilePath(const char* class_signature,
                              const char* class_file_name);

// Checks whether the specified type is a Java boolean type.
inline bool IsBooleanType(JType type) {
  return type == JType::Boolean;
}


// Checks whether the specified type is one of Java integer types (byte,
// char, short, int or long).
inline bool IsIntegerType(JType type) {
  return (type == JType::Byte) ||
         (type == JType::Char) ||
         (type == JType::Short) ||
         (type == JType::Int) ||
         (type == JType::Long);
}


// Converts primitive type name to JType.
//  "boolean" => { JType::Boolean }
//  "int"  => { JType::Int }
inline bool PrimitiveTypeNameToJType(const std::string& type_name,
                                     JType* type) {
  if (type_name == "int") {
    *type = JType::Int;
    return true;
  } else if (type_name == "char") {
    *type = JType::Char;
    return true;
  } else if (type_name == "byte") {
    *type = JType::Byte;
    return true;
  } else if (type_name == "short") {
    *type = JType::Short;
    return true;
  } else if (type_name == "long") {
    *type = JType::Long;
    return true;
  } else if (type_name == "float") {
    *type = JType::Float;
    return true;
  } else if (type_name == "double") {
    *type = JType::Double;
    return true;
  } else if (type_name == "boolean") {
    *type = JType::Boolean;
    return true;
  }

  return false;
}

// Converts numeric type name to JType.
//  "int"  => { JType::Int }
inline bool NumericTypeNameToJType(const std::string& type_name, JType* type) {
  if ("boolean" == type_name) {
    return false;
  }

  return PrimitiveTypeNameToJType(type_name, type);
}

// Returns true if the specified type_name is a numeric type.
inline bool IsNumericTypeName(const std::string& type_name) {
  JType type;
  return NumericTypeNameToJType(type_name, &type);
}

// Returns true if the specified type_name is a Jtype.
inline bool IsNumericJType(JType type) {
  JSignature signature =  {type};
  const std::string& type_name = TypeNameFromSignature(signature);
  return IsNumericTypeName(type_name);
}


// Format array index ("[N]")
inline std::string FormatArrayIndexName(int i) {
  char str[20];
  snprintf(str, arraysize(str), "[%d]", i);
  str[arraysize(str) - 1] = '\0';

  return str;
}

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_TYPE_UTIL_H_


