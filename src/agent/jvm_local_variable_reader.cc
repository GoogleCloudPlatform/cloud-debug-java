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

#include "jvm_local_variable_reader.h"

#include "messages.h"
#include "readers_factory.h"

namespace devtools {
namespace cdbg {

JvmLocalVariableReader::JvmLocalVariableReader(
    const jvmtiLocalVariableEntry& entry,
    bool is_argument,
    bool is_read_error,
    const FormatMessageModel& read_error)
    : is_argument_(is_argument),
      name_(entry.name ? entry.name : ""),
      signature_(JSignatureFromSignature(entry.signature)),
      start_location_(entry.start_location),
      section_length_(entry.length),
      slot_(entry.slot),
      is_read_error_(is_read_error),
      read_error_(read_error) {
}


JvmLocalVariableReader::JvmLocalVariableReader(
    const JvmLocalVariableReader& source)
    : is_argument_(source.is_argument_),
      name_(source.name_),
      signature_(source.signature_),
      start_location_(source.start_location_),
      section_length_(source.section_length_),
      slot_(source.slot_),
      is_read_error_(source.is_read_error_),
      read_error_(source.read_error_) {
}


std::unique_ptr<LocalVariableReader> JvmLocalVariableReader::Clone() const {
  return std::unique_ptr<LocalVariableReader>(
      new JvmLocalVariableReader(*this));
}

bool JvmLocalVariableReader::ReadValue(
    const EvaluationContext& evaluation_context,
    JVariant* result,
    FormatMessageModel* error) const {
  if (is_read_error_) {
    *error = read_error_;
    return false;
  }

  jvmtiError err = JVMTI_ERROR_NONE;

  switch (signature_.type) {
    case JType::Void:
      *error = INTERNAL_ERROR_MESSAGE;
      LOG(ERROR) << "'void' type is unexpected";
      return false;

    case JType::Boolean:
    case JType::Char:
    case JType::Byte:
    case JType::Short:
    case JType::Int: {
      jint value = 0;
      err = jvmti()->GetLocalInt(
          evaluation_context.thread,
          evaluation_context.frame_depth,
          slot_,
          &value);
      if (err != JVMTI_ERROR_NONE) {
        *error = INTERNAL_ERROR_MESSAGE;
        LOG(ERROR) << "GetLocalInt failed, error: " << err;
        return false;
      }

      switch (signature_.type) {
        case JType::Boolean:
          *result = JVariant::Boolean(static_cast<jboolean>(value));
          return true;

        case JType::Char:
          *result = JVariant::Char(static_cast<jchar>(value));
          return true;

        case JType::Byte:
          *result = JVariant::Byte(static_cast<jbyte>(value));
          return true;

        case JType::Short:
          *result = JVariant::Short(static_cast<jshort>(value));
          return true;

        case JType::Int:
          *result = JVariant::Int(value);
          return true;

        default:
          *error = INTERNAL_ERROR_MESSAGE;
          return false;
      }
    }

    case JType::Long: {
      jlong value = 0;
      err = jvmti()->GetLocalLong(
          evaluation_context.thread,
          evaluation_context.frame_depth,
          slot_,
          &value);
      if (err != JVMTI_ERROR_NONE) {
        *error = INTERNAL_ERROR_MESSAGE;
        LOG(ERROR) << "GetLocalLong failed, error: " << err;
        return false;
      }

      *result = JVariant::Long(value);
      return true;
    }

    case JType::Float: {
      jfloat value = 0;
      err = jvmti()->GetLocalFloat(
          evaluation_context.thread,
          evaluation_context.frame_depth,
          slot_,
          &value);
      if (err != JVMTI_ERROR_NONE) {
        *error = INTERNAL_ERROR_MESSAGE;
        LOG(ERROR) << "GetLocalFloat failed, error: " << err;
        return false;
      }

      *result = JVariant::Float(value);
      return true;
    }

    case JType::Double: {
      jdouble value = 0;
      err = jvmti()->GetLocalDouble(
          evaluation_context.thread,
          evaluation_context.frame_depth,
          slot_,
          &value);
      if (err != JVMTI_ERROR_NONE) {
        *error = INTERNAL_ERROR_MESSAGE;
        LOG(ERROR) << "GetLocalDouble failed, error: " << err;
        return false;
      }

      *result = JVariant::Double(value);
      return true;
    }

    case JType::Object: {
      jobject local_ref = nullptr;
      err = jvmti()->GetLocalObject(
          evaluation_context.thread,
          evaluation_context.frame_depth,
          slot_,
          &local_ref);
      if (err != JVMTI_ERROR_NONE) {
        *error = INTERNAL_ERROR_MESSAGE;
        LOG(ERROR) << "GetLocalObject failed, error: " << err;
        return false;
      }

      // Attach local reference to JVariant (without calling JNI::NewLocalRef).
      result->attach_ref(JVariant::ReferenceKind::Local, local_ref);

      return true;
    }
  }

  // Logic flow should not reach this point.
  *error = INTERNAL_ERROR_MESSAGE;
  return false;
}


bool JvmLocalVariableReader::IsDefinedAtLocation(jlocation location) const {
  // Special case: we use (section_length_ == -1) to indicate that
  // the local variable is defined throughout the entire function.
  if (section_length_ == -1) {
    return true;
  }

  // According to JVMTI documentation the range is inclusive at both ends, but
  // the reality suggest that the right end of the range is exclusive.
  return (location >= start_location_) &&
         (location < start_location_ + section_length_);
}

}  // namespace cdbg
}  // namespace devtools


