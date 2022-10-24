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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_JVMTI_ENV_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_JVMTI_ENV_H_

#include <jni.h>
#include <jvmti.h>

#include "gmock/gmock.h"

namespace devtools {
namespace cdbg {

// JVMTI interface is structure with pointers to functions, not a pure virtual
// C++ class that can be mocked. To work around this problem, we create a
// a pure virtual function for each JVMTI function we care about and point
// to it in the function table.
// NOTE: this class does not include all the JVMTI methods, but only the few
// that are used by the agent.
class MockableJvmtiEnv : public _jvmtiEnv {
 public:
  MockableJvmtiEnv() {
    functions = &functions_;

    memset(&functions_, 0, sizeof(functions_));
    functions_.SetEventNotificationMode = &SetEventNotificationMode;
    functions_.RunAgentThread = &RunAgentThread;
    functions_.GetLocalObject = &GetLocalObject;
    functions_.GetLocalInt = &GetLocalInt;
    functions_.GetLocalLong = &GetLocalLong;
    functions_.GetLocalFloat = &GetLocalFloat;
    functions_.GetLocalDouble = &GetLocalDouble;
    functions_.SetBreakpoint = &SetBreakpoint;
    functions_.ClearBreakpoint = &ClearBreakpoint;
    functions_.Deallocate = &Deallocate;
    functions_.GetClassSignature = &GetClassSignature;
    functions_.GetClassStatus = &GetClassStatus;
    functions_.GetSourceFileName = &GetSourceFileName;
    functions_.GetClassModifiers = &GetClassModifiers;
    functions_.GetClassMethods = &GetClassMethods;
    functions_.GetClassFields = &GetClassFields;
    functions_.GetImplementedInterfaces = &GetImplementedInterfaces;
    functions_.IsInterface = &IsInterface;
    functions_.IsArrayClass = &IsArrayClass;
    functions_.GetClassLoader = &GetClassLoader;
    functions_.GetObjectHashCode = &GetObjectHashCode;
    functions_.GetObjectMonitorUsage = &GetObjectMonitorUsage;
    functions_.GetFieldName = &GetFieldName;
    functions_.GetFieldDeclaringClass = &GetFieldDeclaringClass;
    functions_.GetFieldModifiers = &GetFieldModifiers;
    functions_.IsFieldSynthetic = &IsFieldSynthetic;
    functions_.GetMethodName = &GetMethodName;
    functions_.GetMethodDeclaringClass = &GetMethodDeclaringClass;
    functions_.GetMethodModifiers = &GetMethodModifiers;
    functions_.GetMaxLocals = &GetMaxLocals;
    functions_.GetArgumentsSize = &GetArgumentsSize;
    functions_.GetLineNumberTable = &GetLineNumberTable;
    functions_.GetMethodLocation = &GetMethodLocation;
    functions_.GetLocalVariableTable = &GetLocalVariableTable;
    functions_.GetLoadedClasses = &GetLoadedClasses;
    functions_.GetAllStackTraces = &GetAllStackTraces;
    functions_.GetThreadListStackTraces = &GetThreadListStackTraces;
    functions_.GetThreadLocalStorage = &GetThreadLocalStorage;
    functions_.SetThreadLocalStorage = &SetThreadLocalStorage;
    functions_.GetStackTrace = &GetStackTrace;
    functions_.AddCapabilities = &AddCapabilities;
    functions_.GetFrameLocation = &GetFrameLocation;
    functions_.GetErrorName = &GetErrorName;
  }

  virtual ~MockableJvmtiEnv() { }

  virtual jvmtiError SetEventNotificationMode(
      jvmtiEventMode mode,
      jvmtiEvent event_type,
      jthread event_thread) = 0;

  virtual jvmtiError RunAgentThread(
      jthread thread,
      jvmtiStartFunction proc,
      const void* arg,
      jint priority) = 0;

  virtual jvmtiError GetLocalObject(
      jthread thread,
      jint depth,
      jint slot,
      jobject* value_ptr) = 0;

  virtual jvmtiError GetLocalInt(
      jthread thread,
      jint depth,
      jint slot,
      jint* value_ptr) = 0;

  virtual jvmtiError GetLocalLong(
      jthread thread,
      jint depth,
      jint slot,
      jlong* value_ptr) = 0;

  virtual jvmtiError GetLocalFloat(
      jthread thread,
      jint depth,
      jint slot,
      jfloat* value_ptr) = 0;

  virtual jvmtiError GetLocalDouble(
      jthread thread,
      jint depth,
      jint slot,
      jdouble* value_ptr) = 0;

  virtual jvmtiError SetBreakpoint(
      jmethodID method,
      jlocation location) = 0;

  virtual jvmtiError ClearBreakpoint(
      jmethodID method,
      jlocation location) = 0;

  virtual jvmtiError Deallocate(
      unsigned char* mem) = 0;

  virtual jvmtiError GetClassSignature(
      jclass klass,
      char** signature_ptr,
      char** generic_ptr) = 0;

  virtual jvmtiError GetClassStatus(
      jclass klass,
      jint* status_ptr) = 0;

  virtual jvmtiError GetSourceFileName(
      jclass klass,
      char** source_name_ptr) = 0;

  virtual jvmtiError GetClassModifiers(
      jclass klass,
      jint* modifiers_ptr) = 0;

  virtual jvmtiError GetClassMethods(
      jclass klass,
      jint* method_count_ptr,
      jmethodID** methods_ptr) = 0;

  virtual jvmtiError GetClassFields(
      jclass klass,
      jint* field_count_ptr,
      jfieldID** fields_ptr) = 0;

  virtual jvmtiError GetImplementedInterfaces(
      jclass klass,
      jint* interface_count_ptr,
      jclass** interfaces_ptr) = 0;

  virtual jvmtiError IsInterface(
      jclass klass,
      jboolean* is_interface_ptr) = 0;

  virtual jvmtiError IsArrayClass(
      jclass klass,
      jboolean* is_array_class_ptr) = 0;

  virtual jvmtiError GetClassLoader(
      jclass klass,
      jobject* classloader_ptr) = 0;

  virtual jvmtiError GetObjectHashCode(
      jobject object,
      jint* hash_code_ptr) = 0;

  virtual jvmtiError GetObjectMonitorUsage(
      jobject object,
      jvmtiMonitorUsage* info_ptr) = 0;

  virtual jvmtiError GetFieldName(
      jclass klass,
      jfieldID field,
      char** name_ptr,
      char** signature_ptr,
      char** generic_ptr) = 0;

  virtual jvmtiError GetFieldDeclaringClass(
      jclass klass,
      jfieldID field,
      jclass* declaring_class_ptr) = 0;

  virtual jvmtiError GetFieldModifiers(
      jclass klass,
      jfieldID field,
      jint* modifiers_ptr) = 0;

  virtual jvmtiError IsFieldSynthetic(
      jclass klass,
      jfieldID field,
      jboolean* is_synthetic_ptr) = 0;

  virtual jvmtiError GetMethodName(
      jmethodID method,
      char** name_ptr,
      char** signature_ptr,
      char** generic_ptr) = 0;

  virtual jvmtiError GetMethodDeclaringClass(
      jmethodID method,
      jclass* declaring_class_ptr) = 0;

  virtual jvmtiError GetMethodModifiers(
      jmethodID method,
      jint* modifiers_ptr) = 0;

  virtual jvmtiError GetMaxLocals(
      jmethodID method,
      jint* max_ptr) = 0;

  virtual jvmtiError GetArgumentsSize(
      jmethodID method,
      jint* size_ptr) = 0;

  virtual jvmtiError GetLineNumberTable(
      jmethodID method,
      jint* entry_count_ptr,
      jvmtiLineNumberEntry** table_ptr) = 0;

  virtual jvmtiError GetMethodLocation(
      jmethodID method,
      jlocation* start_location_ptr,
      jlocation* end_location_ptr) = 0;

  virtual jvmtiError GetLocalVariableTable(
      jmethodID method,
      jint* entry_count_ptr,
      jvmtiLocalVariableEntry** table_ptr) = 0;

  virtual jvmtiError GetLoadedClasses(
      jint* class_count_ptr,
      jclass** classes_ptr) = 0;

  virtual jvmtiError GetAllStackTraces(
      jint max_frame_count,
      jvmtiStackInfo** stack_info_ptr,
      jint* thread_count_ptr) = 0;

  virtual jvmtiError GetThreadListStackTraces(
      jint thread_count,
      const jthread* thread_list,
      jint max_frame_count,
      jvmtiStackInfo** stack_info_ptr) = 0;

  virtual jvmtiError GetThreadLocalStorage(
      jthread thread,
      void** data_ptr) = 0;

  virtual jvmtiError SetThreadLocalStorage(
      jthread thread,
      const void* data) = 0;

  virtual jvmtiError GetStackTrace(
      jthread thread,
      jint start_depth,
      jint max_frame_count,
      jvmtiFrameInfo* frame_buffer,
      jint* count_ptr) = 0;

  virtual jvmtiError AddCapabilities(
      const jvmtiCapabilities* capabilities_ptr) = 0;

  virtual jvmtiError GetFrameLocation(
      jthread thread,
      jint depth,
      jmethodID* method_ptr,
      jlocation* location_ptr) = 0;

  virtual jvmtiError GetErrorName(
      jvmtiError error,
      char** name_ptr) = 0;

 private:
  static jvmtiError JNICALL SetEventNotificationMode(
      jvmtiEnv* env,
      jvmtiEventMode mode,
      jvmtiEvent event_type,
      jthread event_thread,
      ...) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->SetEventNotificationMode(mode, event_type, event_thread);
  }

  static jvmtiError JNICALL RunAgentThread(
      jvmtiEnv* env,
      jthread thread,
      jvmtiStartFunction proc,
      const void* arg,
      jint priority) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->RunAgentThread(thread, proc, arg, priority);
  }

  static jvmtiError JNICALL GetLocalObject(
      jvmtiEnv* env,
      jthread thread,
      jint depth,
      jint slot,
      jobject* value_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetLocalObject(thread, depth, slot, value_ptr);
  }

  static jvmtiError JNICALL GetLocalInt(
      jvmtiEnv* env,
      jthread thread,
      jint depth,
      jint slot,
      jint* value_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetLocalInt(thread, depth, slot, value_ptr);
  }

  static jvmtiError JNICALL GetLocalLong(
      jvmtiEnv* env,
      jthread thread,
      jint depth,
      jint slot,
      jlong* value_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetLocalLong(thread, depth, slot, value_ptr);
  }

  static jvmtiError JNICALL GetLocalFloat(
      jvmtiEnv* env,
      jthread thread,
      jint depth,
      jint slot,
      jfloat* value_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetLocalFloat(thread, depth, slot, value_ptr);
  }

  static jvmtiError JNICALL GetLocalDouble(
      jvmtiEnv* env,
      jthread thread,
      jint depth,
      jint slot,
      jdouble* value_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetLocalDouble(thread, depth, slot, value_ptr);
  }

  static jvmtiError JNICALL SetBreakpoint(
      jvmtiEnv* env,
      jmethodID method,
      jlocation location) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->SetBreakpoint(method, location);
  }

  static jvmtiError JNICALL ClearBreakpoint(
      jvmtiEnv* env,
      jmethodID method,
      jlocation location) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->ClearBreakpoint(method, location);
  }

  static jvmtiError JNICALL Deallocate(
      jvmtiEnv* env,
      unsigned char* mem) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->Deallocate(mem);
  }

  static jvmtiError JNICALL GetClassSignature(
      jvmtiEnv* env,
      jclass klass,
      char** signature_ptr,
      char** generic_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetClassSignature(klass, signature_ptr, generic_ptr);
  }

  static jvmtiError JNICALL GetClassStatus(
      jvmtiEnv* env,
      jclass klass,
      jint* status_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetClassStatus(klass, status_ptr);
  }

  static jvmtiError JNICALL GetSourceFileName(
      jvmtiEnv* env,
      jclass klass,
      char** source_name_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetSourceFileName(klass, source_name_ptr);
  }

  static jvmtiError JNICALL GetClassModifiers(
      jvmtiEnv* env,
      jclass klass,
      jint* modifiers_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetClassModifiers(klass, modifiers_ptr);
  }

  static jvmtiError JNICALL GetClassMethods(
      jvmtiEnv* env,
      jclass klass,
      jint* method_count_ptr,
      jmethodID** methods_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetClassMethods(klass, method_count_ptr, methods_ptr);
  }

  static jvmtiError JNICALL GetClassFields(
      jvmtiEnv* env,
      jclass klass,
      jint* field_count_ptr,
      jfieldID** fields_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetClassFields(klass, field_count_ptr, fields_ptr);
  }

  static jvmtiError JNICALL GetImplementedInterfaces(
      jvmtiEnv* env,
      jclass klass,
      jint* interface_count_ptr,
      jclass** interfaces_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetImplementedInterfaces(
        klass,
        interface_count_ptr,
        interfaces_ptr);
  }

  static jvmtiError JNICALL IsInterface(
      jvmtiEnv* env,
      jclass klass,
      jboolean* is_interface_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->IsInterface(klass, is_interface_ptr);
  }

  static jvmtiError JNICALL IsArrayClass(
      jvmtiEnv* env,
      jclass klass,
      jboolean* is_array_class_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->IsArrayClass(klass, is_array_class_ptr);
  }

  static jvmtiError JNICALL GetClassLoader(
      jvmtiEnv* env,
      jclass klass,
      jobject* classloader_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetClassLoader(klass, classloader_ptr);
  }

  static jvmtiError JNICALL GetObjectHashCode(
      jvmtiEnv* env,
      jobject object,
      jint* hash_code_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetObjectHashCode(object, hash_code_ptr);
  }

  static jvmtiError JNICALL GetObjectMonitorUsage(
      jvmtiEnv* env,
      jobject object,
      jvmtiMonitorUsage* info_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetObjectMonitorUsage(object, info_ptr);
  }

  static jvmtiError JNICALL GetFieldName(
      jvmtiEnv* env,
      jclass klass,
      jfieldID field,
      char** name_ptr,
      char** signature_ptr,
      char** generic_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetFieldName(klass, field, name_ptr, signature_ptr, generic_ptr);
  }

  static jvmtiError JNICALL GetFieldDeclaringClass(
      jvmtiEnv* env,
      jclass klass,
      jfieldID field,
      jclass* declaring_class_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetFieldDeclaringClass(klass, field, declaring_class_ptr);
  }

  static jvmtiError JNICALL GetFieldModifiers(
      jvmtiEnv* env,
      jclass klass,
      jfieldID field,
      jint* modifiers_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetFieldModifiers(klass, field, modifiers_ptr);
  }

  static jvmtiError JNICALL IsFieldSynthetic(
      jvmtiEnv* env,
      jclass klass,
      jfieldID field,
      jboolean* is_synthetic_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->IsFieldSynthetic(klass, field, is_synthetic_ptr);
  }

  static jvmtiError JNICALL GetMethodName(
      jvmtiEnv* env,
      jmethodID method,
      char** name_ptr,
      char** signature_ptr,
      char** generic_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetMethodName(method, name_ptr, signature_ptr, generic_ptr);
  }

  static jvmtiError JNICALL GetMethodDeclaringClass(
      jvmtiEnv* env,
      jmethodID method,
      jclass* declaring_class_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetMethodDeclaringClass(method, declaring_class_ptr);
  }

  static jvmtiError JNICALL GetMethodModifiers(
      jvmtiEnv* env,
      jmethodID method,
      jint* modifiers_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetMethodModifiers(method, modifiers_ptr);
  }

  static jvmtiError JNICALL GetMaxLocals(
      jvmtiEnv* env,
      jmethodID method,
      jint* max_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetMaxLocals(method, max_ptr);
  }

  static jvmtiError JNICALL GetArgumentsSize(
      jvmtiEnv* env,
      jmethodID method,
      jint* size_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetArgumentsSize(method, size_ptr);
  }

  static jvmtiError JNICALL GetLineNumberTable(
      jvmtiEnv* env,
      jmethodID method,
      jint* entry_count_ptr,
      jvmtiLineNumberEntry** table_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetLineNumberTable(method, entry_count_ptr, table_ptr);
  }

  static jvmtiError JNICALL GetMethodLocation(
      jvmtiEnv* env,
      jmethodID method,
      jlocation* start_location_ptr,
      jlocation* end_location_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetMethodLocation(method, start_location_ptr, end_location_ptr);
  }

  static jvmtiError JNICALL GetLocalVariableTable(
      jvmtiEnv* env,
      jmethodID method,
      jint* entry_count_ptr,
      jvmtiLocalVariableEntry** table_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetLocalVariableTable(method, entry_count_ptr, table_ptr);
  }

  static jvmtiError JNICALL GetLoadedClasses(
      jvmtiEnv* env,
      jint* class_count_ptr,
      jclass** classes_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetLoadedClasses(class_count_ptr, classes_ptr);
  }

  static jvmtiError JNICALL GetAllStackTraces(
      jvmtiEnv* env,
      jint max_frame_count,
      jvmtiStackInfo** stack_info_ptr,
      jint* thread_count_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetAllStackTraces(
        max_frame_count,
        stack_info_ptr,
        thread_count_ptr);
  }

  static jvmtiError JNICALL GetThreadListStackTraces(
      jvmtiEnv* env,
      jint thread_count,
      const jthread* thread_list,
      jint max_frame_count,
      jvmtiStackInfo** stack_info_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetThreadListStackTraces(
        thread_count,
        thread_list,
        max_frame_count,
        stack_info_ptr);
  }

  static jvmtiError JNICALL GetThreadLocalStorage(
      jvmtiEnv* env,
      jthread thread,
      void** data_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetThreadLocalStorage(thread, data_ptr);
  }

  static jvmtiError JNICALL SetThreadLocalStorage(
      jvmtiEnv* env,
      jthread thread,
      const void* data) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->SetThreadLocalStorage(thread, data);
  }

  static jvmtiError JNICALL GetStackTrace(
      jvmtiEnv* env,
      jthread thread,
      jint start_depth,
      jint max_frame_count,
      jvmtiFrameInfo* frame_buffer,
      jint* count_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetStackTrace(
        thread,
        start_depth,
        max_frame_count,
        frame_buffer,
        count_ptr);
  }

  static jvmtiError JNICALL AddCapabilities(
      jvmtiEnv* env,
      const jvmtiCapabilities* capabilities_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->AddCapabilities(capabilities_ptr);
  }

  static jvmtiError JNICALL GetFrameLocation(
      jvmtiEnv* env,
      jthread thread,
      jint depth,
      jmethodID* method_ptr,
      jlocation* location_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetFrameLocation(thread, depth, method_ptr, location_ptr);
  }

  static jvmtiError JNICALL GetErrorName(
      jvmtiEnv* env,
      jvmtiError error,
      char** name_ptr) {
    MockableJvmtiEnv* p = static_cast<MockableJvmtiEnv*>(env);
    return p->GetErrorName(error, name_ptr);
  }

 private:
  jvmtiInterface_1_ functions_;
};


class MockJvmtiEnv : public MockableJvmtiEnv {
 public:
  MOCK_METHOD(jvmtiError, SetEventNotificationMode,
              (jvmtiEventMode, jvmtiEvent, jthread), (override));

  MOCK_METHOD(jvmtiError, RunAgentThread,
              (jthread, jvmtiStartFunction, const void* arg, jint), (override));

  MOCK_METHOD(jvmtiError, GetLocalObject, (jthread, jint, jint, jobject*),
              (override));

  MOCK_METHOD(jvmtiError, GetLocalInt, (jthread, jint, jint, jint*),
              (override));

  MOCK_METHOD(jvmtiError, GetLocalLong, (jthread, jint, jint, jlong*),
              (override));

  MOCK_METHOD(jvmtiError, GetLocalFloat, (jthread, jint, jint, jfloat*),
              (override));

  MOCK_METHOD(jvmtiError, GetLocalDouble, (jthread, jint, jint, jdouble*),
              (override));

  MOCK_METHOD(jvmtiError, SetBreakpoint, (jmethodID, jlocation), (override));

  MOCK_METHOD(jvmtiError, ClearBreakpoint, (jmethodID, jlocation), (override));

  MOCK_METHOD(jvmtiError, Deallocate, (unsigned char*), (override));

  MOCK_METHOD(jvmtiError, GetClassSignature, (jclass, char**, char**),
              (override));

  MOCK_METHOD(jvmtiError, GetClassStatus, (jclass, jint*), (override));

  MOCK_METHOD(jvmtiError, GetSourceFileName, (jclass, char**), (override));

  MOCK_METHOD(jvmtiError, GetClassModifiers, (jclass, jint*), (override));

  MOCK_METHOD(jvmtiError, GetClassMethods, (jclass, jint*, jmethodID**),
              (override));

  MOCK_METHOD(jvmtiError, GetClassFields, (jclass, jint*, jfieldID**),
              (override));

  MOCK_METHOD(jvmtiError, GetImplementedInterfaces, (jclass, jint*, jclass**),
              (override));

  MOCK_METHOD(jvmtiError, IsInterface, (jclass, jboolean*), (override));

  MOCK_METHOD(jvmtiError, IsArrayClass, (jclass, jboolean*), (override));

  MOCK_METHOD(jvmtiError, GetClassLoader, (jclass, jobject*), (override));

  MOCK_METHOD(jvmtiError, GetObjectHashCode, (jobject, jint*), (override));

  MOCK_METHOD(jvmtiError, GetObjectMonitorUsage, (jobject, jvmtiMonitorUsage*),
              (override));

  MOCK_METHOD(jvmtiError, GetFieldName,
              (jclass, jfieldID, char**, char**, char**), (override));

  MOCK_METHOD(jvmtiError, GetFieldDeclaringClass, (jclass, jfieldID, jclass*),
              (override));

  MOCK_METHOD(jvmtiError, GetFieldModifiers, (jclass, jfieldID, jint*),
              (override));

  MOCK_METHOD(jvmtiError, IsFieldSynthetic, (jclass, jfieldID, jboolean*),
              (override));

  MOCK_METHOD(jvmtiError, GetMethodName, (jmethodID, char**, char**, char**),
              (override));

  MOCK_METHOD(jvmtiError, GetMethodDeclaringClass, (jmethodID, jclass*),
              (override));

  MOCK_METHOD(jvmtiError, GetMethodModifiers, (jmethodID, jint*), (override));

  MOCK_METHOD(jvmtiError, GetMaxLocals, (jmethodID, jint*), (override));

  MOCK_METHOD(jvmtiError, GetArgumentsSize, (jmethodID, jint*), (override));

  MOCK_METHOD(jvmtiError, GetLineNumberTable,
              (jmethodID, jint*, jvmtiLineNumberEntry**), (override));

  MOCK_METHOD(jvmtiError, GetMethodLocation,
              (jmethodID, jlocation*, jlocation*), (override));

  MOCK_METHOD(jvmtiError, GetLocalVariableTable,
              (jmethodID, jint*, jvmtiLocalVariableEntry**), (override));

  MOCK_METHOD(jvmtiError, GetLoadedClasses, (jint*, jclass**), (override));

  MOCK_METHOD(jvmtiError, GetAllStackTraces, (jint, jvmtiStackInfo**, jint*),
              (override));

  MOCK_METHOD(jvmtiError, GetThreadListStackTraces,
              (jint, const jthread*, jint, jvmtiStackInfo**), (override));

  MOCK_METHOD(jvmtiError, GetThreadLocalStorage, (jthread, void**), (override));

  MOCK_METHOD(jvmtiError, SetThreadLocalStorage, (jthread, const void*),
              (override));

  MOCK_METHOD(jvmtiError, GetStackTrace,
              (jthread, jint, jint, jvmtiFrameInfo*, jint*), (override));

  MOCK_METHOD(jvmtiError, AddCapabilities, (const jvmtiCapabilities*),
              (override));

  MOCK_METHOD(jvmtiError, GetFrameLocation,
              (jthread, jint, jmethodID*, jlocation*), (override));

  MOCK_METHOD(jvmtiError, GetErrorName, (jvmtiError, char**), (override));
};


class GlobalJvmEnv {
 public:
  GlobalJvmEnv(jvmtiEnv* jvmti, JNIEnv* jni);
  ~GlobalJvmEnv();
};


// Helper class to temporarily set global "JNIEnv*" to nullptr.
class GlobalNoJni {
 public:
  GlobalNoJni();
  ~GlobalNoJni();

 private:
  JNIEnv* const original_jni_;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_JVMTI_ENV_H_
