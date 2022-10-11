// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Disclaimer: This is not an official Google product.

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
    functions_.GetObjectRefType = &CallGetObjectRefType;
    functions_.NewGlobalRef = &CallNewGlobalRef;
    functions_.NewLocalRef = &CallNewLocalRef;
    functions_.NewWeakGlobalRef = &CallNewWeakGlobalRef;
  }
  virtual ~MockableJNIEnv() {}

  virtual void DeleteGlobalRef(jobject gref) = 0;
  virtual void DeleteLocalRef(jobject obj) = 0;
  virtual void DeleteWeakGlobalRef(jweak ref) = 0;
  virtual jobjectRefType GetObjectRefType(jobject obj) = 0;
  virtual jobject NewGlobalRef(jobject lobj) = 0;
  virtual jobject NewLocalRef(jobject ref) = 0;
  virtual jweak NewWeakGlobalRef(jobject obj) = 0;

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
  static jobjectRefType JNICALL CallGetObjectRefType(JNIEnv* env, jobject obj) {
    return static_cast<MockableJNIEnv*>(env)->GetObjectRefType(obj);
  }
  static jobject JNICALL CallNewGlobalRef(JNIEnv* env, jobject lobj) {
    return static_cast<MockableJNIEnv*>(env)->NewGlobalRef(lobj);
  }
  static jobject JNICALL CallNewLocalRef(JNIEnv* env, jobject ref) {
    return static_cast<MockableJNIEnv*>(env)->NewLocalRef(ref);
  }
  static jweak JNICALL CallNewWeakGlobalRef(JNIEnv* env, jobject obj) {
    return static_cast<MockableJNIEnv*>(env)->NewWeakGlobalRef(obj);
  }

  JNINativeInterface_ functions_;
};

class MockJNIEnv : public MockableJNIEnv {
 public:
  MOCK_METHOD(void, DeleteGlobalRef, (jobject gref), (override));
  MOCK_METHOD(void, DeleteLocalRef, (jobject obj), (override));
  MOCK_METHOD(void, DeleteWeakGlobalRef, (jweak ref), (override));
  MOCK_METHOD(jobjectRefType, GetObjectRefType, (jobject obj), (override));
  MOCK_METHOD(jobject, NewGlobalRef, (jobject lobj), (override));
  MOCK_METHOD(jobject, NewLocalRef, (jobject ref), (override));
  MOCK_METHOD(jweak, NewWeakGlobalRef, (jobject obj), (override));
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_JNI_ENV_H_
