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

#include "type_util.h"

#include <algorithm>

namespace devtools {
namespace cdbg {

static Nullable<std::string> InsertExtraArgumentIntoDescriptor(
    const std::string& descriptor, size_t pos,
    const std::string& extra_argument_descriptor) {
  return Nullable<std::string>(
      std::string(descriptor).insert(pos, extra_argument_descriptor));
}

JType JTypeFromSignature(char signature_prefix) {
  switch (signature_prefix) {
    case 'V':
      return JType::Void;

    case 'Z':
      return JType::Boolean;

    case 'C':
      return JType::Char;

    case 'B':
      return JType::Byte;

    case 'S':
      return JType::Short;

    case 'I':
      return JType::Int;

    case 'J':
      return JType::Long;

    case 'F':
      return JType::Float;

    case 'D':
      return JType::Double;

    case '[':  // array, which in Java is also a kind of object.
    case 'L':
      return JType::Object;

    default:
      LOG(ERROR) << "Invalid Java object signature "
                 << static_cast<int>(signature_prefix);
      return JType::Void;  // Just because we need to return something.
  }
}


JType JTypeFromSignature(const char* signature) {
  if (signature == nullptr) {
    return JTypeFromSignature('\0');
  }

  return JTypeFromSignature(signature[0]);
}

JType JTypeFromSignature(const std::string& signature) {
  if (signature.empty()) {
    return JTypeFromSignature('\0');
  }

  return JTypeFromSignature(signature[0]);
}

JSignature JSignatureFromSignature(const char* signature) {
  const JType type = JTypeFromSignature(signature);

  if (type == JType::Object) {
    return { type, signature };
  } else {
    return { type };
  }
}

JSignature JSignatureFromSignature(const std::string& signature) {
  const JType type = JTypeFromSignature(signature);

  if (type == JType::Object) {
    return { type, signature };
  } else {
    return { type };
  }
}

std::string SignatureFromJSignature(JSignature signature) {
  switch (signature.type) {
    case JType::Void:
      return "V";

    case JType::Boolean:
      return "Z";

    case JType::Char:
      return "C";

    case JType::Byte:
      return "B";

    case JType::Short:
      return "S";

    case JType::Int:
      return "I";

    case JType::Long:
      return "J";

    case JType::Float:
      return "F";

    case JType::Double:
      return "D";

    default:
      return std::move(signature.object_signature);
  }
}

bool ParseJMethodSignature(const std::string& signature,
                           JMethodSignature* result) {
  // The signature has the following format (arguments)return_type.
  *result = JMethodSignature();

  std::string::const_iterator it = signature.begin();

  // Skip the opening parenthesis of the arguments list.
  if ((it == signature.end()) || (*it != '(')) {
    return false;
  }

  ++it;

  // Parse arguments.
  while ((it != signature.end()) && (*it != ')')) {
    JType argument_type = JTypeFromSignature(*it);
    if (argument_type == JType::Void) {
      return false;  // Bad type or unexpected 'void'.
    }

    // Primitive types.
    if (argument_type != JType::Object) {
      result->arguments.push_back({ argument_type });
      ++it;
      continue;
    }

    // Array of either a primitive type or array objects.
    if (*it == '[') {
      auto it_type = it + 1;
      while ((it_type != signature.end()) && (*it_type == '[')) {
        ++it_type;
      }

      if (it_type == signature.end()) {
        return false;
      }

      JType element_type = JTypeFromSignature(*it_type);
      if (element_type == JType::Void) {
        return false;  // Bad type or unexpected 'void'.
      }

      if (element_type != JType::Object) {
        result->arguments.push_back(
            {JType::Object, std::string(it, it_type + 1)});

        it = it_type + 1;
        continue;
      }
    }

    // Object type or array of objects. Either way the type is terminated
    // by ';'.
    auto it_next_argument = std::find(it + 1, signature.end(), ';');
    if (it_next_argument == signature.end()) {
      // Couldn't find termination character for the type name.
      return false;
    }

    ++it_next_argument;

    result->arguments.push_back(
        {argument_type, std::string(it, it_next_argument)});

    it = it_next_argument;
  }

  if ((it == signature.end()) || (it + 1 == signature.end())) {
    // Return type not present in the signature.
    return false;
  }

  // Skip the closing parenthesis.
  ++it;

  const JType return_type = JTypeFromSignature(*it);

  if (return_type == JType::Object) {
    result->return_type = {return_type, std::string(it, signature.end())};
  } else {
    result->return_type = { return_type };
  }

  return true;
}

std::string TrimReturnType(const std::string& signature) {
  if (signature.empty() || (signature[0] != '(')) {
    return signature;  // Error, return original signature.
  }

  size_t pos = signature.find_last_of(')');
  if (pos == std::string::npos) {
    return signature;  // Error, return original signature.
  }

  return signature.substr(0, pos + 1);
}

WellKnownJClass WellKnownJClassFromSignature(const JSignature& signature) {
  if (IsArrayObjectType(signature)) {
    return WellKnownJClass::Array;
  }

  if ((signature.type == JType::Object) &&
      (signature.object_signature == kJavaStringClassSignature)) {
    return WellKnownJClass::String;
  }

  return WellKnownJClass::Unknown;
}

bool IsArrayObjectSignature(const std::string& object_signature) {
  return !object_signature.empty() && (object_signature[0] == '[');
}

bool IsArrayObjectType(const JSignature& signature) {
  return (signature.type == JType::Object) &&
         IsArrayObjectSignature(signature.object_signature);
}


JSignature GetArrayElementJSignature(const JSignature& array_signature) {
  DCHECK(IsArrayObjectType(array_signature)) << "Array expected";

  const std::string& object_signature = array_signature.object_signature;
  auto it_begin = object_signature.begin();
  auto it_end = object_signature.end();

  // The array signature has a '[' prefix that we should remove to obtain
  // array element signature.
  if (it_end - it_begin < 2) {
    return { JType::Void };  // Invalid input.
  }

  ++it_begin;

  const JType element_type = JTypeFromSignature(*it_begin);

  if (element_type == JType::Object) {
    return {element_type, std::string(it_begin, it_end)};
  } else {
    return { element_type };
  }
}

Nullable<std::string> AppendExtraArgumentToDescriptor(
    const std::string& method_descriptor,
    const std::string& extra_argument_descriptor) {
  size_t arguments_end_pos = method_descriptor.find(')');
  if (arguments_end_pos == std::string::npos) {
    return Nullable<std::string>();
  }

  // We now assume that the descriptor is well constructed. If it is not, then
  // an internal error will be reported from the java side.
  return InsertExtraArgumentIntoDescriptor(
      method_descriptor,
      arguments_end_pos,
      extra_argument_descriptor);
}

Nullable<std::string> PrependExtraArgumentToDescriptor(
    const std::string& method_descriptor,
    const std::string& instance_descriptor) {
  size_t arguments_end_pos = method_descriptor.find('(');
  if (arguments_end_pos == std::string::npos) {
    return Nullable<std::string>();
  }

  // We now assume that the descriptor is well constructed. If it is not, then
  // an internal error will be reported from the java side.
  return InsertExtraArgumentIntoDescriptor(
      method_descriptor,
      arguments_end_pos + 1,
      instance_descriptor);
}

std::string TypeNameFromSignature(const JSignature& signature) {
  switch (signature.type) {
    case JType::Void:
      return "void";

    case JType::Boolean:
      return "boolean";

    case JType::Byte:
      return "byte";

    case JType::Char:
      return "char";

    case JType::Short:
      return "short";

    case JType::Int:
      return "int";

    case JType::Long:
      return "long";

    case JType::Float:
      return "float";

    case JType::Double:
      return "double";

    case JType::Object:
      if (signature.object_signature.empty()) {
        return "java.lang.Object";
      }

      if (IsArrayObjectType(signature)) {
        return TypeNameFromSignature(
            GetArrayElementJSignature(signature)) + "[]";
      }

      return TypeNameFromJObjectSignature(signature.object_signature);
  }

  return std::string();
}

std::string TypeNameFromJObjectSignature(std::string object_signature) {
  if (object_signature.empty()) {
    return object_signature;
  }

  DCHECK_NE('[', object_signature[0])
      << "Arrays not supported in this function";

  //
  // Read from "source" and write to "target" as we go.
  //

  // TODO: add support for non-ASCII UTF-8 encoded names.

  std::string::const_iterator source = object_signature.begin();
  if (*source == 'L') {
    ++source;
  }

  std::string::iterator target = object_signature.begin();

  while (source != object_signature.end()) {
    // ';' is a suffix appended at the end of class signature. We don't need
    // to include it in the type name.
    if (*source == ';') {
      ++source;
      break;
    }

    // The signature of anonymous classes looks as following:
    //     "Lcom/prod/MyClass$1"
    // while the signature of inner and static classes is:
    //     "Lcom/prod/MyClass$InnerOrStaticClass"
    // Similar to the user experience of Eclipse, we want to translate
    // anonymous class signature to "com.prod.MyClass$1", and inner or
    // static class to "com.prod.MyClass.InnerOrStaticClass". To achieve this
    // figure out whether the name following the '$' sign starts with a digit.
    //
    // In Scala, the signature of a singleton object looks like this:
    //     "Lcom/prod/MyClassObject$;"
    // And we want to display it as "com.prod.MyClassObject$" rather than
    // "com.prod.MyClassObject.", i.e., without replacing the final '$' that
    // comes just before a semicolon. Note that nested singletons do not cause
    // trouble, as they are put inside the corresponding class, and hence,
    // we don't get "MyOuterClassObject$$MyInnerClassObject$", but instead, we
    // just get "MyOuterClassObject$MyInnerClassObject$".

    char ch = *source;

    if (ch == '/') {
      ch = '.';
    } else if (ch == '$') {
      if (source + 1 == object_signature.end()) {
        ch = '.';
      } else if (*(source + 1) == '$') {
        // Inner class inside Scala singleton. Pass the $ character as is.
        // E.g., "Lcom/prod/MyClassObject$$anonfun$1"
      } else if (isdigit(*(source + 1))) {
        // Anonymous class. Pass the $ character as is.
        // E.g., "Lcom/prod/MyClass$1;"
      } else if (*(source + 1) == ';') {
        // Scala singleton object. Pass the $ character as is.
        // E.g., "Lcom/prod/MyClassObject$;"
      } else {
        ch = '.';
      }
    }

    *target = ch;

    ++source;
    ++target;
  }

  object_signature.resize(target - object_signature.begin());

  return object_signature;
}

std::string TrimJObjectSignature(std::string object_signature) {
  if (object_signature.empty()) {
    return object_signature;
  }

  DCHECK_NE('[', object_signature[0])
      << "Arrays are not supported in this function";

  if (object_signature[0] == 'L') {
    object_signature.erase(0, 1);
  }

  // The string might become empty if we removed leading character
  if (!object_signature.empty() && (*object_signature.rbegin() == ';')) {
    object_signature.pop_back();
  }

  return object_signature;
}

std::string BinaryNameFromJObjectSignature(const std::string& signature) {
  if (signature.size() < 2) {
    return signature;
  }

  std::string binary_name;
  if (signature.front() == '[') {
    // This is an array class. Binary names for array classes are identical
    // to JVMTI signature with the exception that '/' is replaced with '.'.
    // Note that primitive array classes (e.g. "[B") follow the same rule here.
    binary_name = signature;
  } else {
    // Skip over the 'L' character and the last ';'. They don't get included
    // in a binary name.
    binary_name = signature.substr(1, signature.size() - 2);
  }

  std::replace(binary_name.begin(), binary_name.end(), '/', '.');

  return binary_name;
}

std::string ConstructFilePath(const char* class_signature,
                              const char* class_file_name) {
  if (*class_signature == '\0') {
    return class_file_name;
  }

  int class_file_name_length = strlen(class_file_name);

  const char* class_path_first = class_signature;
  const char* class_path_last = class_signature + strlen(class_signature) - 1;

  if (*class_path_first == 'L') {
    ++class_path_first;
  }

  if (*class_path_last == ';') {
    --class_path_last;
  }

  // Not expecting arrays here.
  if (*class_path_first == '[') {
    return class_file_name;
  }

  if (class_path_last < class_path_first) {
    // The class is in the package root (e.g. "LMyClass;")
    return class_file_name;
  }

  // Search for the package path without the class name. Inner classes are
  // separated by '$' and this search will skip them (which we want to do since
  // inner classes are defined in their parent source file).
  const char* package_last = class_path_last;
  while ((*package_last != '/') && (package_last != class_path_first)) {
    --package_last;
  }

  if (package_last == class_path_first) {
    // The class is in the package root (e.g. "LMyClass;")
    return class_file_name;
  }

  const char* package_end = package_last + 1;

  std::string path;

  int reserve_size = (package_end - class_path_first) + class_file_name_length;
  path.reserve(reserve_size);

  path.append(class_path_first, package_end);
  path.append(class_file_name, class_file_name_length);

  DCHECK_EQ(reserve_size, path.size());

  return path;
}

}  // namespace cdbg
}  // namespace devtools
