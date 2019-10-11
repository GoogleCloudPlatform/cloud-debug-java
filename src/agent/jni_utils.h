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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JNI_UTILS_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JNI_UTILS_H_

#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "nullable.h"
#include "common.h"

namespace devtools {
namespace cdbg {

// JNI local reference deleter compatible with "std::unique_ptr".
class JniLocalRefDeleter {
 public:
  void operator() (jobject obj) {
    if (obj != nullptr) {
      jni()->DeleteLocalRef(obj);
    }
  }
};

// JNI global reference deleter compatible with "std::unique_ptr".
class JniGlobalRefDeleter {
 public:
  void operator() (jobject obj) {
    if (obj != nullptr) {
      jni()->DeleteGlobalRef(obj);
    }
  }
};

// Shortcut for "std::unique_ptr" that properly deletes JNI local references.
// "JniLocalRef" ensures only one object can own the reference at a time. When
// "JniLocalRef" is destroyed "std::unique_ptr" calls
// "JniLocalRefDeleter::operator()" to release the reference.
using JniLocalRef = std::unique_ptr<_jobject, JniLocalRefDeleter>;

// Shortcut for "std::unique_ptr" that properly deletes JNI global references.
using JniGlobalRef = std::unique_ptr<_jobject, JniGlobalRefDeleter>;


// Wraps functionality to obtain Java class objects through JNI and
// extract class methods. All the functions handle Java exceptions.
class JavaClass {
 public:
  // Initializes the class to the specified Java class.
  bool Assign(const JavaClass& cls);

  // Initializes the class to the specified Java class.
  bool Assign(jclass cls);

  // Initializes the class to one of the specified Java object.
  bool AssignObjectClass(jobject obj);

  // Finds class (and loads it if necessary) through JNI. This uses
  // application ClassLoader. Since this function loads the class, it
  // must not be called on any application classes - only on system
  // classes.
  bool FindWithJNI(const char* class_signature);

  // Wrapper function for "ClassLoader.loadClass".
  bool LoadWithClassLoader(
      jobject class_loader_obj,
      const char* class_name);

  // Releases the global reference to Java class object. This function must be
  // called before this class is destroyed.
  void ReleaseRef();

  // Fetches static method of this class. Returns nullptr if the method is not
  // found or exception is thrown. The returned "jmethodID" is guaranteed to
  // be valid until "ReleaseRef" is called.
  jmethodID GetStaticMethod(
      const char* method_name,
      const char* method_signature) const;

  // Same as GetStaticMethod(method_name.c_str(), method_signature.c_str())
  jmethodID GetStaticMethod(const std::string& method_name,
                            const std::string& method_signature) const;

  // Fetches instance method of this class. Returns nullptr if the method is
  // not found or exception is thrown. The returned "jmethodID" is guaranteed
  // to be valid until "ReleaseRef" is called.
  jmethodID GetInstanceMethod(
      const char* method_name,
      const char* method_signature) const;

  // Same as GetInstanceMethod(method_name.c_str(), method_signature.c_str())
  jmethodID GetInstanceMethod(const std::string& method_name,
                              const std::string& method_signature) const;

  // Fetches constructor method of this class. This does essentially the same as
  // calling GetInstanceMethod("<init>", constructor_signature).
  jmethodID GetConstructor(const char* constructor_signature) const;

  // Gets the global reference to underlying Java class object. This class
  // retains reference ownership and the caller must not delete the reference.
  jclass get() const { return static_cast<jclass>(cls_.get()); }

  // Gets the class loader for the class. Returns nullptr in case of failure.
  JniLocalRef GetClassLoader() const;

 private:
  // Global reference to underlying Java class object.
  JniGlobalRef cls_;
};


// Maps Java enums to C++ enums.
template <typename TEnum>
class JavaEnum {
 public:
  JavaEnum() {}

  ~JavaEnum() {}

  // Retrieves the Java objects for the specified enum values and associates
  // them with their C++ enum counterparts.
  bool Initialize(
      jclass enum_cls,
      std::initializer_list<std::pair<TEnum, const char*>> enum_values);

  // Gets the Java enum object correspond to C++ enum value. Returns nullptr
  // if not mapped.
  jobject ToJavaEnum(TEnum value) const;

  // Gets the C++ enum value corresponding to Java enum object. Returns null
  // if not mapped.
  Nullable<TEnum> ToNativeEnum(jobject enum_obj) const;

 private:
  std::map<TEnum, JniGlobalRef> enum_map_;

  DISALLOW_COPY_AND_ASSIGN(JavaEnum);
};


// TODO: remove this class and replace usage with ExceptionOr<>.
struct JavaExceptionInfo {
  // Caught exception object.
  JniLocalRef exception_obj;

  // Class of the caught exception object.
  JniLocalRef exception_cls;

  // Signature of the exception class (e.g. "Ljava.lang.OutOfMemoryError;").
  std::string exception_class_signature;

  // Result of "toString()" call on the exception object.
  std::string exception_message;
};

// Specifies what to do with exception in "ExceptionOr<T>::Release".
enum class ExceptionAction {
  // Ignores the exception. Returns default "T()" value.
  IGNORE,

  // Logs the exception. Returns default "T()" value.
  // NOTE: this is an unsafe option. See "FormatException" for details.
  LOG_AND_IGNORE
};

// Stores a value or an exception.
template <typename T>
class ExceptionOr {
 public:
  ExceptionOr() = default;

  ExceptionOr(const char* log_context, JniLocalRef exception, T data);

  // Returns true if exception was caught.
  bool HasException() const { return exception_ != nullptr; }

  // Gets the exception object or nullptr if no exception was caught.
  jthrowable GetException() const {
    return static_cast<jthrowable>(exception_.get());
  }

  // Gets data (only valid if exception was not caught).
  const T& GetData() const {
    DCHECK(exception_ == nullptr);
    return data_;
  }

  // Logs the exception at ERROR level (if caught).
  // NOTE: this is an unsafe function. See "FormatException" for details.
  void LogException() const;

  // Returns the data (with std::move). The "exception_action" specifies
  // what to do if exception was caught.
  // NOTE: logging exception details is an unsafe operations.
  // See "FormatException" for details.
  T Release(ExceptionAction exception_action);

 private:
  // Message prefix to log when exception is caught.
  const char* log_context_ { nullptr };

  // Exception object or nullptr if "data" is valid.
  JniLocalRef exception_;

  // Data (only valid if "exception" is nullptr).
  T data_;
};

// Converts Java String object to C++ UTF-8 string.
std::string JniToNativeString(jobject jstr);

// Converts C++ UTF-8 string to Java string.
JniLocalRef JniToJavaString(const std::string& s);

// Converts C++ UTF-8 string to Java string. Returns nullptr if s is nullptr.
JniLocalRef JniToJavaString(const char* s);

// Converts Java array of strings (String[]) to native vector.
std::vector<std::string> JniToNativeStringArray(jobject string_array_obj);

// Converts native vector to Java array of strings (String[]). Returns empty
// array if "arr" is empty. Returns nullptr on failure.
JniLocalRef JniToJavaStringArray(const std::vector<std::string>& arr);

// Converts native BLOB to Java byte array (byte[]).
JniLocalRef JniToByteArray(const std::string& data);

// Converts Java byte array (byte[]) to native BLOB.
std::string JniToNativeBlob(jobject byte_array_obj);

// Creates a new local reference to the specified Java object. Returns
// nullptr if "obj" is nullptr.
JniLocalRef JniNewLocalRef(jobject obj);

// Creates a new global reference to the specified Java object. Returns
// nullptr if "obj" is nullptr.
JniGlobalRef JniNewGlobalRef(jobject obj);

// Gets the class in which the specified method is contained.
JniLocalRef GetMethodDeclaringClass(jmethodID method);

// Gets the class of the specified object. Returns null on failure.
JniLocalRef GetObjectClass(jobject obj);

// Gets JVMTI signature of a class. Returns empty string on error.
std::string GetClassSignature(jobject cls);

// Gets the signature of the class of the specified object. Returns empty
// string on errors.
std::string GetObjectClassSignature(jobject obj);

// Checks whether a JVM exception has been thrown. If not, returns null.
// If it was, clears the exception and returns the information about the
// thrown exception.
// The exception_message is only filled if "verbose" was specified.
// TODO: remove this class and replace usage with ExceptionOr<>.
Nullable<JavaExceptionInfo> JniCatchException(bool verbose);

// Checks whether JVM exception has been thrown. If it was, this function
// prints the exception details into ERROR log, clears exception and returns
// false. If no exception has been thrown, the return value is true.
// "debug_context" printed as is in the log in case of exception.
// This function should not be used to check exceptions after calling
// arbitrary application method, use "JniCatchException" instead.
// TODO: remove this class and replace usage with ExceptionOr<>.
bool JniCheckNoException(const char* debug_context);

// Prints exception details with the call stack.
// NOTE: this function might be unsafe. It calls "getMessage()" directly
// and does not assert safety. Use this function only for exceptions
// generated by the debugger code. Never use it on exceptions thrown by
// the application code.
std::string FormatException(jobject exception);

// Checks pending exception. If an exception was thrown, constructs
// ExceptionOr<> with an exception and clears the exception. Otherwise
// constructs it with the specified data.
template <typename T>
static ExceptionOr<T> CatchOr(const char* log_context, T data);

// Gets the Java enum value object. Returns nullptr in case of a failure.
JniLocalRef JniGetEnumValue(jclass enum_cls, const char* value_name);

// Retrieves the Java objects for the specified enum values and associates
// them with their C++ enum counterparts.
template <typename TEnum>
bool JavaEnum<TEnum>::Initialize(
    jclass enum_cls,
    std::initializer_list<std::pair<TEnum, const char*>> enum_values) {
  std::map<TEnum, JniGlobalRef> enum_map;
  for (const auto& enum_value : enum_values) {
    JniLocalRef enum_obj = JniGetEnumValue(enum_cls, enum_value.second);
    if (enum_obj == nullptr) {
      return false;
    }

    enum_map.emplace(enum_value.first, JniNewGlobalRef(enum_obj.get()));
  }

  enum_map_ = std::move(enum_map);

  return true;
}

// Gets the Java enum object correspond to C++ enum value. Returns nullptr
// if not mapped.
template <typename TEnum>
jobject JavaEnum<TEnum>::ToJavaEnum(TEnum value) const {
  auto it = enum_map_.find(value);
  if (it == enum_map_.end()) {
    return nullptr;
  }

  return it->second.get();
}

// Gets the C++ enum value corresponding to Java enum object. Returns null
// if not mapped.
template <typename TEnum>
Nullable<TEnum> JavaEnum<TEnum>::ToNativeEnum(jobject enum_obj) const {
  if (enum_obj == nullptr) {
    return nullptr;
  }

  for (const auto& entry : enum_map_) {
    if (jni()->IsSameObject(enum_obj, entry.second.get())) {
      return entry.first;
    }
  }

  return nullptr;
}


template <typename T>
ExceptionOr<T>::ExceptionOr(
    const char* log_context,
    JniLocalRef exception,
    T data)
    : log_context_(log_context),
      exception_(std::move(exception)),
      data_(std::move(data)) {
}


template <typename T>
void ExceptionOr<T>::LogException() const {
  if (exception_ == nullptr) {
    return;
  }

  if (log_context_ == nullptr) {
    LOG(ERROR) << FormatException(exception_.get());
  } else {
    LOG(ERROR) << log_context_ << ": " << FormatException(exception_.get());
  }
}


template <typename T>
T ExceptionOr<T>::Release(ExceptionAction exception_action) {
  if (exception_action == ExceptionAction::LOG_AND_IGNORE) {
    LogException();
  }

  if (exception_ == nullptr) {
    return std::move(data_);
  }

  return T();
}


template <typename T>
static ExceptionOr<T> CatchOr(const char* log_context, T data) {
  if (jni()->ExceptionCheck()) {
    JniLocalRef exception(jni()->ExceptionOccurred());
    jni()->ExceptionClear();
    return ExceptionOr<T>(log_context, std::move(exception), T());
  }

  return ExceptionOr<T>(log_context, nullptr, std::move(data));
}

}  // namespace cdbg
}  // namespace devtools


#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JNI_UTILS_H_
