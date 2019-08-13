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

#include "string_evaluator.h"

#include "jni_utils.h"
#include "messages.h"
#include "model.h"

namespace devtools {
namespace cdbg {

// TODO: refactor this code to use auto-ref classes.

StringEvaluator::StringEvaluator(std::vector<jchar> string_content)
    : string_content_(std::move(string_content)),
      jstr_(nullptr) {
}


StringEvaluator::~StringEvaluator() {
  if (jstr_ != nullptr) {
    jni()->DeleteGlobalRef(jstr_);
    jstr_ = nullptr;
  }
}


bool StringEvaluator::Compile(
    ReadersFactory* readers_factory,
    FormatMessageModel* error_message) {
  DCHECK(jstr_ == nullptr);
  if (jstr_ != nullptr) {
    return false;
  }

  // Calling operator[0] on empty vector is not allowed.
  const jchar empty[] = { };
  const jchar* string_ptr = empty;
  if (!string_content_.empty()) {
    string_ptr = &string_content_[0];
  }

  // Create new Java string object (obtaining local reference).
  // TODO: wrap with exception checking.
  jobject jstr_local_ref = jni()->NewString(string_ptr, string_content_.size());

  if (!JniCheckNoException("StringEvaluator::Compile")) {
    return false;
  }

  if (jstr_local_ref == nullptr) {
    LOG(WARNING) << "Java string object could not be create";
    *error_message = { OutOfMemory };
    return false;
  }

  // Convert local reference to a global reference.
  jstr_ = static_cast<jstring>(jni()->NewGlobalRef(jstr_local_ref));
  jni()->DeleteLocalRef(jstr_local_ref);

  return true;
}


const JSignature& StringEvaluator::GetStaticType() const {
  static const JSignature string_signature = {
    JType::Object,
    kJavaStringClassSignature
  };

  return string_signature;
}


ErrorOr<JVariant> StringEvaluator::Evaluate(
    const EvaluationContext& evaluation_context) const {
  JVariant result;
  result.assign_new_ref(JVariant::ReferenceKind::Local, jstr_);

  return std::move(result);
}


}  // namespace cdbg
}  // namespace devtools

