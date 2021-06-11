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

#include "safe_method_caller.h"

#include <cstdint>

#include "jni_proxy_nullpointerexception.h"
#include "jni_method_caller.h"
#include "type_util.h"

ABSL_DECLARE_FLAG(bool, enable_safe_caller);
ABSL_DECLARE_FLAG(int32, safe_caller_max_array_elements);
ABSL_DECLARE_FLAG(int32, safe_caller_max_interpreter_stack_depth);

namespace devtools {
namespace cdbg {

using devtools::cdbg::nanojava::NanoJavaInterpreter;

SafeMethodCaller::SafeMethodCaller(
    const Config* config,
    Config::MethodCallQuota quota,
    ClassIndexer* class_indexer,
    ClassFilesCache* class_files_cache)
    : config_(config),
      quota_(quota),
      class_indexer_(class_indexer),
      class_files_cache_(class_files_cache) {
}


SafeMethodCaller::~SafeMethodCaller() {
  DCHECK(current_interpreter_ == nullptr);
}


ErrorOr<JVariant> SafeMethodCaller::Invoke(
    const ClassMetadataReader::Method& metadata,
    const JVariant& source,
    std::vector<JVariant> arguments) {
  DCHECK(current_interpreter_ == nullptr)
      << "InvokeInternal should be used for recursive calls";

  jobject source_obj = nullptr;
  if (!metadata.is_static() && !source.get<jobject>(&source_obj)) {
    return INTERNAL_ERROR_MESSAGE;
  }

  MethodCallResult rc =
      InvokeInternal(false, metadata, source_obj, std::move(arguments));

  switch (rc.result_type()) {
    case MethodCallResult::Type::Error:
      return rc.error();

    case MethodCallResult::Type::JavaException:
      return rc.format_exception();

    case MethodCallResult::Type::Success:
      return MethodCallResult::detach_return_value(std::move(rc));
  }

  return INTERNAL_ERROR_MESSAGE;
}


MethodCallResult SafeMethodCaller::InvokeNested(
    bool nonvirtual,
    const ConstantPool::MethodRef& method,
    jobject source,
    std::vector<JVariant> arguments) {
  return InvokeInternal(nonvirtual, method.metadata.value(), source, arguments);
}


MethodCallResult SafeMethodCaller::InvokeInternal(
    bool nonvirtual,
    const ClassMetadataReader::Method& metadata,
    jobject source,
    std::vector<JVariant> arguments) {
  if (!metadata.is_static() && (source == nullptr)) {
    return MethodCallResult::JavaException(
        jniproxy::NullPointerException()->NewObject()
        .Release(ExceptionAction::LOG_AND_IGNORE).get());
  }

  // Figure out the class we will be calling. For expression "a.f()" we have:
  // 1. Declared type of "a", which is "metadata.class_signature".
  // 2. Actual object type of "a" (e.g. while "a" might be "java.lang.Object",
  //    the actual type might be "java.util.HashMap").
  // 3. The class in which the method was defined (e.g. "java.util.HashMap"
  //    overloads "toString()", but some custom class might not).
  ErrorOr<SafeMethodCaller::CallTarget> call_target =
      GetCallTarget(nonvirtual, metadata, source);
  if (call_target.is_error()) {
    return MethodCallResult::Error(call_target.error_message());
  }

  switch (call_target.value().method_config->action) {
    case Config::Method::CallAction::Block:
      return MethodBlocked(metadata, call_target.value());

    case Config::Method::CallAction::Allow:
      return InvokeJni(
          nonvirtual,
          metadata,
          source,
          std::move(arguments),
          call_target.value());

    case Config::Method::CallAction::Interpret:
      return InvokeInterpreter(
          metadata,
          source,
          std::move(arguments),
          call_target.value());
  }

  DCHECK(false) << "bad action";
  return MethodCallResult::Error(INTERNAL_ERROR_MESSAGE);
}


ErrorOr<SafeMethodCaller::CallTarget> SafeMethodCaller::GetCallTarget(
    bool nonvirtual,
    const ClassMetadataReader::Method& metadata,
    jobject source) {
  JniLocalRef object_cls;
  JniLocalRef method_cls;

  if (metadata.is_static()) {
    std::shared_ptr<ClassIndexer::Type> type = class_indexer_->GetReference(
        metadata.class_signature.object_signature);

    object_cls = JniNewLocalRef(type->FindClass());
    if (object_cls == nullptr) {
      DLOG(INFO) << "Class " << type->GetSignature()
                 << " not loaded, call stack:\n" << CurrentCallStack();
      return FormatMessageModel {
          ClassNotLoaded,
          {
            TypeNameFromSignature(metadata.class_signature),
            SignatureFromJSignature(metadata.class_signature)
          }
      };
    }

    method_cls = JniNewLocalRef(object_cls.get());
  } else if (nonvirtual) {
    // Invokespecial (i.e., nonvirtual=true) instruction is used when an
    // instance method must be invoked based on the type of the reference, not
    // on the class of the object. In other words, invokespecial requires
    // static binding (compile-time type), rather than dynamic binding (i.e.,
    // runtime type).
    //
    // There are three particular cases where invokespecial instruction is used:
    //   1. Invocation of <init> methods.
    //   2. Invocation of private methods.
    //   3. Invocation of methods using the super keyword.
    //
    // In cases (1) and (2), searching the given method name/signature in the
    // given reference type (similar to the is_static case above) is sufficient,
    // but the third case requires special class/method resolution. When a
    // method is invoked with the 'super' keyword, the provided super class
    // (i.e., the static type) may not implement the given method, and we must
    // search for the closest super class with the given method name and
    // signature. Note that this search is different from the regular virtual
    // method case, because the search does not start with the type of the
    // object, but instead, starts with the type of the reference.
    //
    // Note:
    // The JVM specification for the invokespecial instruction further explains
    // a split-behavior based on the existence of the ACC_SUPER flag for the
    // target class. However, all Java versions starting with Java 1.1 are
    // guaranteed to always have the ACC_SUPER flag set, and we do not support
    // Java 1.0. Therefore, we do not check the class flags here, and just
    // assume ACC_SUPER is always set.

    std::shared_ptr<ClassIndexer::Type> type = class_indexer_->GetReference(
        metadata.class_signature.object_signature);

    object_cls = JniNewLocalRef(type->FindClass());
    if (object_cls == nullptr) {
      DLOG(INFO) << "Class " << type->GetSignature()
                 << " not loaded, call stack:\n" << CurrentCallStack();
      return FormatMessageModel {
          ClassNotLoaded,
          {
            TypeNameFromSignature(metadata.class_signature),
            SignatureFromJSignature(metadata.class_signature)
          }
      };
    }

    jmethodID method_id = jni()->GetMethodID(
        static_cast<jclass>(object_cls.get()),
        metadata.name.c_str(),
        metadata.signature.c_str());
    if (jni()->ExceptionCheck()) {
      DLOG(INFO) << "Method not found: " << metadata.name << ", "
                << metadata.signature;
      return MethodCallResult::PendingJniException().format_exception();
    }

    if (method_id == nullptr) {
      return INTERNAL_ERROR_MESSAGE;
    }

    method_cls = GetMethodDeclaringClass(method_id);
  } else {
    object_cls = GetObjectClass(source);
    if (object_cls == nullptr) {
      return INTERNAL_ERROR_MESSAGE;
    }

    jmethodID method_id = jni()->GetMethodID(
        static_cast<jclass>(object_cls.get()),
        metadata.name.c_str(),
        metadata.signature.c_str());
    if (jni()->ExceptionCheck()) {
      return MethodCallResult::PendingJniException().format_exception();
    }

    if (method_id == nullptr) {
      return INTERNAL_ERROR_MESSAGE;
    }

    method_cls = GetMethodDeclaringClass(method_id);
  }

  const std::string method_cls_signature = GetClassSignature(method_cls.get());
  const std::string object_cls_signature = GetClassSignature(object_cls.get());

  const Config::Method& method_config = config_->GetMethodRule(
      method_cls_signature,
      object_cls_signature,
      metadata.name,
      metadata.signature);

  return CallTarget {
      std::move(method_cls),
      std::move(method_cls_signature),
      std::move(object_cls),
      std::move(object_cls_signature),
      &method_config
  };
}


MethodCallResult SafeMethodCaller::MethodBlocked(
    const ClassMetadataReader::Method& metadata,
    const CallTarget& call_target) const {
  DLOG(INFO)
      << "Method blocked, object class: " << call_target.object_cls_signature
      << ", method class: " << call_target.method_cls_signature
      << ", name: " << metadata.name
      << ", call stack:\n " << CurrentCallStack();

  std::string name;
  name += TypeNameFromJObjectSignature(call_target.method_cls_signature);
  name += '.';
  name += metadata.name;

  return MethodCallResult::Error({ MethodNotSafe, { name } });
}


MethodCallResult SafeMethodCaller::InvokeJni(
    bool nonvirtual,
    const ClassMetadataReader::Method& metadata,
    jobject source,
    std::vector<JVariant> arguments,
    const CallTarget& call_target) {
  if (call_target.method_config->require_temporary_object &&
      !IsTemporaryObject(source)) {
    return MethodBlocked(metadata, call_target);
  }

  MethodCallResult rc;

  if (call_target.method_config->thunk != nullptr) {
    rc = call_target.method_config->thunk(this, source, &arguments);
    if (rc.result_type() != MethodCallResult::Type::Success) {
      return rc;
    }
  }

  JMethodSignature method_signature;
  if (!ParseJMethodSignature(metadata.signature, &method_signature)) {
    LOG(ERROR) << "Failed to parse method signature, "
                  "class: " << metadata.class_signature.object_signature
               << ", name: " << metadata.name
               << ", signature: " << metadata.signature;
    return MethodCallResult::Error(INTERNAL_ERROR_MESSAGE);
  }

  // Check that the class we found matches the source object.
  if (!metadata.is_static() &&
      nonvirtual &&
      !jni()->IsInstanceOf(
          source,
          static_cast<jclass>(call_target.object_cls.get()))) {
    return MethodCallResult::Error(INTERNAL_ERROR_MESSAGE);
  }

  rc = CheckArguments(method_signature, arguments);
  if (rc.result_type() != MethodCallResult::Type::Success) {
    return rc;
  }

  // TODO: cache method callers.
  JniMethodCaller method_caller;
  if (!method_caller.Bind(
          static_cast<jclass>(call_target.object_cls.get()),
          metadata)) {
    return MethodCallResult::Error({
        ClassNotLoaded,
        {
          TypeNameFromJObjectSignature(call_target.object_cls_signature),
          call_target.object_cls_signature
        }
    });
  }

  rc = method_caller.Call(nonvirtual, source, arguments);
  if (call_target.method_config->returns_temporary_object &&
      (rc.result_type() == MethodCallResult::Type::Success) &&
      rc.return_value().has_non_null_object()) {
    temporary_objects_.Insert(rc.return_ref(), {});
  }

  return rc;
}


MethodCallResult SafeMethodCaller::InvokeInterpreter(
    const ClassMetadataReader::Method& metadata,
    jobject source,
    std::vector<JVariant> arguments,
    const CallTarget& call_target) {
  if (!absl::GetFlag(FLAGS_enable_safe_caller)) {
    // Configuration error. When safe caller is not enabled, the configuration
    // should never specify MethodCallAction::Interpret.
    return MethodCallResult::Error(INTERNAL_ERROR_MESSAGE);
  }

  // We want "Method call X not allowed" error message if safe caller is
  // disabled in the current scenario. Without this statement, the error message
  // will be "Method taking too many cycles".
  if (IsNanoJavaInterpreterDisabled()) {
    return MethodBlocked(metadata, call_target);
  }

  // Limit maximum interpreter stack depth to avoid native stack overflow.
  if ((current_interpreter_ != nullptr) &&
      (current_interpreter_->GetStackDepth() >=
       absl::GetFlag(FLAGS_safe_caller_max_interpreter_stack_depth))) {
    LOG(INFO) << "Interpreter stack overflow:\n" << CurrentCallStack();
    return MethodCallResult::Error({ StackOverflow });
  }

  ErrorOr<std::unique_ptr<ClassFilesCache::AutoClassFile>> class_file =
      CacheLoadClassFile(call_target.method_cls.get());

  if (class_file.is_error()) {
    return MethodCallResult::Error(class_file.error_message());
  }

  ClassFile::Method* method = class_file.value()->get()->FindMethod(
      metadata.is_static(),
      metadata.name,
      metadata.signature);

  if (method == nullptr) {
    return MethodCallResult::Error({
        metadata.is_static() ? StaticMethodNotFound : InstanceMethodNotFound,
        {
            metadata.name,
            TypeNameFromJObjectSignature(
                class_file.value()->get()->GetClass()->type->GetSignature())
        }
    });
  }

  NanoJavaInterpreter interpreter(
      this,
      method,
      current_interpreter_,
      source,
      arguments);

  // Add the new interpreter to the stack trace linked list.
  const NanoJavaInterpreter* previous_interpreter = current_interpreter_;
  current_interpreter_ = &interpreter;

  // Execute the method. Method calls within the executed method will
  // recursively call this function.
  MethodCallResult rc = interpreter.Execute();

  // Restore the stack trace.
  current_interpreter_ = previous_interpreter;

  return rc;
}


void SafeMethodCaller::NewObjectAllocated(jobject obj) {
  DCHECK(obj != nullptr);
  temporary_objects_.Insert(obj, {});
}


std::unique_ptr<FormatMessageModel>
SafeMethodCaller::IsNextInstructionAllowed() {
  ++total_instructions_counter_;

  if (total_instructions_counter_ > quota_.max_interpreter_instructions) {
    return std::unique_ptr<FormatMessageModel>(
        new FormatMessageModel { InterpreterQuotaExceeded });
  }

  return nullptr;
}

std::unique_ptr<FormatMessageModel> SafeMethodCaller::IsNewArrayAllowed(
    int32_t count) {
  if (count > absl::GetFlag(FLAGS_safe_caller_max_array_elements)) {
    return std::unique_ptr<FormatMessageModel>(new FormatMessageModel {
        MethodNotSafeNewArrayTooLarge,
        { GetCurrentMethodName(), std::to_string(count) }
    });
  }

  return nullptr;
}

std::unique_ptr<FormatMessageModel> SafeMethodCaller::IsArrayModifyAllowed(
    jobject array) {
  if (!IsTemporaryObject(array)) {
    return std::unique_ptr<FormatMessageModel>(new FormatMessageModel {
      MethodNotSafeAttemptedArrayChange,
      { GetCurrentMethodName() }
    });
  }

  return nullptr;
}



std::unique_ptr<FormatMessageModel> SafeMethodCaller::IsFieldModifyAllowed(
    jobject target,
    const ConstantPool::FieldRef& field) {
  const char* error_format = nullptr;  // No error.
  if (field.is_static.value()) {
    error_format = MethodNotSafeAttemptedChangeStaticField;
  } else if (!IsTemporaryObject(target)) {
    error_format = MethodNotSafeAttemptedInstanceFieldChange;
  }

  if (error_format == nullptr) {
    return nullptr;  // Modifying the field is allowed.
  }

  return std::unique_ptr<FormatMessageModel>(new FormatMessageModel {
    error_format,
    {
        GetCurrentMethodName(),
        TypeNameFromJObjectSignature(field.owner->type->GetSignature()),
        field.field_name
    }
  });
}


bool SafeMethodCaller::IsTemporaryObject(jobject obj) const {
  return temporary_objects_.Contains(obj);
}


MethodCallResult SafeMethodCaller::CheckArguments(
    const JMethodSignature& signature,
    const std::vector<JVariant>& arguments) {
  // Check arguments. If argument types are messed up, JVM may crash.
  if (arguments.size() != signature.arguments.size()) {
    LOG(ERROR) << "Arguments count mismatch";
    return MethodCallResult::Error(INTERNAL_ERROR_MESSAGE);
  }

  for (int i = 0; i < arguments.size(); ++i) {
    if (!CheckSignature(signature.arguments[i], arguments[i])) {
      LOG(ERROR) << "Type mismatch for argument " << i;
      return MethodCallResult::Error(INTERNAL_ERROR_MESSAGE);
    }
  }

  return MethodCallResult::Success(JVariant());
}


bool SafeMethodCaller::CheckSignature(
    const JSignature& signature,
    const JVariant& value) const {
  if (value.type() != signature.type) {
    LOG(ERROR)
        << "Type mismatch, expected type: "
        << static_cast<int>(signature.type)
        << ", specified type: "
        << static_cast<int>(value.type());
    return false;
  }

  if (value.has_non_null_object()) {
    std::shared_ptr<ClassIndexer::Type> type =
        class_indexer_->GetReference(signature.object_signature);
    jclass cls = type->FindClass();
    if (cls == nullptr) {
      return false;
    }

    jobject obj = nullptr;
    value.get<jobject>(&obj);

    if (!jni()->IsInstanceOf(obj, cls)) {
      LOG(ERROR)
            << "Type mismatch, expected type: "
            << signature.object_signature
            << ", actual type: "
            << GetObjectClassSignature(obj);
      return false;
    }
  }

  return true;
}


ErrorOr<std::unique_ptr<ClassFilesCache::AutoClassFile>>
SafeMethodCaller::CacheLoadClassFile(jobject cls) {
  if (total_method_load_counter_ >= quota_.max_classes_load) {
    auto class_file = class_files_cache_->Get(cls);
    if (class_file == nullptr) {
      return FormatMessageModel { MethodLoadQuotaExceeded };
    }

    return std::move(class_file);
  }

  bool loaded = false;
  auto class_file = class_files_cache_->GetOrLoad(cls, &loaded);

  if (loaded) {
    ++total_method_load_counter_;
  }

  if (class_file == nullptr) {
    return FormatMessageModel {
        ClassLoadFailed,
        { TypeNameFromJObjectSignature(GetClassSignature(cls)) }
    };
  }

  return std::move(class_file);
}

std::string SafeMethodCaller::GetCurrentMethodName() const {
  if (current_interpreter_ == nullptr) {
    static const std::string kEmpty;
    return kEmpty;
  }

  return current_interpreter_->method_name();
}

// Format call stack of the interpreted methods.
std::string SafeMethodCaller::CurrentCallStack() const {
  if (current_interpreter_ == nullptr) {
    return std::string();
  }

  return current_interpreter_->FormatCallStack();
}

}  // namespace cdbg
}  // namespace devtools
