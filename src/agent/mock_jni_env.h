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
    functions_.DeleteGlobalRef = &CallDeleteGlobalRef;
    functions_.DeleteLocalRef = &CallDeleteLocalRef;
    functions_.DeleteWeakGlobalRef = &CallDeleteWeakGlobalRef;
    functions_.ExceptionCheck = &CallExceptionCheck;
    functions_.ExceptionClear = &CallExceptionClear;
    functions_.ExceptionOccurred = &CallExceptionOccurred;
    functions_.FindClass = &CallFindClass;
    functions_.GetArrayLength = &CallGetArrayLength;
    functions_.GetMethodID = &CallGetMethodID;
    functions_.GetObjectArrayElement = &CallGetObjectArrayElement;
    functions_.GetObjectClass = &CallGetObjectClass;
    functions_.GetObjectRefType = &CallGetObjectRefType;
    functions_.GetPrimitiveArrayCritical = &CallGetPrimitiveArrayCritical;
    functions_.GetStaticMethodID = &CallGetStaticMethodID;
    functions_.GetStringCritical = &CallGetStringCritical;
    functions_.GetStringLength = &CallGetStringLength;
    functions_.GetStringUTFChars = &CallGetStringUTFChars;
    functions_.GetStringUTFRegion = &CallGetStringUTFRegion;
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

  virtual void DeleteGlobalRef(jobject gref) = 0;
  virtual void DeleteLocalRef(jobject obj) = 0;
  virtual void DeleteWeakGlobalRef(jweak ref) = 0;
  virtual jboolean ExceptionCheck() = 0;
  virtual void ExceptionClear() = 0;
  virtual jthrowable ExceptionOccurred() = 0;
  virtual jclass FindClass(const char* name) = 0;
  virtual jsize GetArrayLength(jarray array) = 0;
  virtual jmethodID GetMethodID(
      jclass clazz, const char* name, const char* sig) = 0;
  virtual jobject GetObjectArrayElement(jobjectArray array, jsize index) = 0;
  virtual jclass GetObjectClass(jobject obj) = 0;
  virtual jobjectRefType GetObjectRefType(jobject obj) = 0;
  virtual void* GetPrimitiveArrayCritical(jarray array, jboolean* isCopy) = 0;
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

  static jobject JNICALL CallGetObjectArrayElement(
      JNIEnv* env, jobjectArray array, jsize index) {
    return static_cast<MockableJNIEnv*>(env)->GetObjectArrayElement(
        array, index);
  }

  static jmethodID JNICALL CallGetMethodID(
      JNIEnv* env, jclass clazz, const char* name, const char* sig) {
    return static_cast<MockableJNIEnv*>(env)->GetMethodID(clazz, name, sig);
  }

  static jclass JNICALL CallGetObjectClass(JNIEnv* env, jobject obj) {
    return static_cast<MockableJNIEnv*>(env)->GetObjectClass(obj);
  }

  static jobjectRefType JNICALL CallGetObjectRefType(JNIEnv* env, jobject obj) {
    return static_cast<MockableJNIEnv*>(env)->GetObjectRefType(obj);
  }

  static void* JNICALL CallGetPrimitiveArrayCritical(
      JNIEnv* env, jarray array, jboolean* isCopy) {
    return static_cast<MockableJNIEnv*>(env)->GetPrimitiveArrayCritical(
        array, isCopy);
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
  MOCK_METHOD(void, DeleteGlobalRef, (jobject gref), (override));
  MOCK_METHOD(void, DeleteLocalRef, (jobject obj), (override));
  MOCK_METHOD(void, DeleteWeakGlobalRef, (jweak ref), (override));
  MOCK_METHOD(jboolean, ExceptionCheck, (), (override));
  MOCK_METHOD(void, ExceptionClear, (), (override));
  MOCK_METHOD(jthrowable, ExceptionOccurred, (), (override));
  MOCK_METHOD(jclass, FindClass, (const char* name), (override));
  MOCK_METHOD(jsize, GetArrayLength, (jarray array), (override));
  MOCK_METHOD(jmethodID, GetMethodID,
              (jclass clazz, const char* name, const char* sig), (override));
  MOCK_METHOD(jobject, GetObjectArrayElement, (jobjectArray array, jsize index),
              (override));
  MOCK_METHOD(jclass, GetObjectClass, (jobject obj), (override));
  MOCK_METHOD(jobjectRefType, GetObjectRefType, (jobject obj), (override));
  MOCK_METHOD(void*, GetPrimitiveArrayCritical,
              (jarray array, jboolean* isCopy), (override));
  MOCK_METHOD(jmethodID, GetStaticMethodID,
              (jclass clazz, const char* name, const char* sig), (override));
  MOCK_METHOD(const jchar*, GetStringCritical, (jstring str, jboolean* isCopy),
              (override));
  MOCK_METHOD(jsize, GetStringLength, (jstring str), (override));
  MOCK_METHOD(const char*, GetStringUTFChars, (jstring str, jboolean* isCopy),
              (override));
  MOCK_METHOD(void, GetStringUTFRegion,
              (jstring str, jsize start, jsize len, char* buf), (override));
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
