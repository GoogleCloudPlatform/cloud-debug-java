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

#include "jni_method_caller.h"

namespace devtools {
namespace cdbg {

bool JniMethodCaller::Bind(
    jclass cls,
    const ClassMetadataReader::Method& metadata) {
  if (!cls_.Assign(cls)) {
    return false;
  }

  metadata_ = metadata;

  if (!ParseJMethodSignature(metadata_.signature, &method_signature_)) {
    LOG(ERROR) << "Failed to parse method signature";
    return false;
  }

  if (metadata_.is_static()) {
    method_id_ = cls_.GetStaticMethod(metadata_.name, metadata_.signature);
  } else {
    method_id_ = cls_.GetInstanceMethod(metadata_.name, metadata_.signature);
  }

  if (method_id_ == nullptr) {
    LOG(ERROR) << "Method " << metadata_.name << " not found, "
                  "class: " << metadata_.class_signature.object_signature
               << ", signature: " << metadata_.signature;
    return false;
  }

  return true;
}


MethodCallResult JniMethodCaller::Call(
    bool nonvirtual,
    jobject source,
    const std::vector<JVariant>& arguments) {
  std::vector<jvalue> argument_values;
  argument_values.reserve(arguments.size());
  for (const JVariant& argument : arguments) {
    argument_values.push_back(argument.get_jvalue());
  }

  jvalue* argument_jvalues =
      (argument_values.empty() ? nullptr : &argument_values[0]);

  JVariant return_value;
  if (metadata_.is_static()) {
    return_value = CallStatic(argument_jvalues);
  } else if (nonvirtual) {
    return_value = CallNonVirtual(source, argument_jvalues);
  } else {
    return_value = CallVirtual(source, argument_jvalues);
  }

  if (jni()->ExceptionCheck()) {
    return MethodCallResult::PendingJniException();
  }

  return MethodCallResult::Success(std::move(return_value));
}


JVariant JniMethodCaller::CallStatic(jvalue* arguments) {
  switch (method_signature_.return_type.type) {
    case JType::Void:
      jni()->CallStaticVoidMethodA(
          cls_.get(),
          method_id_,
          arguments);
      return JVariant();  // = void

    case JType::Boolean:
      return JVariant::Boolean(jni()->CallStaticBooleanMethodA(
          cls_.get(),
          method_id_,
          arguments));

    case JType::Byte:
      return JVariant::Byte(jni()->CallStaticByteMethodA(
          cls_.get(),
          method_id_,
          arguments));

    case JType::Char:
      return JVariant::Char(jni()->CallStaticCharMethodA(
          cls_.get(),
          method_id_,
          arguments));

    case JType::Short:
      return JVariant::Short(jni()->CallStaticShortMethodA(
          cls_.get(),
          method_id_,
          arguments));

    case JType::Int:
      return JVariant::Int(jni()->CallStaticIntMethodA(
          cls_.get(),
          method_id_,
          arguments));

    case JType::Long:
      return JVariant::Long(jni()->CallStaticLongMethodA(
          cls_.get(),
          method_id_,
          arguments));

    case JType::Float:
      return JVariant::Float(jni()->CallStaticFloatMethodA(
          cls_.get(),
          method_id_,
          arguments));

    case JType::Double:
      return JVariant::Double(jni()->CallStaticDoubleMethodA(
          cls_.get(),
          method_id_,
          arguments));

    case JType::Object:
      return JVariant::LocalRef(jni()->CallStaticObjectMethodA(
          cls_.get(),
          method_id_,
          arguments));

    default:
      DCHECK(false);
      return JVariant();
  }
}


JVariant JniMethodCaller::CallNonVirtual(jobject source, jvalue* arguments) {
  switch (method_signature_.return_type.type) {
    case JType::Void:
      jni()->CallNonvirtualVoidMethodA(
          source,
          cls_.get(),
          method_id_,
          arguments);
      return JVariant();  // = void

    case JType::Boolean:
      return JVariant::Boolean(jni()->CallNonvirtualBooleanMethodA(
          source,
          cls_.get(),
          method_id_,
          arguments));

    case JType::Byte:
      return JVariant::Byte(jni()->CallNonvirtualByteMethodA(
          source,
          cls_.get(),
          method_id_,
          arguments));

    case JType::Char:
      return JVariant::Char(jni()->CallNonvirtualCharMethodA(
          source,
          cls_.get(),
          method_id_,
          arguments));

    case JType::Short:
      return JVariant::Short(jni()->CallNonvirtualShortMethodA(
          source,
          cls_.get(),
          method_id_,
          arguments));

    case JType::Int:
      return JVariant::Int(jni()->CallNonvirtualIntMethodA(
          source,
          cls_.get(),
          method_id_,
          arguments));

    case JType::Long:
      return JVariant::Long(jni()->CallNonvirtualLongMethodA(
          source,
          cls_.get(),
          method_id_,
          arguments));

    case JType::Float:
      return JVariant::Float(jni()->CallNonvirtualFloatMethodA(
          source,
          cls_.get(),
          method_id_,
          arguments));

    case JType::Double:
      return JVariant::Double(jni()->CallNonvirtualDoubleMethodA(
          source,
          cls_.get(),
          method_id_,
          arguments));

    case JType::Object:
      return JVariant::LocalRef(jni()->CallNonvirtualObjectMethodA(
          source,
          cls_.get(),
          method_id_,
          arguments));

    default:
      DCHECK(false);
      return JVariant();
  }
}

JVariant JniMethodCaller::CallVirtual(jobject source, jvalue* arguments) {
  switch (method_signature_.return_type.type) {
    case JType::Void:
      jni()->CallVoidMethodA(
          source,
          method_id_,
          arguments);
      return JVariant();  // = void

    case JType::Boolean:
      return JVariant::Boolean(jni()->CallBooleanMethodA(
          source,
          method_id_,
          arguments));

    case JType::Byte:
      return JVariant::Byte(jni()->CallByteMethodA(
          source,
          method_id_,
          arguments));

    case JType::Char:
      return JVariant::Char(jni()->CallCharMethodA(
          source,
          method_id_,
          arguments));

    case JType::Short:
      return JVariant::Short(jni()->CallShortMethodA(
          source,
          method_id_,
          arguments));

    case JType::Int:
      return JVariant::Int(jni()->CallIntMethodA(
          source,
          method_id_,
          arguments));

    case JType::Long:
      return JVariant::Long(jni()->CallLongMethodA(
          source,
          method_id_,
          arguments));

    case JType::Float:
      return JVariant::Float(jni()->CallFloatMethodA(
          source,
          method_id_,
          arguments));

    case JType::Double:
      return JVariant::Double(jni()->CallDoubleMethodA(
          source,
          method_id_,
          arguments));

    case JType::Object:
      return JVariant::LocalRef(jni()->CallObjectMethodA(
          source,
          method_id_,
          arguments));

    default:
      DCHECK(false);
      return JVariant();
  }
}

}  // namespace cdbg
}  // namespace devtools
