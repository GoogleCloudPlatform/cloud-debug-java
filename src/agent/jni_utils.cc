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

#include "jni_utils.h"

#include <cstdarg>

#include "jni_proxy_object.h"
#include "jni_proxy_printwriter.h"
#include "jni_proxy_stringwriter.h"
#include "jni_proxy_throwable.h"
#include "jvmti_buffer.h"

namespace devtools {
namespace cdbg {

std::string JniToNativeString(jobject jstr) {
  if (jstr == nullptr) {
    return std::string();
  }

  const char* cstr = jni()->GetStringUTFChars(static_cast<jstring>(jstr), 0);
  std::string str(cstr);
  jni()->ReleaseStringUTFChars(static_cast<jstring>(jstr), cstr);

  return str;
}

JniLocalRef JniToJavaString(const std::string& s) {
  return JniToJavaString(s.c_str());
}

JniLocalRef JniToJavaString(const char* s) {
  if (s == nullptr) {
    return nullptr;
  }

  // The only exception NewStringUTF can throw is "OutOfMemoryException". We
  // deliberately don't handle it and let it propagate through. Out of memory
  // condition is generally not handled anywhere in the code.
  return JniLocalRef(jni()->NewStringUTF(s));
}

JniLocalRef JniToByteArray(const std::string& data) {
  JniLocalRef byte_array(jni()->NewByteArray(data.size()));
  if (byte_array == nullptr) {
    LOG(ERROR) << "Failed to allocate byte array, size: " << data.size();
    return nullptr;
  }

  jni()->SetByteArrayRegion(
      static_cast<jbyteArray>(byte_array.get()),
      0,
      data.size(),
      reinterpret_cast<const jbyte*>(data.data()));

  return byte_array;
}

std::string JniToNativeBlob(jobject byte_array_obj) {
  if (byte_array_obj == nullptr) {
    return std::string();
  }

  jbyteArray byte_array = static_cast<jbyteArray>(byte_array_obj);

  jsize length = jni()->GetArrayLength(byte_array);
  jbyte* elements = jni()->GetByteArrayElements(byte_array, nullptr);
  std::string data(elements, elements + length);
  jni()->ReleaseByteArrayElements(byte_array, elements, JNI_ABORT);

  return data;
}

std::vector<std::string> JniToNativeStringArray(jobject string_array_obj) {
  if (string_array_obj == nullptr) {
    return {};
  }

  const jsize size =
      jni()->GetArrayLength(static_cast<jarray>(string_array_obj));

  std::vector<std::string> elements;
  elements.reserve(size);

  for (int i = 0; i < size; ++i) {
    JniLocalRef jstr(jni()->GetObjectArrayElement(
        static_cast<jobjectArray>(string_array_obj),
        i));

    elements.push_back(JniToNativeString(jstr.get()));
  }

  return elements;
}

JniLocalRef JniToJavaStringArray(const std::vector<std::string>& arr) {
  const size_t size = arr.size();

  // TODO: use JNI proxy generated code.
  JavaClass jstring;
  if (!jstring.FindWithJNI("java/lang/String")) {
    return nullptr;
  }

  JniLocalRef string_array(jni()->NewObjectArray(size, jstring.get(), nullptr));
  if (string_array == nullptr) {
    LOG(ERROR) << "Failed to allocate string array, size: " << size;
    return nullptr;
  }

  for (int i = 0; i < size; ++i) {
    jni()->SetObjectArrayElement(
        static_cast<jobjectArray>(string_array.get()),
        i,
        JniToJavaString(arr[i]).get());
  }

  if (!JniCheckNoException("JniToNativeStringArray")) {
    return nullptr;
  }

  return string_array;
}

JniLocalRef JniNewLocalRef(jobject obj) {
  return JniLocalRef(jni()->NewLocalRef(obj));
}


JniGlobalRef JniNewGlobalRef(jobject obj) {
  if (obj == nullptr) {
    return nullptr;
  }

  return JniGlobalRef(jni()->NewGlobalRef(obj));
}


JniLocalRef GetMethodDeclaringClass(jmethodID method) {
  if (method == nullptr) {
    LOG(ERROR) << "method == nullptr";
    return nullptr;
  }

  jclass cls = nullptr;
  jvmtiError err = jvmti()->GetMethodDeclaringClass(method, &cls);
  if (err != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "GetMethodDeclaringClass failed, error: " << err;
    return nullptr;
  }

  return JniLocalRef(cls);
}


JniLocalRef GetObjectClass(jobject obj) {
  if (obj == nullptr) {
    return nullptr;
  }

  return JniLocalRef(jni()->GetObjectClass(obj));
}


// Gets JVMTI signature of a class.
std::string GetClassSignature(jobject cls) {
  if (cls == nullptr) {
    return std::string();
  }

  JvmtiBuffer<char> class_signature_buffer;
  jvmtiError err = jvmti()->GetClassSignature(
      static_cast<jclass>(cls),
      class_signature_buffer.ref(),
      nullptr);

  if (err != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "GetClassSignature failed, error: " << err;
    return std::string();
  }

  return class_signature_buffer.get();
}

std::string GetObjectClassSignature(jobject obj) {
  return GetClassSignature(GetObjectClass(obj).get());
}

static JavaExceptionInfo JniGetExceptionInfo(
    JniLocalRef exception_obj,
    bool verbose) {
  JavaExceptionInfo exception_info;
  exception_info.exception_obj = std::move(exception_obj);

  // Don't use "JavaClass" here to avoid infinite loop in case of errors since
  // "JavaClass" will call this method in case of exceptions.
  exception_info.exception_cls =
      GetObjectClass(exception_info.exception_obj.get());
  if (exception_info.exception_cls == nullptr) {
    return {};
  }

  JvmtiBuffer<char> class_signature_buffer;
  jvmtiError err = jvmti()->GetClassSignature(
      static_cast<jclass>(exception_info.exception_cls.get()),
      class_signature_buffer.ref(),
      nullptr);
  if (err != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "GetClassSignature failed, error: " << err;
    return {};
  }

  exception_info.exception_class_signature = class_signature_buffer.get();

  if (!verbose) {
    return exception_info;
  }

  // Don't worry about exceptions thrown by "GetMethodID". This function is
  // only called by "JniCatchException" that clears exceptions afterwards.
  jmethodID to_string_method = jni()->GetMethodID(
      static_cast<jclass>(exception_info.exception_cls.get()),
      "toString",
      "()Ljava/lang/String;");
  if (to_string_method == nullptr) {
    return exception_info;
  }

  JniLocalRef exception_jstr(jni()->CallObjectMethod(
      exception_info.exception_obj.get(),
      to_string_method));
  if (exception_jstr != nullptr) {
    exception_info.exception_message =
        JniToNativeString(exception_jstr.get());
  }

  return exception_info;
}


Nullable<JavaExceptionInfo> JniCatchException(bool verbose) {
  // Check potential exception thrown by the method we just called.
  JniLocalRef exception_obj(jni()->ExceptionOccurred());
  if (exception_obj == nullptr) {
    return nullptr;  // No exception.
  }

  // If "JniGetExceptionInfo" results in an exception, it will be cleared
  // in the upcoming call to "ExceptionClear".
  JavaExceptionInfo exception_info =
      JniGetExceptionInfo(std::move(exception_obj), verbose);

  jni()->ExceptionClear();

  return std::move(exception_info);
}


bool JniCheckNoException(const char* debug_context) {
  Nullable<JavaExceptionInfo> exception = JniCatchException(true);
  if (!exception.has_value()) {
    return true;  // No exception.
  }

  LOG(ERROR) << "Java exception "
             << exception.value().exception_class_signature
             << " thrown at " << debug_context << ": "
             << exception.value().exception_message;

  return false;  // Exception was thrown.
}

std::string FormatException(jobject exception) {
  // C++ equivalent of this Java code:
  //   try {
  //     StringWriter stringWriter = new StringWriter();
  //     PrintWriter printWriter = new PrintWriter(stringWriter);
  //     exception.printStackTrace(printWriter);
  //     printWriter.flush();
  //     return stringWriter.toString();
  //   } catch (Throwable e) {
  //     return "<bad exception object>";
  //   }
  //
  // It is important that this code never tries to log any exception that
  // the invoked methods might throw. Otherwise we may end up with infinite
  // loop.

  static constexpr char kError[] = "<bad exception object>";

  ExceptionOr<std::nullptr_t> rc;

  const JniLocalRef& string_writer =
      jniproxy::StringWriter()->NewObject()
      .Release(ExceptionAction::IGNORE);
  if (string_writer == nullptr) {
    return kError;
  }

  const JniLocalRef& print_writer =
      jniproxy::PrintWriter()->NewObject(string_writer.get())
      .Release(ExceptionAction::IGNORE);
  if (print_writer == nullptr) {
    return kError;
  }

  rc = jniproxy::Throwable()->printStackTrace(exception, print_writer.get());
  if (rc.HasException()) {
    return kError;
  }

  rc = jniproxy::PrintWriter()->flush(print_writer.get());
  if (rc.HasException()) {
    return kError;
  }

  ExceptionOr<std::string> msg =
      jniproxy::Object()->toString(string_writer.get());
  if (msg.HasException()) {
    return kError;
  }

  return msg.Release(ExceptionAction::IGNORE);
}

JniLocalRef JniGetEnumValue(jclass enum_cls, const char* value_name) {
  JvmtiBuffer<char> class_signature_buffer;
  jvmtiError err = jvmti()->GetClassSignature(
      enum_cls,
      class_signature_buffer.ref(),
      nullptr);
  if (err != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "GetClassSignature failed, error: " << err;
    return nullptr;
  }

  jfieldID field = jni()->GetStaticFieldID(
      enum_cls,
      value_name,
      class_signature_buffer.get());
  if (!JniCheckNoException(value_name)) {
    return nullptr;
  }

  if (field == nullptr) {
    LOG(ERROR) << "Static field " << value_name
               << " in " << class_signature_buffer.get()
               << " could not be found";
    return nullptr;
  }

  JniLocalRef enum_value_local_ref(
      jni()->GetStaticObjectField(enum_cls, field));
  if (!JniCheckNoException(value_name)) {
    return nullptr;
  }

  if (enum_value_local_ref == nullptr) {
    LOG(ERROR) << "Static field " << value_name
               << " in " << class_signature_buffer.get()
               << " not available";
    return nullptr;
  }

  return enum_value_local_ref;
}


bool JavaClass::Assign(const JavaClass& cls) {
  return Assign(cls.get());
}


bool JavaClass::Assign(jclass cls) {
  ReleaseRef();

  if (cls == nullptr) {
    LOG(ERROR) << "Null object provided";
    return false;
  }

  cls_ = JniNewGlobalRef(cls);
  if (cls_ == nullptr) {
    LOG(ERROR) << "Failed to create global reference to " << cls;
    return false;
  }

  return true;
}


bool JavaClass::AssignObjectClass(jobject obj) {
  return Assign(static_cast<jclass>(GetObjectClass(obj).get()));
}


bool JavaClass::FindWithJNI(const char* class_signature) {
  ReleaseRef();

  JniLocalRef cls_local_ref(jni()->FindClass(class_signature));

  if (!JniCheckNoException("JavaClass::FindWithJNI")) {
    LOG(ERROR) << "Java class " << class_signature
               << " could not be loaded due to exception";
    return false;
  }

  if (cls_local_ref == nullptr) {
    LOG(ERROR) << "Java class " << class_signature << " not found";
    return false;
  }

  return Assign(static_cast<jclass>(cls_local_ref.get()));
}


bool JavaClass::LoadWithClassLoader(
    jobject class_loader_obj,
    const char* class_name) {
  ReleaseRef();

  // Get the class object of the provided ClassLoader instance.
  JavaClass class_loader_cls;
  if (!class_loader_cls.AssignObjectClass(class_loader_obj)) {
    return false;
  }

  // Get "loadClass" method.
  jmethodID load_class_method = class_loader_cls.GetInstanceMethod(
      "loadClass",
      "(Ljava/lang/String;)Ljava/lang/Class;");
  if (load_class_method == nullptr) {
    class_loader_cls.ReleaseRef();
    return false;
  }

  // Invoke the "loadClass" method.
  JniLocalRef cls_local_ref(jni()->CallObjectMethod(
      class_loader_obj,
      load_class_method,
      JniToJavaString(class_name).get()));

  if (!JniCheckNoException("JavaClass::LoadWithClassLoader")) {
    class_loader_cls.ReleaseRef();
    return false;
  }

  class_loader_cls.ReleaseRef();

  return Assign(static_cast<jclass>(cls_local_ref.get()));
}


void JavaClass::ReleaseRef() {
  cls_ = nullptr;
}


jmethodID JavaClass::GetStaticMethod(
    const char* method_name,
    const char* method_signature) const {
  if (cls_ == nullptr) {
    LOG(ERROR) << "Java class object not available";
    return nullptr;
  }

  jmethodID method = jni()->GetStaticMethodID(
      static_cast<jclass>(cls_.get()),
      method_name,
      method_signature);

  if (!JniCheckNoException("JavaClass::GetStaticMethod")) {
    LOG(ERROR) << "Exception occurred while retrieving static method "
               << method_name << ", signature: " << method_signature;
    return nullptr;
  }

  if (method == nullptr) {
    LOG(ERROR) << "Static method " << method_name << " not found, signature: "
               << method_signature;
    return nullptr;
  }

  return method;
}

jmethodID JavaClass::GetStaticMethod(
    const std::string& method_name, const std::string& method_signature) const {
  return GetStaticMethod(method_name.c_str(), method_signature.c_str());
}

jmethodID JavaClass::GetInstanceMethod(
    const char* method_name,
    const char* method_signature) const {
  if (cls_ == nullptr) {
    LOG(ERROR) << "Java class object not available";
    return nullptr;
  }

  jmethodID method = jni()->GetMethodID(
      static_cast<jclass>(cls_.get()),
      method_name,
      method_signature);

  if (!JniCheckNoException("JavaClass::GetInstanceMethod")) {
    LOG(ERROR) << "Exception occurred while retrieving instance method "
               << method_name << ", signature: " << method_signature;
    return nullptr;
  }

  if (method == nullptr) {
    LOG(ERROR) << "Instance method " << method_name << " not found, "
                  "signature: " << method_signature;
    return nullptr;
  }

  return method;
}

jmethodID JavaClass::GetInstanceMethod(
    const std::string& method_name, const std::string& method_signature) const {
  return GetInstanceMethod(method_name.c_str(), method_signature.c_str());
}

jmethodID JavaClass::GetConstructor(const char* constructor_signature) const {
  return GetInstanceMethod("<init>", constructor_signature);
}


JniLocalRef JavaClass::GetClassLoader() const {
  if (cls_ == nullptr) {
    LOG(ERROR) << "Java class object not available";
    return nullptr;
  }

  // Class of a class.
  JavaClass cls_cls;
  if (!cls_cls.FindWithJNI("java/lang/Class")) {
    return nullptr;
  }

  jmethodID get_class_loader_method = cls_cls.GetInstanceMethod(
      "getClassLoader",
      "()Ljava/lang/ClassLoader;");
  if (get_class_loader_method == nullptr) {
    return nullptr;
  }

  JniLocalRef class_loader(
      jni()->CallObjectMethod(cls_.get(), get_class_loader_method));

  if (!JniCheckNoException("JavaClass::GetClassLoader")) {
    return nullptr;
  }

  if (class_loader == nullptr) {
    LOG(ERROR) << "ClassLoader not available";
  }

  return class_loader;
}


}  // namespace cdbg
}  // namespace devtools
