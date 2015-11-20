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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_METHOD_CALL_RESULT_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_METHOD_CALL_RESULT_H_

#include "common.h"
#include "jvariant.h"
#include "messages.h"
#include "model.h"
#include "type_util.h"

namespace devtools {
namespace cdbg {

// Defines an outcome of calling a method using safe caller.
class MethodCallResult {
 public:
  enum class Type {
    // Method call failed due to an error (e.g. arguments mismatch or method
    // was blocked because it tried to modify a static field).
    Error,

    // The call completed. The called method threw an exception.
    JavaException,

    // The call completed with return value (which can be void).
    Success
  };

  MethodCallResult() {}

  // Builds "MethodCallResult" for each one of the "Type" options.
  static MethodCallResult Error(FormatMessageModel error);
  static MethodCallResult JavaException(jobject exception);
  static MethodCallResult Success(JVariant return_value);

  // Builds a "JavaException" result if there is a pending Java exception
  // or returns "Success" with value of void otherwise.
  static MethodCallResult PendingJniException();

  Type result_type() const { return result_type_; }

  const FormatMessageModel& error() const {
    DCHECK(result_type_ == Type::Error);
    return error_;
  }

  const jobject exception() const {
    DCHECK(result_type_ == Type::JavaException);

    jobject obj = nullptr;
    data_.get<jobject>(&obj);

    return obj;
  }

  FormatMessageModel format_exception() const {
    return {
      MethodCallExceptionOccurred,
      {
        TypeNameFromJObjectSignature(GetObjectClassSignature(exception()))
      }
    };
  }

  const JVariant& return_value() const {
    DCHECK(result_type_ == Type::Success);
    return data_;
  }

  static JVariant detach_return_value(MethodCallResult rc) {
    return std::move(rc.data_);
  }

  jobject return_ref() const {
    DCHECK((result_type_ == Type::Success) && (data_.type() == JType::Object));

    jobject obj = nullptr;
    if (!data_.get<jobject>(&obj)) {
      DCHECK(false);
    }

    return obj;
  }

 private:
  Type result_type_ { Type::Error };

  // Holds method return value or an exception object if "result_type_" is
  // either "JavaException" or "Success".
  JVariant data_;

  // Error details if "result_type_" is "Error".
  FormatMessageModel error_;
};


inline MethodCallResult MethodCallResult::Error(FormatMessageModel error) {
  MethodCallResult result;
  result.result_type_ = Type::Error;
  result.error_ = std::move(error);

  return result;
}


inline MethodCallResult MethodCallResult::JavaException(jobject exception) {
  MethodCallResult result;
  result.result_type_ = Type::JavaException;
  result.data_ = JVariant::GlobalRef(jni()->NewGlobalRef(exception));

  return result;
}


inline MethodCallResult MethodCallResult::PendingJniException() {
  JniLocalRef exception(jni()->ExceptionOccurred());
  if (exception == nullptr) {
    return MethodCallResult::Success(JVariant());
  }

  MethodCallResult rc(JavaException(exception.get()));
  jni()->ExceptionClear();
  return rc;
}


inline MethodCallResult MethodCallResult::Success(JVariant return_value) {
  // If the return value is an object, we need to make sure we don't keep
  // local reference. Local references will be discarded once
  // "JNIEnv::PopLocalFrame" is called and we want to persist the return
  // value for longer than that.
  return_value.change_ref_type(JVariant::ReferenceKind::Global);

  MethodCallResult result;
  result.result_type_ = Type::Success;
  result.data_ = std::move(return_value);

  return result;
}


}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_METHOD_CALL_RESULT_H_
