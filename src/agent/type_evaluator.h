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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_TYPE_EVALUATOR_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_TYPE_EVALUATOR_H_

#include "class_metadata_reader.h"
#include "common.h"
#include "method_caller.h"

namespace devtools {
namespace cdbg {

struct NamedJVariant;

// Default settings limiting the amount of data we capture for collection.
constexpr int kMaxCaptureExpressionElements = 200;
constexpr int kMaxCapturePrimitiveElements = 100;
constexpr int kMaxCaptureObjectElements = 10;

// Captures the content of a Java object. This can be either enumeration of
// all the fields or type specific formatting. For example "java.util.HashMap"
// type is very hard to understand if looking at its members and developers
// expect it to be represented as a list of key-value pairs.
//
// Instances of this interfaces are thread safe.
class TypeEvaluator {
 public:
  virtual ~TypeEvaluator() {}

  // Gets the name of this pretty evaluator. Only used for unit tests and
  // diagnostics.
  virtual std::string GetEvaluatorName() = 0;

  // Reads all the object's fields. "status" may be set to information or error
  // message. Example of such a message is: "only first 10 elements out of 1578
  // were captured". "method_caller" holds the method evaluation policy and
  // keeps track of method evaluation quota. Some type evaluators don't need
  // the "method_caller". The "method_caller" is not stored beyond immediate
  // call to "Evaluate".
  virtual void Evaluate(
      MethodCaller* method_caller,
      const ClassMetadataReader::Entry& class_metadata,
      jobject obj,
      bool is_watch_expression,
      std::vector<NamedJVariant>* members) = 0;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_TYPE_EVALUATOR_H_

