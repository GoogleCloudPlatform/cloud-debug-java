/**
 * Copyright 2022 Google Inc. All Rights Reserved.
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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_JNI_ENV_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_JNI_ENV_H_

#include <jni.h>

#include "gmock/gmock.h"

namespace devtools {
namespace cdbg {

using InputJvalueArray = const jvalue*;

// JNIEnv interface is structure with pointers to functions, not a pure virtual
// C++ class that can be mocked. To work around this problem, we create a
// a pure virtual function for each JNIEnv function we care about and point
// to it in the function table.
// NOTE: this class does not include all the JNIEnv methods, but only the few
// that are used by the agent.
class MockableJNIEnv : public JNIEnv {
 public:
  MockableJNIEnv() {
    functions = &functions_;
    functions_.CallBooleanMethodA = &CallCallBooleanMethodA;
    functions_.CallByteMethodA = &CallCallByteMethodA;
    functions_.CallCharMethodA = &CallCallCharMethodA;
    functions_.CallDoubleMethodA = &CallCallDoubleMethodA;
    functions_.CallFloatMethodA = &CallCallFloatMethodA;
    functions_.CallIntMethodA = &CallCallIntMethodA;
    functions_.CallLongMethodA = &CallCallLongMethodA;
    functions_.CallNonvirtualBooleanMethodA = &CallCallNonvirtualBooleanMethodA;
    functions_.CallNonvirtualByteMethodA = &CallCallNonvirtualByteMethodA;
    functions_.CallNonvirtualCharMethodA = &CallCallNonvirtualCharMethodA;
    functions_.CallNonvirtualDoubleMethodA = &CallCallNonvirtualDoubleMethodA;
    functions_.CallNonvirtualFloatMethodA = &CallCallNonvirtualFloatMethodA;
    functions_.CallNonvirtualIntMethodA = &CallCallNonvirtualIntMethodA;
    functions_.CallNonvirtualLongMethodA = &CallCallNonvirtualLongMethodA;
    functions_.CallNonvirtualObjectMethodA = &CallCallNonvirtualObjectMethodA;
    functions_.CallNonvirtualShortMethodA = &CallCallNonvirtualShortMethodA;
    functions_.CallNonvirtualVoidMethodA = &CallCallNonvirtualVoidMethodA;
    functions_.CallObjectMethodA = &CallCallObjectMethodA;
    functions_.CallShortMethodA = &CallCallShortMethodA;
    functions_.CallStaticBooleanMethodA = &CallCallStaticBooleanMethodA;
    functions_.CallStaticByteMethodA = &CallCallStaticByteMethodA;
    functions_.CallStaticCharMethodA = &CallCallStaticCharMethodA;
    functions_.CallStaticDoubleMethodA = &CallCallStaticDoubleMethodA;
    functions_.CallStaticFloatMethodA = &CallCallStaticFloatMethodA;
    functions_.CallStaticIntMethodA = &CallCallStaticIntMethodA;
    functions_.CallStaticLongMethodA = &CallCallStaticLongMethodA;
    functions_.CallStaticObjectMethodA = &CallCallStaticObjectMethodA;
    functions_.CallStaticShortMethodA = &CallCallStaticShortMethodA;
    functions_.CallStaticVoidMethodA = &CallCallStaticVoidMethodA;
    functions_.CallVoidMethodA = &CallCallVoidMethodA;
    functions_.DeleteGlobalRef = &CallDeleteGlobalRef;
    functions_.DeleteLocalRef = &CallDeleteLocalRef;
    functions_.DeleteWeakGlobalRef = &CallDeleteWeakGlobalRef;
    functions_.ExceptionCheck = &CallExceptionCheck;
    functions_.ExceptionClear = &CallExceptionClear;
    functions_.ExceptionOccurred = &CallExceptionOccurred;
    functions_.FindClass = &CallFindClass;
    functions_.GetArrayLength = &CallGetArrayLength;
    functions_.GetMethodID = &CallGetMethodID;
    functions_.GetBooleanField = &CallGetBooleanField;
    functions_.GetByteField = &CallGetByteField;
    functions_.GetCharField = &CallGetCharField;
    functions_.GetDoubleField = &CallGetDoubleField;
    functions_.GetFloatField = &CallGetFloatField;
    functions_.GetIntField = &CallGetIntField;
    functions_.GetLongField = &CallGetLongField;
    functions_.GetObjectArrayElement = &CallGetObjectArrayElement;
    functions_.GetObjectClass = &CallGetObjectClass;
    functions_.GetObjectField = &CallGetObjectField;
    functions_.GetObjectRefType = &CallGetObjectRefType;
    functions_.GetPrimitiveArrayCritical = &CallGetPrimitiveArrayCritical;
    functions_.GetShortField = &CallGetShortField;
    functions_.GetStaticBooleanField = &CallGetStaticBooleanField;
    functions_.GetStaticByteField = &CallGetStaticByteField;
    functions_.GetStaticCharField = &CallGetStaticCharField;
    functions_.GetStaticDoubleField = &CallGetStaticDoubleField;
    functions_.GetStaticFloatField = &CallGetStaticFloatField;
    functions_.GetStaticIntField = &CallGetStaticIntField;
    functions_.GetStaticLongField = &CallGetStaticLongField;
    functions_.GetStaticMethodID = &CallGetStaticMethodID;
    functions_.GetStaticObjectField = &CallGetStaticObjectField;
    functions_.GetStaticShortField = &CallGetStaticShortField;
    functions_.GetStringCritical = &CallGetStringCritical;
    functions_.GetStringLength = &CallGetStringLength;
    functions_.GetStringUTFChars = &CallGetStringUTFChars;
    functions_.GetStringUTFRegion = &CallGetStringUTFRegion;
    functions_.GetSuperclass = &CallGetSuperclass;
    functions_.IsAssignableFrom = &CallIsAssignableFrom;
    functions_.IsInstanceOf = &CallIsInstanceOf;
    functions_.IsSameObject = &CallIsSameObject;
    functions_.NewGlobalRef = &CallNewGlobalRef;
    functions_.NewLocalRef = &CallNewLocalRef;
    functions_.NewString = &CallNewString;
    functions_.NewStringUTF = &CallNewStringUTF;
    functions_.NewWeakGlobalRef = &CallNewWeakGlobalRef;
    functions_.ReleasePrimitiveArrayCritical =
        &CallReleasePrimitiveArrayCritical;
    functions_.ReleaseStringCritical = &CallReleaseStringCritical;
    functions_.ReleaseStringUTFChars = &CallReleaseStringUTFChars;
    functions_.Throw = &CallThrow;
  }
  virtual ~MockableJNIEnv() {}

  virtual jboolean CallBooleanMethodA(jobject obj, jmethodID methodID,
                                      InputJvalueArray args) = 0;
  virtual jbyte CallByteMethodA(jobject obj, jmethodID methodID,
                                InputJvalueArray args) = 0;
  virtual jchar CallCharMethodA(jobject obj, jmethodID methodID,
                                InputJvalueArray args) = 0;
  virtual jdouble CallDoubleMethodA(jobject obj, jmethodID methodID,
                                    InputJvalueArray args) = 0;
  virtual jfloat CallFloatMethodA(jobject obj, jmethodID methodID,
                                  InputJvalueArray args) = 0;
  virtual jint CallIntMethodA(jobject obj, jmethodID methodID,
                              InputJvalueArray args) = 0;
  virtual jlong CallLongMethodA(jobject obj, jmethodID methodID,
                                InputJvalueArray args) = 0;
  virtual jboolean CallNonvirtualBooleanMethodA(jobject obj, jclass clazz,
                                                jmethodID methodID,
                                                InputJvalueArray args) = 0;
  virtual jbyte CallNonvirtualByteMethodA(jobject obj, jclass clazz,
                                          jmethodID methodID,
                                          InputJvalueArray args) = 0;
  virtual jchar CallNonvirtualCharMethodA(jobject obj, jclass clazz,
                                          jmethodID methodID,
                                          InputJvalueArray args) = 0;
  virtual jdouble CallNonvirtualDoubleMethodA(jobject obj, jclass clazz,
                                              jmethodID methodID,
                                              InputJvalueArray args) = 0;
  virtual jfloat CallNonvirtualFloatMethodA(jobject obj, jclass clazz,
                                            jmethodID methodID,
                                            InputJvalueArray args) = 0;
  virtual jint CallNonvirtualIntMethodA(jobject obj, jclass clazz,
                                        jmethodID methodID,
                                        InputJvalueArray args) = 0;
  virtual jlong CallNonvirtualLongMethodA(jobject obj, jclass clazz,
                                          jmethodID methodID,
                                          InputJvalueArray args) = 0;
  virtual jobject CallNonvirtualObjectMethodA(jobject obj, jclass clazz,
                                              jmethodID methodID,
                                              InputJvalueArray args) = 0;
  virtual jshort CallNonvirtualShortMethodA(jobject obj, jclass clazz,
                                            jmethodID methodID,
                                            InputJvalueArray args) = 0;
  virtual void CallNonvirtualVoidMethodA(jobject obj, jclass clazz,
                                         jmethodID methodID,
                                         InputJvalueArray args) = 0;
  virtual jobject CallObjectMethodA(jobject obj, jmethodID methodID,
                                    InputJvalueArray args) = 0;
  virtual jshort CallShortMethodA(jobject obj, jmethodID methodID,
                                  InputJvalueArray args) = 0;
  virtual jboolean CallStaticBooleanMethodA(jclass clazz, jmethodID methodID,
                                            InputJvalueArray args) = 0;
  virtual jbyte CallStaticByteMethodA(jclass clazz, jmethodID methodID,
                                      InputJvalueArray args) = 0;
  virtual jchar CallStaticCharMethodA(jclass clazz, jmethodID methodID,
                                      InputJvalueArray args) = 0;
  virtual jdouble CallStaticDoubleMethodA(jclass clazz, jmethodID methodID,
                                          InputJvalueArray args) = 0;
  virtual jfloat CallStaticFloatMethodA(jclass clazz, jmethodID methodID,
                                        InputJvalueArray args) = 0;
  virtual jint CallStaticIntMethodA(jclass clazz, jmethodID methodID,
                                    InputJvalueArray args) = 0;
  virtual jlong CallStaticLongMethodA(jclass clazz, jmethodID methodID,
                                      InputJvalueArray args) = 0;
  virtual jobject CallStaticObjectMethodA(jclass clazz, jmethodID methodID,
                                          InputJvalueArray args) = 0;
  virtual jshort CallStaticShortMethodA(jclass clazz, jmethodID methodID,
                                        InputJvalueArray args) = 0;
  virtual void CallStaticVoidMethodA(jclass cls, jmethodID methodID,
                                     InputJvalueArray args) = 0;
  virtual void CallVoidMethodA(jobject obj, jmethodID methodID,
                               InputJvalueArray args) = 0;
  virtual void DeleteGlobalRef(jobject gref) = 0;
  virtual void DeleteLocalRef(jobject obj) = 0;
  virtual void DeleteWeakGlobalRef(jweak ref) = 0;
  virtual jboolean ExceptionCheck() = 0;
  virtual void ExceptionClear() = 0;
  virtual jthrowable ExceptionOccurred() = 0;
  virtual jclass FindClass(const char* name) = 0;
  virtual jsize GetArrayLength(jarray array) = 0;
  virtual jboolean GetBooleanField(jobject obj, jfieldID fieldID) = 0;
  virtual jbyte GetByteField(jobject obj, jfieldID fieldID) = 0;
  virtual jchar GetCharField(jobject obj, jfieldID fieldID) = 0;
  virtual jdouble GetDoubleField(jobject obj, jfieldID fieldID) = 0;
  virtual jfloat GetFloatField(jobject obj, jfieldID fieldID) = 0;
  virtual jint GetIntField(jobject obj, jfieldID fieldID) = 0;
  virtual jlong GetLongField(jobject obj, jfieldID fieldID) = 0;
  virtual jmethodID GetMethodID(
      jclass clazz, const char* name, const char* sig) = 0;
  virtual jobject GetObjectArrayElement(jobjectArray array, jsize index) = 0;
  virtual jclass GetObjectClass(jobject obj) = 0;
  virtual jobject GetObjectField(jobject obj, jfieldID fieldID) = 0;
  virtual jobjectRefType GetObjectRefType(jobject obj) = 0;
  virtual jshort GetShortField(jobject obj, jfieldID fieldID) = 0;
  virtual jclass GetSuperclass(jclass sub) = 0;
  virtual void* GetPrimitiveArrayCritical(jarray array, jboolean* isCopy) = 0;
  virtual jboolean GetStaticBooleanField(jclass clazz, jfieldID fieldID) = 0;
  virtual jbyte GetStaticByteField(jclass clazz, jfieldID fieldID) = 0;
  virtual jchar GetStaticCharField(jclass clazz, jfieldID fieldID) = 0;
  virtual jdouble GetStaticDoubleField(jclass clazz, jfieldID fieldID) = 0;
  virtual jfloat GetStaticFloatField(jclass clazz, jfieldID fieldID) = 0;
  virtual jint GetStaticIntField(jclass clazz, jfieldID fieldID) = 0;
  virtual jlong GetStaticLongField(jclass clazz, jfieldID fieldID) = 0;
  virtual jobject GetStaticObjectField(jclass clazz, jfieldID fieldID) = 0;
  virtual jshort GetStaticShortField(jclass clazz, jfieldID fieldID) = 0;
  virtual jmethodID GetStaticMethodID(
      jclass clazz, const char* name, const char* sig) = 0;
  virtual const jchar* GetStringCritical(jstring str, jboolean* isCopy) = 0;
  virtual jsize GetStringLength(jstring str) = 0;
  virtual const char* GetStringUTFChars(jstring str, jboolean* isCopy) = 0;
  virtual void GetStringUTFRegion(
      jstring str, jsize start, jsize len, char* buf) = 0;
  virtual jboolean IsAssignableFrom(jclass sub, jclass sup) = 0;
  virtual jboolean IsInstanceOf(jobject obj, jclass clazz) = 0;
  virtual jboolean IsSameObject(jobject obj1, jobject obj2) = 0;
  virtual jobject NewGlobalRef(jobject lobj) = 0;
  virtual jobject NewLocalRef(jobject ref) = 0;
  virtual jstring NewString(const jchar* unicode, jsize len) = 0;
  virtual jstring NewStringUTF(const char* utf) = 0;
  virtual jweak NewWeakGlobalRef(jobject obj) = 0;
  virtual void ReleasePrimitiveArrayCritical(
      jarray array, void* carray, jint mode) = 0;
  virtual void ReleaseStringCritical(jstring str, const jchar* cstring) = 0;
  virtual void ReleaseStringUTFChars(jstring str, const char* chars) = 0;
  virtual jint Throw(jthrowable obj) = 0;

 private:
  static jboolean JNICALL CallCallBooleanMethodA(JNIEnv* env, jobject obj,
                                                 jmethodID methodID,
                                                 InputJvalueArray args) {
    return static_cast<MockableJNIEnv*>(env)->CallBooleanMethodA(
        obj, methodID, args);
  }

  static jbyte JNICALL CallCallByteMethodA(JNIEnv* env, jobject obj,
                                           jmethodID methodID,
                                           InputJvalueArray args) {
    return static_cast<MockableJNIEnv*>(env)->CallByteMethodA(
        obj, methodID, args);
  }

  static jchar JNICALL CallCallCharMethodA(JNIEnv* env, jobject obj,
                                           jmethodID methodID,
                                           InputJvalueArray args) {
    return static_cast<MockableJNIEnv*>(env)->CallCharMethodA(
        obj, methodID, args);
  }

  static jdouble JNICALL CallCallDoubleMethodA(JNIEnv* env, jobject obj,
                                               jmethodID methodID,
                                               InputJvalueArray args) {
    return static_cast<MockableJNIEnv*>(env)->CallDoubleMethodA(
        obj, methodID, args);
  }

  static jfloat JNICALL CallCallFloatMethodA(JNIEnv* env, jobject obj,
                                             jmethodID methodID,
                                             InputJvalueArray args) {
    return static_cast<MockableJNIEnv*>(env)->CallFloatMethodA(
        obj, methodID, args);
  }

  static jint JNICALL CallCallIntMethodA(JNIEnv* env, jobject obj,
                                         jmethodID methodID,
                                         InputJvalueArray args) {
    return static_cast<MockableJNIEnv*>(env)->CallIntMethodA(
        obj, methodID, args);
  }

  static jlong JNICALL CallCallLongMethodA(JNIEnv* env, jobject obj,
                                           jmethodID methodID,
                                           InputJvalueArray args) {
    return static_cast<MockableJNIEnv*>(env)->CallLongMethodA(
        obj, methodID, args);
  }

  static jboolean JNICALL
  CallCallNonvirtualBooleanMethodA(JNIEnv* env, jobject obj, jclass clazz,
                                   jmethodID methodID, InputJvalueArray args) {
    return static_cast<MockableJNIEnv*>(env)->CallNonvirtualBooleanMethodA(
        obj, clazz, methodID, args);
  }

  static jbyte JNICALL CallCallNonvirtualByteMethodA(JNIEnv* env, jobject obj,
                                                     jclass clazz,
                                                     jmethodID methodID,
                                                     InputJvalueArray args) {
    return static_cast<MockableJNIEnv*>(env)->CallNonvirtualByteMethodA(
        obj, clazz, methodID, args);
  }

  static jchar JNICALL CallCallNonvirtualCharMethodA(JNIEnv* env, jobject obj,
                                                     jclass clazz,
                                                     jmethodID methodID,
                                                     InputJvalueArray args) {
    return static_cast<MockableJNIEnv*>(env)->CallNonvirtualCharMethodA(
        obj, clazz, methodID, args);
  }

  static jdouble JNICALL
  CallCallNonvirtualDoubleMethodA(JNIEnv* env, jobject obj, jclass clazz,
                                  jmethodID methodID, InputJvalueArray args) {
    return static_cast<MockableJNIEnv*>(env)->CallNonvirtualDoubleMethodA(
        obj, clazz, methodID, args);
  }

  static jfloat JNICALL CallCallNonvirtualFloatMethodA(JNIEnv* env, jobject obj,
                                                       jclass clazz,
                                                       jmethodID methodID,
                                                       InputJvalueArray args) {
    return static_cast<MockableJNIEnv*>(env)->CallNonvirtualFloatMethodA(
        obj, clazz, methodID, args);
  }

  static jint JNICALL CallCallNonvirtualIntMethodA(JNIEnv* env, jobject obj,
                                                   jclass clazz,
                                                   jmethodID methodID,
                                                   InputJvalueArray args) {
    return static_cast<MockableJNIEnv*>(env)->CallNonvirtualIntMethodA(
        obj, clazz, methodID, args);
  }

  static jlong JNICALL CallCallNonvirtualLongMethodA(JNIEnv* env, jobject obj,
                                                     jclass clazz,
                                                     jmethodID methodID,
                                                     InputJvalueArray args) {
    return static_cast<MockableJNIEnv*>(env)->CallNonvirtualLongMethodA(
        obj, clazz, methodID, args);
  }

  static jobject JNICALL
  CallCallNonvirtualObjectMethodA(JNIEnv* env, jobject obj, jclass clazz,
                                  jmethodID methodID, InputJvalueArray args) {
    return static_cast<MockableJNIEnv*>(env)->CallNonvirtualObjectMethodA(
        obj, clazz, methodID, args);
  }

  static jshort JNICALL CallCallNonvirtualShortMethodA(JNIEnv* env, jobject obj,
                                                       jclass clazz,
                                                       jmethodID methodID,
                                                       InputJvalueArray args) {
    return static_cast<MockableJNIEnv*>(env)->CallNonvirtualShortMethodA(
        obj, clazz, methodID, args);
  }

  static void JNICALL CallCallNonvirtualVoidMethodA(JNIEnv* env, jobject obj,
                                                    jclass clazz,
                                                    jmethodID methodID,
                                                    InputJvalueArray args) {
    static_cast<MockableJNIEnv*>(env)->CallNonvirtualVoidMethodA(
        obj, clazz, methodID, args);
  }

  static jobject JNICALL CallCallObjectMethodA(JNIEnv* env, jobject obj,
                                               jmethodID methodID,
                                               InputJvalueArray args) {
    return static_cast<MockableJNIEnv*>(env)->CallObjectMethodA(
        obj, methodID, args);
  }

  static jshort JNICALL CallCallShortMethodA(JNIEnv* env, jobject obj,
                                             jmethodID methodID,
                                             InputJvalueArray args) {
    return static_cast<MockableJNIEnv*>(env)->CallShortMethodA(
        obj, methodID, args);
  }

  static jboolean JNICALL CallCallStaticBooleanMethodA(JNIEnv* env,
                                                       jclass clazz,
                                                       jmethodID methodID,
                                                       InputJvalueArray args) {
    return static_cast<MockableJNIEnv*>(env)->CallStaticBooleanMethodA(
        clazz, methodID, args);
  }

  static jbyte JNICALL CallCallStaticByteMethodA(JNIEnv* env, jclass clazz,
                                                 jmethodID methodID,
                                                 InputJvalueArray args) {
    return static_cast<MockableJNIEnv*>(env)->CallStaticByteMethodA(
        clazz, methodID, args);
  }

  static jchar JNICALL CallCallStaticCharMethodA(JNIEnv* env, jclass clazz,
                                                 jmethodID methodID,
                                                 InputJvalueArray args) {
    return static_cast<MockableJNIEnv*>(env)->CallStaticCharMethodA(
        clazz, methodID, args);
  }

  static jdouble JNICALL CallCallStaticDoubleMethodA(JNIEnv* env, jclass clazz,
                                                     jmethodID methodID,
                                                     InputJvalueArray args) {
    return static_cast<MockableJNIEnv*>(env)->CallStaticDoubleMethodA(
        clazz, methodID, args);
  }

  static jfloat JNICALL CallCallStaticFloatMethodA(JNIEnv* env, jclass clazz,
                                                   jmethodID methodID,
                                                   InputJvalueArray args) {
    return static_cast<MockableJNIEnv*>(env)->CallStaticFloatMethodA(
        clazz, methodID, args);
  }

  static jint JNICALL CallCallStaticIntMethodA(JNIEnv* env, jclass clazz,
                                               jmethodID methodID,
                                               InputJvalueArray args) {
    return static_cast<MockableJNIEnv*>(env)->CallStaticIntMethodA(
        clazz, methodID, args);
  }

  static jlong JNICALL CallCallStaticLongMethodA(JNIEnv* env, jclass clazz,
                                                 jmethodID methodID,
                                                 InputJvalueArray args) {
    return static_cast<MockableJNIEnv*>(env)->CallStaticLongMethodA(
        clazz, methodID, args);
  }

  static jobject JNICALL CallCallStaticObjectMethodA(JNIEnv* env, jclass clazz,
                                                     jmethodID methodID,
                                                     InputJvalueArray args) {
    return static_cast<MockableJNIEnv*>(env)->CallStaticObjectMethodA(
        clazz, methodID, args);
  }

  static jshort JNICALL CallCallStaticShortMethodA(JNIEnv* env, jclass clazz,
                                                   jmethodID methodID,
                                                   InputJvalueArray args) {
    return static_cast<MockableJNIEnv*>(env)->CallStaticShortMethodA(
        clazz, methodID, args);
  }

  static void JNICALL CallCallStaticVoidMethodA(JNIEnv* env, jclass cls,
                                                jmethodID methodID,
                                                InputJvalueArray args) {
    static_cast<MockableJNIEnv*>(env)->CallStaticVoidMethodA(
        cls, methodID, args);
  }

  static void JNICALL CallCallVoidMethodA(JNIEnv* env, jobject obj,
                                          jmethodID methodID,
                                          InputJvalueArray args) {
    static_cast<MockableJNIEnv*>(env)->CallVoidMethodA(obj, methodID, args);
  }

  static void JNICALL CallDeleteGlobalRef(JNIEnv* env, jobject gref) {
    static_cast<MockableJNIEnv*>(env)->DeleteGlobalRef(gref);
  }

  static void JNICALL CallDeleteLocalRef(JNIEnv* env, jobject obj) {
    static_cast<MockableJNIEnv*>(env)->DeleteLocalRef(obj);
  }

  static void JNICALL CallDeleteWeakGlobalRef(JNIEnv* env, jweak ref) {
    static_cast<MockableJNIEnv*>(env)->DeleteWeakGlobalRef(ref);
  }

  static jboolean JNICALL CallExceptionCheck(JNIEnv* env) {
    return static_cast<MockableJNIEnv*>(env)->ExceptionCheck();
  }

  static void JNICALL CallExceptionClear(JNIEnv* env) {
    static_cast<MockableJNIEnv*>(env)->ExceptionClear();
  }

  static jthrowable JNICALL CallExceptionOccurred(JNIEnv* env) {
    return static_cast<MockableJNIEnv*>(env)->ExceptionOccurred();
  }

  static jclass JNICALL CallFindClass(JNIEnv* env, const char* name) {
    return static_cast<MockableJNIEnv*>(env)->FindClass(name);
  }

  static jsize JNICALL CallGetArrayLength(JNIEnv* env, jarray array) {
    return static_cast<MockableJNIEnv*>(env)->GetArrayLength(array);
  }

  static jboolean JNICALL CallGetBooleanField(
      JNIEnv* env, jobject obj, jfieldID fieldID) {
    return static_cast<MockableJNIEnv*>(env)->GetBooleanField(obj, fieldID);
  }

  static jbyte JNICALL CallGetByteField(
      JNIEnv* env, jobject obj, jfieldID fieldID) {
    return static_cast<MockableJNIEnv*>(env)->GetByteField(obj, fieldID);
  }

  static jchar JNICALL CallGetCharField(
      JNIEnv* env, jobject obj, jfieldID fieldID) {
    return static_cast<MockableJNIEnv*>(env)->GetCharField(obj, fieldID);
  }

  static jdouble JNICALL CallGetDoubleField(
      JNIEnv* env, jobject obj, jfieldID fieldID) {
    return static_cast<MockableJNIEnv*>(env)->GetDoubleField(obj, fieldID);
  }

  static jfloat JNICALL CallGetFloatField(
      JNIEnv* env, jobject obj, jfieldID fieldID) {
    return static_cast<MockableJNIEnv*>(env)->GetFloatField(obj, fieldID);
  }

  static jint JNICALL CallGetIntField(
      JNIEnv* env, jobject obj, jfieldID fieldID) {
    return static_cast<MockableJNIEnv*>(env)->GetIntField(obj, fieldID);
  }

  static jlong JNICALL CallGetLongField(
      JNIEnv* env, jobject obj, jfieldID fieldID) {
    return static_cast<MockableJNIEnv*>(env)->GetLongField(obj, fieldID);
  }

  static jmethodID JNICALL CallGetMethodID(
      JNIEnv* env, jclass clazz, const char* name, const char* sig) {
    return static_cast<MockableJNIEnv*>(env)->GetMethodID(clazz, name, sig);
  }

  static jobject JNICALL CallGetObjectArrayElement(
      JNIEnv* env, jobjectArray array, jsize index) {
    return static_cast<MockableJNIEnv*>(env)->GetObjectArrayElement(
        array, index);
  }

  static jclass JNICALL CallGetObjectClass(JNIEnv* env, jobject obj) {
    return static_cast<MockableJNIEnv*>(env)->GetObjectClass(obj);
  }

  static jobject JNICALL CallGetObjectField(
      JNIEnv* env, jobject obj, jfieldID fieldID) {
    return static_cast<MockableJNIEnv*>(env)->GetObjectField(obj, fieldID);
  }

  static jobjectRefType JNICALL CallGetObjectRefType(JNIEnv* env, jobject obj) {
    return static_cast<MockableJNIEnv*>(env)->GetObjectRefType(obj);
  }

  static void* JNICALL CallGetPrimitiveArrayCritical(
      JNIEnv* env, jarray array, jboolean* isCopy) {
    return static_cast<MockableJNIEnv*>(env)->GetPrimitiveArrayCritical(
        array, isCopy);
  }

  static jshort JNICALL CallGetShortField(
      JNIEnv* env, jobject obj, jfieldID fieldID) {
    return static_cast<MockableJNIEnv*>(env)->GetShortField(obj, fieldID);
  }

  static jboolean JNICALL CallGetStaticBooleanField(
      JNIEnv* env, jclass clazz, jfieldID fieldID) {
    return static_cast<MockableJNIEnv*>(env)->GetStaticBooleanField(
        clazz, fieldID);
  }

  static jbyte JNICALL CallGetStaticByteField(
      JNIEnv* env, jclass clazz, jfieldID fieldID) {
    return static_cast<MockableJNIEnv*>(env)->GetStaticByteField(
        clazz, fieldID);
  }

  static jchar JNICALL CallGetStaticCharField(
      JNIEnv* env, jclass clazz, jfieldID fieldID) {
    return static_cast<MockableJNIEnv*>(env)->GetStaticCharField(
        clazz, fieldID);
  }

  static jdouble JNICALL CallGetStaticDoubleField(
      JNIEnv* env, jclass clazz, jfieldID fieldID) {
    return static_cast<MockableJNIEnv*>(env)->GetStaticDoubleField(
        clazz, fieldID);
  }

  static jfloat JNICALL CallGetStaticFloatField(
      JNIEnv* env, jclass clazz, jfieldID fieldID) {
    return static_cast<MockableJNIEnv*>(env)->GetStaticFloatField(
        clazz, fieldID);
  }

  static jint JNICALL CallGetStaticIntField(
      JNIEnv* env, jclass clazz, jfieldID fieldID) {
    return static_cast<MockableJNIEnv*>(env)->GetStaticIntField(
        clazz, fieldID);
  }

  static jlong JNICALL CallGetStaticLongField(
      JNIEnv* env, jclass clazz, jfieldID fieldID) {
    return static_cast<MockableJNIEnv*>(env)->GetStaticLongField(
        clazz, fieldID);
  }

  static jobject JNICALL CallGetStaticObjectField(
      JNIEnv* env, jclass clazz, jfieldID fieldID) {
    return static_cast<MockableJNIEnv*>(env)->GetStaticObjectField(
        clazz, fieldID);
  }

  static jshort JNICALL CallGetStaticShortField(
      JNIEnv* env, jclass clazz, jfieldID fieldID) {
    return static_cast<MockableJNIEnv*>(env)->GetStaticShortField(
        clazz, fieldID);
  }

  static jmethodID JNICALL CallGetStaticMethodID(
      JNIEnv* env, jclass clazz, const char* name, const char* sig) {
    return static_cast<MockableJNIEnv*>(env)->GetStaticMethodID(
        clazz, name, sig);
  }

  static const jchar* JNICALL CallGetStringCritical(
      JNIEnv* env, jstring str, jboolean* isCopy) {
    return static_cast<MockableJNIEnv*>(env)->GetStringCritical(
        str, isCopy);
  }

  static jsize JNICALL CallGetStringLength(JNIEnv* env, jstring str) {
    return static_cast<MockableJNIEnv*>(env)->GetStringLength(str);
  }

  static const char* JNICALL CallGetStringUTFChars(
      JNIEnv* env, jstring str, jboolean* isCopy) {
    return static_cast<MockableJNIEnv*>(env)->GetStringUTFChars(
        str, isCopy);
  }

  static void JNICALL CallGetStringUTFRegion(
      JNIEnv* env, jstring str, jsize start, jsize len, char* buf) {
    static_cast<MockableJNIEnv*>(env)->GetStringUTFRegion(
        str, start, len, buf);
  }

  static jclass JNICALL CallGetSuperclass(JNIEnv* env, jclass sub) {
    return static_cast<MockableJNIEnv*>(env)->GetSuperclass(sub);
  }

  static jboolean JNICALL CallIsAssignableFrom(
      JNIEnv* env, jclass sub, jclass sup) {
    return static_cast<MockableJNIEnv*>(env)->IsAssignableFrom(sub, sup);
  }

  static jboolean JNICALL CallIsInstanceOf(
      JNIEnv* env, jobject obj, jclass clazz) {
    return static_cast<MockableJNIEnv*>(env)->IsInstanceOf(obj, clazz);
  }

  static jboolean JNICALL CallIsSameObject(
      JNIEnv* env, jobject obj1, jobject obj2) {
    return static_cast<MockableJNIEnv*>(env)->IsSameObject(obj1, obj2);
  }

  static jobject JNICALL CallNewGlobalRef(JNIEnv* env, jobject lobj) {
    return static_cast<MockableJNIEnv*>(env)->NewGlobalRef(lobj);
  }

  static jobject JNICALL CallNewLocalRef(JNIEnv* env, jobject ref) {
    return static_cast<MockableJNIEnv*>(env)->NewLocalRef(ref);
  }

  static jstring JNICALL CallNewString(
      JNIEnv* env, const jchar* unicode, jsize len) {
    return static_cast<MockableJNIEnv*>(env)->NewString(unicode, len);
  }

  static jstring JNICALL CallNewStringUTF(JNIEnv* env, const char* utf) {
    return static_cast<MockableJNIEnv*>(env)->NewStringUTF(utf);
  }

  static jweak JNICALL CallNewWeakGlobalRef(JNIEnv* env, jobject obj) {
    return static_cast<MockableJNIEnv*>(env)->NewWeakGlobalRef(obj);
  }

  static void JNICALL CallReleasePrimitiveArrayCritical(
      JNIEnv* env, jarray array, void* carray, jint mode) {
    static_cast<MockableJNIEnv*>(env)->ReleasePrimitiveArrayCritical(
        array, carray, mode);
  }

  static void JNICALL CallReleaseStringCritical(
      JNIEnv* env, jstring str, const jchar* cstring) {
    static_cast<MockableJNIEnv*>(env)->ReleaseStringCritical(str, cstring);
  }

  static void JNICALL CallReleaseStringUTFChars(
      JNIEnv* env, jstring str, const char* chars) {
    static_cast<MockableJNIEnv*>(env)->ReleaseStringUTFChars(str, chars);
  }

  static jint JNICALL CallThrow(JNIEnv* env, jthrowable obj) {
    return static_cast<MockableJNIEnv*>(env)->Throw(obj);
  }

  JNINativeInterface_ functions_;
};

class MockJNIEnv : public MockableJNIEnv {
 public:
  MOCK_METHOD(jboolean, CallBooleanMethodA,
              (jobject obj, jmethodID methodID, InputJvalueArray args),
              (override));
  MOCK_METHOD(jbyte, CallByteMethodA,
              (jobject obj, jmethodID methodID, InputJvalueArray args),
              (override));
  MOCK_METHOD(jchar, CallCharMethodA,
              (jobject obj, jmethodID methodID, InputJvalueArray args),
              (override));
  MOCK_METHOD(jdouble, CallDoubleMethodA,
              (jobject obj, jmethodID methodID, InputJvalueArray args),
              (override));
  MOCK_METHOD(jfloat, CallFloatMethodA,
              (jobject obj, jmethodID methodID, InputJvalueArray args),
              (override));
  MOCK_METHOD(jint, CallIntMethodA,
              (jobject obj, jmethodID methodID, InputJvalueArray args),
              (override));
  MOCK_METHOD(jlong, CallLongMethodA,
              (jobject obj, jmethodID methodID, InputJvalueArray args),
              (override));
  MOCK_METHOD(jboolean, CallNonvirtualBooleanMethodA,
              (jobject obj, jclass clazz, jmethodID methodID,
               InputJvalueArray args),
              (override));
  MOCK_METHOD(jbyte, CallNonvirtualByteMethodA,
              (jobject obj, jclass clazz, jmethodID methodID,
               InputJvalueArray args),
              (override));
  MOCK_METHOD(jchar, CallNonvirtualCharMethodA,
              (jobject obj, jclass clazz, jmethodID methodID,
               InputJvalueArray args),
              (override));
  MOCK_METHOD(jdouble, CallNonvirtualDoubleMethodA,
              (jobject obj, jclass clazz, jmethodID methodID,
               InputJvalueArray args),
              (override));
  MOCK_METHOD(jfloat, CallNonvirtualFloatMethodA,
              (jobject obj, jclass clazz, jmethodID methodID,
               InputJvalueArray args),
              (override));
  MOCK_METHOD(jint, CallNonvirtualIntMethodA,
              (jobject obj, jclass clazz, jmethodID methodID,
               InputJvalueArray args),
              (override));
  MOCK_METHOD(jlong, CallNonvirtualLongMethodA,
              (jobject obj, jclass clazz, jmethodID methodID,
               InputJvalueArray args),
              (override));
  MOCK_METHOD(jobject, CallNonvirtualObjectMethodA,
              (jobject obj, jclass clazz, jmethodID methodID,
               InputJvalueArray args),
              (override));
  MOCK_METHOD(jshort, CallNonvirtualShortMethodA,
              (jobject obj, jclass clazz, jmethodID methodID,
               InputJvalueArray args),
              (override));
  MOCK_METHOD(void, CallNonvirtualVoidMethodA,
              (jobject obj, jclass clazz, jmethodID methodID,
               InputJvalueArray args),
              (override));
  MOCK_METHOD(jobject, CallObjectMethodA,
              (jobject obj, jmethodID methodID, InputJvalueArray args),
              (override));
  MOCK_METHOD(jshort, CallShortMethodA,
              (jobject obj, jmethodID methodID, InputJvalueArray args),
              (override));
  MOCK_METHOD(jboolean, CallStaticBooleanMethodA,
              (jclass clazz, jmethodID methodID, InputJvalueArray args),
              (override));
  MOCK_METHOD(jbyte, CallStaticByteMethodA,
              (jclass clazz, jmethodID methodID, InputJvalueArray args),
              (override));
  MOCK_METHOD(jchar, CallStaticCharMethodA,
              (jclass clazz, jmethodID methodID, InputJvalueArray args),
              (override));
  MOCK_METHOD(jdouble, CallStaticDoubleMethodA,
              (jclass clazz, jmethodID methodID, InputJvalueArray args),
              (override));
  MOCK_METHOD(jfloat, CallStaticFloatMethodA,
              (jclass clazz, jmethodID methodID, InputJvalueArray args),
              (override));
  MOCK_METHOD(jint, CallStaticIntMethodA,
              (jclass clazz, jmethodID methodID, InputJvalueArray args),
              (override));
  MOCK_METHOD(jlong, CallStaticLongMethodA,
              (jclass clazz, jmethodID methodID, InputJvalueArray args),
              (override));
  MOCK_METHOD(jobject, CallStaticObjectMethodA,
              (jclass clazz, jmethodID methodID, InputJvalueArray args),
              (override));
  MOCK_METHOD(jshort, CallStaticShortMethodA,
              (jclass clazz, jmethodID methodID, InputJvalueArray args),
              (override));
  MOCK_METHOD(void, CallStaticVoidMethodA,
              (jclass cls, jmethodID methodID, InputJvalueArray args),
              (override));
  MOCK_METHOD(void, CallVoidMethodA,
              (jobject obj, jmethodID methodID, InputJvalueArray args),
              (override));
  MOCK_METHOD(void, DeleteGlobalRef, (jobject gref), (override));
  MOCK_METHOD(void, DeleteLocalRef, (jobject obj), (override));
  MOCK_METHOD(void, DeleteWeakGlobalRef, (jweak ref), (override));
  MOCK_METHOD(jboolean, ExceptionCheck, (), (override));
  MOCK_METHOD(void, ExceptionClear, (), (override));
  MOCK_METHOD(jthrowable, ExceptionOccurred, (), (override));
  MOCK_METHOD(jclass, FindClass, (const char* name), (override));
  MOCK_METHOD(jsize, GetArrayLength, (jarray array), (override));
  MOCK_METHOD(jboolean, GetBooleanField, (jobject obj, jfieldID fieldID),
              (override));
  MOCK_METHOD(jbyte, GetByteField, (jobject obj, jfieldID fieldID), (override));
  MOCK_METHOD(jchar, GetCharField, (jobject obj, jfieldID fieldID), (override));
  MOCK_METHOD(jdouble, GetDoubleField, (jobject obj, jfieldID fieldID),
              (override));
  MOCK_METHOD(jfloat, GetFloatField, (jobject obj, jfieldID fieldID),
              (override));
  MOCK_METHOD(jint, GetIntField, (jobject obj, jfieldID fieldID), (override));
  MOCK_METHOD(jlong, GetLongField, (jobject obj, jfieldID fieldID), (override));
  MOCK_METHOD(jmethodID, GetMethodID,
              (jclass clazz, const char* name, const char* sig), (override));
  MOCK_METHOD(jobject, GetObjectArrayElement, (jobjectArray array, jsize index),
              (override));
  MOCK_METHOD(jclass, GetObjectClass, (jobject obj), (override));
  MOCK_METHOD(jobject, GetObjectField, (jobject obj, jfieldID fieldID),
              (override));
  MOCK_METHOD(jobjectRefType, GetObjectRefType, (jobject obj), (override));
  MOCK_METHOD(void*, GetPrimitiveArrayCritical,
              (jarray array, jboolean* isCopy), (override));
  MOCK_METHOD(jshort, GetShortField, (jobject obj, jfieldID fieldID),
              (override));
  MOCK_METHOD(jboolean, GetStaticBooleanField, (jclass clazz, jfieldID fieldID),
              (override));
  MOCK_METHOD(jbyte, GetStaticByteField, (jclass clazz, jfieldID fieldID),
              (override));
  MOCK_METHOD(jchar, GetStaticCharField, (jclass clazz, jfieldID fieldID),
              (override));
  MOCK_METHOD(jdouble, GetStaticDoubleField, (jclass clazz, jfieldID fieldID),
              (override));
  MOCK_METHOD(jfloat, GetStaticFloatField, (jclass clazz, jfieldID fieldID),
              (override));
  MOCK_METHOD(jint, GetStaticIntField, (jclass clazz, jfieldID fieldID),
              (override));
  MOCK_METHOD(jlong, GetStaticLongField, (jclass clazz, jfieldID fieldID),
              (override));
  MOCK_METHOD(jobject, GetStaticObjectField, (jclass clazz, jfieldID fieldID),
              (override));
  MOCK_METHOD(jshort, GetStaticShortField, (jclass clazz, jfieldID fieldID),
              (override));
  MOCK_METHOD(jmethodID, GetStaticMethodID,
              (jclass clazz, const char* name, const char* sig), (override));
  MOCK_METHOD(const jchar*, GetStringCritical, (jstring str, jboolean* isCopy),
              (override));
  MOCK_METHOD(jsize, GetStringLength, (jstring str), (override));
  MOCK_METHOD(const char*, GetStringUTFChars, (jstring str, jboolean* isCopy),
              (override));
  MOCK_METHOD(void, GetStringUTFRegion,
              (jstring str, jsize start, jsize len, char* buf), (override));
  MOCK_METHOD(jclass, GetSuperclass, (jclass sub), (override));
  MOCK_METHOD(jboolean, IsAssignableFrom, (jclass sub, jclass sup), (override));
  MOCK_METHOD(jboolean, IsInstanceOf, (jobject obj, jclass clazz), (override));
  MOCK_METHOD(jboolean, IsSameObject, (jobject obj1, jobject obj2), (override));
  MOCK_METHOD(jobject, NewGlobalRef, (jobject lobj), (override));
  MOCK_METHOD(jobject, NewLocalRef, (jobject ref), (override));
  MOCK_METHOD(jstring, NewString, (const jchar* unicode, jsize len),
              (override));
  MOCK_METHOD(jstring, NewStringUTF, (const char* utf), (override));
  MOCK_METHOD(jweak, NewWeakGlobalRef, (jobject obj), (override));
  MOCK_METHOD(void, ReleasePrimitiveArrayCritical,
              (jarray array, void* carray, jint mode), (override));
  MOCK_METHOD(void, ReleaseStringCritical, (jstring str, const jchar* cstring),
              (override));
  MOCK_METHOD(void, ReleaseStringUTFChars, (jstring str, const char* chars),
              (override));
  MOCK_METHOD(jint, Throw, (jthrowable obj), (override));
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_JNI_ENV_H_
