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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_MESSAGES_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_MESSAGES_H_

#include <cstring>

namespace devtools {
namespace cdbg {

constexpr char ExpressionTooLong[] =
    "Expression is too long";

constexpr char ExpressionTreeTooDeep[] =
    "Expression is too complex";

constexpr char ExpressionSensitiveData[] =
    "Expression is not allowed for this breakpoint because it may expose "
    "sensitive data";

constexpr char ExpressionParserError[] =
    "Invalid expression syntax";

constexpr char BadNumericLiteral[] =
    "Invalid numeric literal $0";

constexpr char GeneralExpressionError[] =
    "Error occurred while parsing expression";

constexpr char TypeCastUnsupported[] =
    "Unsupported Java type cast from $0 to $1";

constexpr char TypeCastCompileInvalid[] =
    "Inconvertible types found: $0 required: $1";

constexpr char TypeCastEvaluateInvalid[] =
    "$0 cannot be cast to $1";

constexpr char ReferenceTypeNotFound[] = "Reference type required, found $0";

constexpr char TypeMismatch[] =
    "Type mismatch";

constexpr char OutOfMemory[] =
    "Out of memory";

constexpr char InvalidIdentifier[] =
    "Identifier $0 not found";

constexpr char ClassNotLoaded[] =
    "Java class $0 has not been loaded yet (Class signature is $1)";

constexpr char ClassLoadFailed[] =
    "Failed to load Java class $0";

constexpr char AmbiguousClassName2[] =
    "The identifier $0 is ambiguous (possible matches: $1, $2)";

constexpr char AmbiguousClassName3[] =
    "The identifier $0 is ambiguous (possible matches: $1, $2, $3)";

constexpr char AmbiguousClassName4OrMore[] =
    "The identifier $0 is ambiguous "
    "(possible matches: $1, $2, $3 and $4 more)";

constexpr char PrimitiveTypeField[] =
    "The primitive type $0 does not have a field $1";

constexpr char InstanceFieldNotFound[] =
    "Instance field $0 not found in class $1";

constexpr char StaticFieldNotFound[] =
    "Static field $0 not found in class $1";

constexpr char MethodCallOnPrimitiveType[] =
    "Cannot invoke $0 on the primitive type $1";

constexpr char MethodNotSafe[] =
    "Calling $0 is not allowed";

constexpr char MethodLoadQuotaExceeded[] =
    "Method execution uses too many classes, which can affect application "
    "performance";

constexpr char InterpreterQuotaExceeded[] =
    "Method executes too many instructions, which can affect application "
    "performance";

constexpr char MethodNotSafeAttemptedInstanceFieldChange[] =
    "Method $0 attempted to change instance field $2 of class $1";

constexpr char MethodNotSafeAttemptedArrayChange[] =
    "Method $0 is not safe (attempted to change an array)";

constexpr char MethodNotSafeCopyArrayTooLarge[] =
    "Method $0 blocked (attempted to copy $1 elements of an array)";

constexpr char MethodNotSafeNewArrayTooLarge[] =
    "Method $0 blocked (attempted to allocate $1 elements array)";

constexpr char MethodNotSafeAttemptedChangeStaticField[] =
    "Method $0 is not safe (attempted to change static field $2 of class $1)";

constexpr char NativeMethodNotSafe[] =
    "Native method call $0 is not allowed";

constexpr char OpcodeNotSupported[] =
    "Method $0 blocked ($1 instruction not supported)";

constexpr char StackOverflow[] =
    "Stack overflow";

constexpr char MethodCallExceptionOccurred[] =
    "An exception occurred: $0";

constexpr char InstanceMethodNotFound[] =
    "The instance method $0 is undefined for the type $1";

constexpr char StaticMethodNotFound[] =
    "The static method $0 is undefined for the type $1";

constexpr char ImplicitMethodNotFound[] =
    "The method $0 is undefined for the type $1";

constexpr char MethodCallArgumentsMismatchSingleCandidate[] =
    "The arguments do not match $0 parameters";

constexpr char MethodCallArgumentsMismatchMultipleCandidates[] =
    "The arguments do not match any of the $0 signatures";

constexpr char AmbiguousMethodCall[] =
    "Ambiguous method $0 call";

constexpr char ConditionNotBoolean[] =
    "Condition does not evaluate to boolean (return type: $0)";

constexpr char ObjectHasNoFields[] =
    "Object has no fields";

constexpr char InstanceFieldsOmitted[] =
    "Non-public fields of system classes omitted";

constexpr char EmptyCollection[] =
    "Empty collection";

constexpr char OutOfBufferSpace[] =
    "Buffer full. Use an expression to see more data";

constexpr char NullPointerDereference[] =
    "Null pointer dereference";

constexpr char DivisionByZero[] =
    "Division by zero";

constexpr char IntegerDivisionOverflow[] =
    "Integer division overflow";

constexpr char SnapshotExpired[] =
    "The snapshot has expired";

constexpr char LogpointExpired[] =
    "The logpoint has expired";

constexpr char ConditionEvaluationCostExceededPerBreakpointLimit[] =
    "Cancelled. The condition evaluation at this location might "
    "affect application performance. Please simplify the condition or move the "
    "location to a less frequently called statement.";

constexpr char ConditionEvaluationCostExceededGlobalLimit[] =
    "Cancelled. The condition evaluation cost for all active "
    "snapshots and logpoints might affect the application performance.";

constexpr char LocalCollectionNotAllItemsCaptured[] =
    "Only first $0 items were captured. "
    "Use in an expression to see more items.";

constexpr char ExpressionCollectionNotAllItemsCaptured[] =
    "Only first $0 items were captured.";

constexpr char FormatTrimmedLocalString[] =
    "length=$0; Use in an expression to see more data";

constexpr char FormatTrimmedExpressionString[] =
    "length=$0";

constexpr char DynamicLoggerNotAvailable[] =
    "Logpoints are not available";

constexpr char ArrayTypeExpected[] =
    "$0 is not an array";

constexpr char ArrayIndexNotInteger[] =
    "The array index $0 is not an integer";

constexpr char InvalidParameterIndex[] =
    "Invalid parameter $$$0";

constexpr char DynamicLogOutOfBytesQuota[] =
    "Logpoint is paused due to high byte rate until log quota is restored";

constexpr char DynamicLogOutOfCallQuota[] =
    "Logpoint is paused due to high call rate until log quota is restored";

constexpr char CanaryBreakpointUnhealthy[] =
    "The snapshot canary has failed and the snapshot cancelled. Please try "
    "again at a later time.";

#define INTERNAL_ERROR_MESSAGE (devtools::cdbg::FormatMessageModel { \
      "Internal error at $0:$1", \
      { \
        devtools::cdbg::ShortFileName(__FILE__), \
        STRINGIFY(__LINE__) \
      } \
    })

// Gets the file name portion from the full path.
inline const char* ShortFileName(const char* long_file_name) {
  if (long_file_name == nullptr) {
    return "";
  }

  const char* short_file_name = strrchr(long_file_name, '/');
  if (short_file_name == nullptr) {
    return long_file_name;
  }

  return short_file_name + 1;
}

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_MESSAGES_H_
