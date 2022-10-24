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

#include "src/agent/jni_method_caller.h"

#include <cstdint>

#include "gtest/gtest.h"

#include "src/agent/class_metadata_reader.h"
#include "tests/agent/fake_jni.h"
#include "src/agent/jni_utils.h"
#include "tests/agent/mock_jni_env.h"
#include "tests/agent/mock_jvmti_env.h"

using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::NiceMock;
using testing::NotNull;
using testing::Return;
using testing::StrictMock;
using testing::WithArgs;
using testing::WithoutArgs;

namespace devtools {
namespace cdbg {

class JniMethodCallerTest : public ::testing::Test {
 protected:
  JniMethodCallerTest()
      : fake_jni_(&jvmti_, &jni_),
        global_jvm_(&jvmti_, &jni_) {
  }

  jmethodID RegisterMethod(const ClassMetadataReader::Method& method) {
    static uint64_t unique_method_id = 0x567000;

    FakeJni::ClassMetadata& class_metadata =
        fake_jni_.MutableClassMetadata(
            fake_jni_.GetStockClass(FakeJni::StockClass::MyClass1));

    jmethodID method_id = reinterpret_cast<jmethodID>(unique_method_id++);
    class_metadata.methods.push_back({
      method_id,  // id
      method,     // metadata
      {}          // line_number_table
    });

    return method_id;
  }

  bool Bind(
      JniMethodCaller* method_caller,
      const ClassMetadataReader::Method& method) {
    return method_caller->Bind(
        fake_jni_.GetStockClass(FakeJni::StockClass::MyClass1),
        method);
  }

 protected:
  NiceMock<MockJvmtiEnv> jvmti_;
  NiceMock<MockJNIEnv> jni_;
  FakeJni fake_jni_;
  GlobalJvmEnv global_jvm_;
};


TEST_F(JniMethodCallerTest, InstanceMethodBindSuccess) {
  auto method = InstanceMethod("Lcom/prod/MyClass1;", "instanceMethod", "()V");
  RegisterMethod(method);

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));
}


TEST_F(JniMethodCallerTest, StaticMethodBindSuccess) {
  auto method = StaticMethod("Lcom/prod/MyClass1;", "staticMethod", "()V");
  RegisterMethod(method);

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));
}


TEST_F(JniMethodCallerTest, BadMethodSignature) {
  ClassMetadataReader::Method method;
  method.class_signature = { JType::Object, "LMyClass;" };
  method.name = "myMethod";
  method.signature = "()";
  method.modifiers = 0;

  JniMethodCaller caller;
  EXPECT_FALSE(Bind(&caller, method));
}


TEST_F(JniMethodCallerTest, ClassNotFound) {
  ClassMetadataReader::Method method;
  method.class_signature = { JType::Object, "LUnknownClass;" };
  method.name = "myMethod";
  method.signature = "()V";
  method.modifiers = 0;

  JniMethodCaller caller;
  EXPECT_FALSE(Bind(&caller, method));
}


TEST_F(JniMethodCallerTest, InstanceMethodNotFound) {
  ClassMetadataReader::Method method =
      InstanceMethod("Lcom/prod/MyClass1;", "unknownMethod", "()V");
  JniMethodCaller caller;
  EXPECT_FALSE(Bind(&caller, method));
}


TEST_F(JniMethodCallerTest, StaticMethodNotFound) {
  ClassMetadataReader::Method method =
      StaticMethod("Lcom/prod/MyClass1;", "unknownMethod", "()V");
  JniMethodCaller caller;
  EXPECT_FALSE(Bind(&caller, method));
}


TEST_F(JniMethodCallerTest, CallStaticVoidMethod) {
  auto method = StaticMethod(
      "Lcom/prod/MyClass1;",
      "myMethod",
      "(IIZLjava/lang/String;)V");
  jmethodID expected_method_id = RegisterMethod(method);

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  std::vector<JVariant> arguments(4);
  arguments[0] = JVariant::Int(1);
  arguments[1] = JVariant::Int(2);
  arguments[2] = JVariant::Boolean(true);
  arguments[3].attach_ref(
      JVariant::ReferenceKind::Local,
      fake_jni_.CreateNewJavaString("hunter-gatherer"));

  EXPECT_CALL(jni_, CallStaticVoidMethodA(_, _, _))
      .WillOnce(Invoke([this, expected_method_id] (
          jclass cls,
          jmethodID actual_method_id,
          const jvalue* arguments) {
        EXPECT_TRUE(jni_.IsSameObject(
            cls,
            fake_jni_.GetStockClass(FakeJni::StockClass::MyClass1)));

        EXPECT_EQ(expected_method_id, actual_method_id);

        EXPECT_EQ(1, arguments[0].i);
        EXPECT_EQ(2, arguments[1].i);
        EXPECT_EQ(JNI_TRUE, arguments[2].z);
        EXPECT_EQ(
            "hunter-gatherer",
            JniToNativeString(arguments[3].l));
      }));

  MethodCallResult result = caller.Call(false, nullptr, arguments);

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ(JType::Void, result.return_value().type());
}


TEST_F(JniMethodCallerTest, CallStaticBooleanMethod) {
  auto method = StaticMethod("Lcom/prod/MyClass1;", "myMethod", "()Z");
  RegisterMethod(method);

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallStaticBooleanMethodA(_, _, _))
      .WillOnce(Invoke([] (jclass cls, jmethodID method, const jvalue* args) {
        return true;
      }));

  MethodCallResult result = caller.Call(false, nullptr, {});

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ("<boolean>true", result.return_value().ToString(false));
}


TEST_F(JniMethodCallerTest, CallStaticByteMethod) {
  auto method = StaticMethod("Lcom/prod/MyClass1;", "myMethod", "()B");
  RegisterMethod(method);

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallStaticByteMethodA(_, _, _))
      .WillOnce(Invoke([] (jclass cls, jmethodID method, const jvalue* args) {
        return 11;
      }));

  MethodCallResult result = caller.Call(false, nullptr, {});

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ("<byte>11", result.return_value().ToString(false));
}


TEST_F(JniMethodCallerTest, CallStaticCharMethod) {
  auto method = StaticMethod("Lcom/prod/MyClass1;", "myMethod", "()C");
  RegisterMethod(method);

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallStaticCharMethodA(_, _, _))
      .WillOnce(Invoke([] (jclass cls, jmethodID method, const jvalue* args) {
        return 'A';
      }));

  MethodCallResult result = caller.Call(false, nullptr, {});

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ("<char>65", result.return_value().ToString(false));
}


TEST_F(JniMethodCallerTest, CallStaticShortMethod) {
  auto method = StaticMethod("Lcom/prod/MyClass1;", "myMethod", "()S");
  RegisterMethod(method);

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallStaticShortMethodA(_, _, _))
      .WillOnce(Invoke([] (jclass cls, jmethodID method, const jvalue* args) {
        return -23456;
      }));

  MethodCallResult result = caller.Call(false, nullptr, {});

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ("<short>-23456", result.return_value().ToString(false));
}


TEST_F(JniMethodCallerTest, CallStaticIntMethod) {
  auto method = StaticMethod("Lcom/prod/MyClass1;", "myMethod", "()I");
  RegisterMethod(method);

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallStaticIntMethodA(_, _, _))
      .WillOnce(Invoke([] (jclass cls, jmethodID method, const jvalue* args) {
        return 1234567;
      }));

  MethodCallResult result = caller.Call(false, nullptr, {});

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ("<int>1234567", result.return_value().ToString(false));
}


TEST_F(JniMethodCallerTest, CallStaticLongMethod) {
  auto method = StaticMethod("Lcom/prod/MyClass1;", "myMethod", "()J");
  RegisterMethod(method);

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallStaticLongMethodA(_, _, _))
      .WillOnce(Invoke([] (jclass cls, jmethodID method, const jvalue* args) {
        return 12345678987654321L;
      }));

  MethodCallResult result = caller.Call(false, nullptr, {});

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ("<long>12345678987654321", result.return_value().ToString(false));
}


TEST_F(JniMethodCallerTest, CallStaticFloatMethod) {
  auto method = StaticMethod("Lcom/prod/MyClass1;", "myMethod", "()F");
  RegisterMethod(method);

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallStaticFloatMethodA(_, _, _))
      .WillOnce(Invoke([] (jclass cls, jmethodID method, const jvalue* args) {
        return 3.14f;
      }));

  MethodCallResult result = caller.Call(false, nullptr, {});

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ("<float>3.14", result.return_value().ToString(false));
}


TEST_F(JniMethodCallerTest, CallStaticDoubleMethod) {
  auto method = StaticMethod("Lcom/prod/MyClass1;", "myMethod", "()D");
  RegisterMethod(method);

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallStaticDoubleMethodA(_, _, _))
      .WillOnce(Invoke([] (jclass cls, jmethodID method, const jvalue* args) {
        return 3.1415;
      }));

  MethodCallResult result = caller.Call(false, nullptr, {});

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ("<double>3.1415", result.return_value().ToString(false));
}


TEST_F(JniMethodCallerTest, CallStaticObjectMethod) {
  auto method = StaticMethod(
      "Lcom/prod/MyClass1;",
      "myMethod",
      "()Ljava/lang/String;");
  RegisterMethod(method);

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallStaticObjectMethodA(_, _, _))
      .WillOnce(Invoke([this] (
          jclass cls,
          jmethodID method,
          const jvalue* args) {
        return fake_jni_.CreateNewJavaString("australopithecus");
      }));

  MethodCallResult result = caller.Call(false, nullptr, {});

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ(JType::Object, result.return_value().type());
  EXPECT_EQ("australopithecus", JniToNativeString(result.return_ref()));
}


TEST_F(JniMethodCallerTest, CallInstanceVoidMethod) {
  auto method = InstanceMethod("Lcom/prod/MyClass1;",
                               "myMethod",
                               "(IIZLjava/lang/String;)V");
  jmethodID expected_method_id = RegisterMethod(method);

  JniLocalRef source(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  std::vector<JVariant> arguments(4);
  arguments[0] = JVariant::Int(1);
  arguments[1] = JVariant::Int(2);
  arguments[2] = JVariant::Boolean(true);
  arguments[3].attach_ref(
      JVariant::ReferenceKind::Local,
      fake_jni_.CreateNewJavaString("hunter-gatherer"));

  EXPECT_CALL(jni_, CallVoidMethodA(_, _, _))
      .WillOnce(Invoke([this, expected_method_id, &source] (
          jobject obj,
          jmethodID actual_method_id,
          const jvalue* arguments) {
        EXPECT_TRUE(jni_.IsSameObject(source.get(), obj));

        EXPECT_EQ(expected_method_id, actual_method_id);

        EXPECT_EQ(1, arguments[0].i);
        EXPECT_EQ(2, arguments[1].i);
        EXPECT_EQ(JNI_TRUE, arguments[2].z);
        EXPECT_EQ(
            "hunter-gatherer",
            JniToNativeString(arguments[3].l));
      }));

  MethodCallResult result = caller.Call(false, source.get(), arguments);

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ(JType::Void, result.return_value().type());
}


TEST_F(JniMethodCallerTest, CallInstanceBooleanMethod) {
  auto method = InstanceMethod("Lcom/prod/MyClass1;", "myMethod", "()Z");
  RegisterMethod(method);

  JniLocalRef source(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallBooleanMethodA(_, _, _))
      .WillOnce(Invoke([this, &source] (
          jobject obj,
          jmethodID method_id,
          const jvalue* arguments) {
        EXPECT_TRUE(jni_.IsSameObject(source.get(), obj));
        return true;
      }));

  MethodCallResult result = caller.Call(false, source.get(), {});

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ("<boolean>true", result.return_value().ToString(false));
}


TEST_F(JniMethodCallerTest, CallInstanceByteMethod) {
  auto method = InstanceMethod("Lcom/prod/MyClass1;", "myMethod", "()B");
  RegisterMethod(method);

  JniLocalRef source(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallByteMethodA(_, _, _))
      .WillOnce(Invoke([this, &source] (
          jobject obj,
          jmethodID method_id,
          const jvalue* arguments) {
        EXPECT_TRUE(jni_.IsSameObject(source.get(), obj));
        return -45;
      }));

  MethodCallResult result = caller.Call(false, source.get(), {});

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ("<byte>-45", result.return_value().ToString(false));
}


TEST_F(JniMethodCallerTest, CallInstanceCharMethod) {
  auto method = InstanceMethod("Lcom/prod/MyClass1;", "myMethod", "()C");
  RegisterMethod(method);

  JniLocalRef source(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallCharMethodA(_, _, _))
      .WillOnce(Invoke([this, &source] (
          jobject obj,
          jmethodID method_id,
          const jvalue* arguments) {
        EXPECT_TRUE(jni_.IsSameObject(source.get(), obj));
        return 'B';
      }));

  MethodCallResult result = caller.Call(false, source.get(), {});

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ("<char>66", result.return_value().ToString(false));
}


TEST_F(JniMethodCallerTest, CallInstanceShortMethod) {
  auto method = InstanceMethod("Lcom/prod/MyClass1;", "myMethod", "()S");
  RegisterMethod(method);

  JniLocalRef source(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallShortMethodA(_, _, _))
      .WillOnce(Invoke([this, &source] (
          jobject obj,
          jmethodID method_id,
          const jvalue* arguments) {
        EXPECT_TRUE(jni_.IsSameObject(source.get(), obj));
        return 456;
      }));

  MethodCallResult result = caller.Call(false, source.get(), {});

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ("<short>456", result.return_value().ToString(false));
}


TEST_F(JniMethodCallerTest, CallInstanceIntMethod) {
  auto method = InstanceMethod("Lcom/prod/MyClass1;", "myMethod", "()I");
  RegisterMethod(method);

  JniLocalRef source(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallIntMethodA(_, _, _))
      .WillOnce(Invoke([this, &source] (
          jobject obj,
          jmethodID method_id,
          const jvalue* arguments) {
        EXPECT_TRUE(jni_.IsSameObject(source.get(), obj));
        return 358447356;
      }));

  MethodCallResult result = caller.Call(false, source.get(), {});

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ("<int>358447356", result.return_value().ToString(false));
}


TEST_F(JniMethodCallerTest, CallInstanceLongMethod) {
  auto method = InstanceMethod("Lcom/prod/MyClass1;", "myMethod", "()J");
  RegisterMethod(method);

  JniLocalRef source(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallLongMethodA(_, _, _))
      .WillOnce(Invoke([this, &source] (
          jobject obj,
          jmethodID method_id,
          const jvalue* arguments) {
        EXPECT_TRUE(jni_.IsSameObject(source.get(), obj));
        return 45784329647297L;
      }));

  MethodCallResult result = caller.Call(false, source.get(), {});

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ("<long>45784329647297", result.return_value().ToString(false));
}


TEST_F(JniMethodCallerTest, CallInstanceFloatMethod) {
  auto method = InstanceMethod("Lcom/prod/MyClass1;", "myMethod", "()F");
  RegisterMethod(method);

  JniLocalRef source(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallFloatMethodA(_, _, _))
      .WillOnce(Invoke([this, &source] (
          jobject obj,
          jmethodID method_id,
          const jvalue* arguments) {
        EXPECT_TRUE(jni_.IsSameObject(source.get(), obj));
        return 3.45f;
      }));

  MethodCallResult result = caller.Call(false, source.get(), {});

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ("<float>3.45", result.return_value().ToString(false));
}


TEST_F(JniMethodCallerTest, CallInstanceDoubleMethod) {
  auto method = InstanceMethod("Lcom/prod/MyClass1;", "myMethod", "()D");
  RegisterMethod(method);

  JniLocalRef source(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallDoubleMethodA(_, _, _))
      .WillOnce(Invoke([this, &source] (
          jobject obj,
          jmethodID method_id,
          const jvalue* arguments) {
        EXPECT_TRUE(jni_.IsSameObject(source.get(), obj));
        return 5643.11;
      }));

  MethodCallResult result = caller.Call(false, source.get(), {});

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ("<double>5643.11", result.return_value().ToString(false));
}


TEST_F(JniMethodCallerTest, CallInstanceObjectMethod) {
  auto method = InstanceMethod(
      "Lcom/prod/MyClass1;",
      "myMethod",
      "()Ljava/lang/String;");
  RegisterMethod(method);

  JniLocalRef source(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallObjectMethodA(_, _, _))
      .WillOnce(Invoke([this, &source] (
          jobject obj,
          jmethodID method_id,
          const jvalue* arguments) {
        EXPECT_TRUE(jni_.IsSameObject(source.get(), obj));
        return fake_jni_.CreateNewJavaString("neanderthal");
      }));

  MethodCallResult result = caller.Call(false, source.get(), {});

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ(JType::Object, result.return_value().type());
  EXPECT_EQ("neanderthal", JniToNativeString(result.return_ref()));
}


TEST_F(JniMethodCallerTest, CallNonVirtualVoidMethod) {
  auto method = InstanceMethod("Lcom/prod/MyClass1;",
                               "myMethod",
                               "(IIZLjava/lang/String;)V");
  jmethodID expected_method_id = RegisterMethod(method);

  JniLocalRef source(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  std::vector<JVariant> arguments(4);
  arguments[0] = JVariant::Int(1);
  arguments[1] = JVariant::Int(2);
  arguments[2] = JVariant::Boolean(true);
  arguments[3].attach_ref(
      JVariant::ReferenceKind::Local,
      fake_jni_.CreateNewJavaString("hunter-gatherer"));

  EXPECT_CALL(jni_, CallNonvirtualVoidMethodA(_, _, _, _))
      .WillOnce(Invoke([this, expected_method_id, &source] (
          jobject obj,
          jclass cls,
          jmethodID actual_method_id,
          const jvalue* arguments) {
        EXPECT_TRUE(jni_.IsSameObject(source.get(), obj));
        EXPECT_TRUE(jni_.IsSameObject(
            cls,
            fake_jni_.GetStockClass(FakeJni::StockClass::MyClass1)));

        EXPECT_EQ(expected_method_id, actual_method_id);

        EXPECT_EQ(1, arguments[0].i);
        EXPECT_EQ(2, arguments[1].i);
        EXPECT_EQ(JNI_TRUE, arguments[2].z);
        EXPECT_EQ(
            "hunter-gatherer",
            JniToNativeString(arguments[3].l));
      }));

  MethodCallResult result = caller.Call(true, source.get(), arguments);

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ(JType::Void, result.return_value().type());
}


TEST_F(JniMethodCallerTest, CallNonVirtualBooleanMethod) {
  auto method = InstanceMethod("Lcom/prod/MyClass1;", "myMethod", "()Z");
  RegisterMethod(method);

  JniLocalRef source(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallNonvirtualBooleanMethodA(_, _, _, _))
      .WillOnce(Invoke([this, &source] (
          jobject obj,
          jclass cls,
          jmethodID method_id,
          const jvalue* arguments) {
        EXPECT_TRUE(jni_.IsSameObject(source.get(), obj));
        EXPECT_TRUE(jni_.IsSameObject(
            cls,
            fake_jni_.GetStockClass(FakeJni::StockClass::MyClass1)));
        return true;
      }));

  MethodCallResult result = caller.Call(true, source.get(), {});

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ("<boolean>true", result.return_value().ToString(false));
}


TEST_F(JniMethodCallerTest, CallNonVirtualByteMethod) {
  auto method = InstanceMethod("Lcom/prod/MyClass1;", "myMethod", "()B");
  RegisterMethod(method);

  JniLocalRef source(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallNonvirtualByteMethodA(_, _, _, _))
      .WillOnce(Invoke([this, &source] (
          jobject obj,
          jclass cls,
          jmethodID method_id,
          const jvalue* arguments) {
        EXPECT_TRUE(jni_.IsSameObject(source.get(), obj));
        EXPECT_TRUE(jni_.IsSameObject(
            cls,
            fake_jni_.GetStockClass(FakeJni::StockClass::MyClass1)));
        return -45;
      }));

  MethodCallResult result = caller.Call(true, source.get(), {});

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ("<byte>-45", result.return_value().ToString(false));
}


TEST_F(JniMethodCallerTest, CallNonVirtualCharMethod) {
  auto method = InstanceMethod("Lcom/prod/MyClass1;", "myMethod", "()C");
  RegisterMethod(method);

  JniLocalRef source(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallNonvirtualCharMethodA(_, _, _, _))
      .WillOnce(Invoke([this, &source] (
          jobject obj,
          jclass cls,
          jmethodID method_id,
          const jvalue* arguments) {
        EXPECT_TRUE(jni_.IsSameObject(source.get(), obj));
        EXPECT_TRUE(jni_.IsSameObject(
            cls,
            fake_jni_.GetStockClass(FakeJni::StockClass::MyClass1)));
        return 'B';
      }));

  MethodCallResult result = caller.Call(true, source.get(), {});

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ("<char>66", result.return_value().ToString(false));
}


TEST_F(JniMethodCallerTest, CallNonVirtualShortMethod) {
  auto method = InstanceMethod("Lcom/prod/MyClass1;", "myMethod", "()S");
  RegisterMethod(method);

  JniLocalRef source(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallNonvirtualShortMethodA(_, _, _, _))
      .WillOnce(Invoke([this, &source] (
          jobject obj,
          jclass cls,
          jmethodID method_id,
          const jvalue* arguments) {
        EXPECT_TRUE(jni_.IsSameObject(source.get(), obj));
        EXPECT_TRUE(jni_.IsSameObject(
            cls,
            fake_jni_.GetStockClass(FakeJni::StockClass::MyClass1)));
        return 456;
      }));

  MethodCallResult result = caller.Call(true, source.get(), {});

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ("<short>456", result.return_value().ToString(false));
}


TEST_F(JniMethodCallerTest, CallNonVirtualIntMethod) {
  auto method = InstanceMethod("Lcom/prod/MyClass1;", "myMethod", "()I");
  RegisterMethod(method);

  JniLocalRef source(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallNonvirtualIntMethodA(_, _, _, _))
      .WillOnce(Invoke([this, &source] (
          jobject obj,
          jclass cls,
          jmethodID method_id,
          const jvalue* arguments) {
        EXPECT_TRUE(jni_.IsSameObject(source.get(), obj));
        EXPECT_TRUE(jni_.IsSameObject(
            cls,
            fake_jni_.GetStockClass(FakeJni::StockClass::MyClass1)));
        return 358447356;
      }));

  MethodCallResult result = caller.Call(true, source.get(), {});

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ("<int>358447356", result.return_value().ToString(false));
}


TEST_F(JniMethodCallerTest, CallNonVirtualLongMethod) {
  auto method = InstanceMethod("Lcom/prod/MyClass1;", "myMethod", "()J");
  RegisterMethod(method);

  JniLocalRef source(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallNonvirtualLongMethodA(_, _, _, _))
      .WillOnce(Invoke([this, &source] (
          jobject obj,
          jclass cls,
          jmethodID method_id,
          const jvalue* arguments) {
        EXPECT_TRUE(jni_.IsSameObject(source.get(), obj));
        EXPECT_TRUE(jni_.IsSameObject(
            cls,
            fake_jni_.GetStockClass(FakeJni::StockClass::MyClass1)));
        return 45784329647297L;
      }));

  MethodCallResult result = caller.Call(true, source.get(), {});

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ("<long>45784329647297", result.return_value().ToString(false));
}


TEST_F(JniMethodCallerTest, CallNonVirtualFloatMethod) {
  auto method = InstanceMethod("Lcom/prod/MyClass1;", "myMethod", "()F");
  RegisterMethod(method);

  JniLocalRef source(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallNonvirtualFloatMethodA(_, _, _, _))
      .WillOnce(Invoke([this, &source] (
          jobject obj,
          jclass cls,
          jmethodID method_id,
          const jvalue* arguments) {
        EXPECT_TRUE(jni_.IsSameObject(source.get(), obj));
        EXPECT_TRUE(jni_.IsSameObject(
            cls,
            fake_jni_.GetStockClass(FakeJni::StockClass::MyClass1)));
        return 3.45f;
      }));

  MethodCallResult result = caller.Call(true, source.get(), {});

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ("<float>3.45", result.return_value().ToString(false));
}


TEST_F(JniMethodCallerTest, CallNonVirtualDoubleMethod) {
  auto method = InstanceMethod("Lcom/prod/MyClass1;", "myMethod", "()D");
  RegisterMethod(method);

  JniLocalRef source(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallNonvirtualDoubleMethodA(_, _, _, _))
      .WillOnce(Invoke([this, &source] (
          jobject obj,
          jclass cls,
          jmethodID method_id,
          const jvalue* arguments) {
        EXPECT_TRUE(jni_.IsSameObject(source.get(), obj));
        EXPECT_TRUE(jni_.IsSameObject(
            cls,
            fake_jni_.GetStockClass(FakeJni::StockClass::MyClass1)));
        return 5643.11;
      }));

  MethodCallResult result = caller.Call(true, source.get(), {});

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ("<double>5643.11", result.return_value().ToString(false));
}


TEST_F(JniMethodCallerTest, CallNonVirtualObjectMethod) {
  auto method = InstanceMethod(
      "Lcom/prod/MyClass1;",
      "myMethod",
      "()Ljava/lang/String;");
  RegisterMethod(method);

  JniLocalRef source(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallNonvirtualObjectMethodA(_, _, _, _))
      .WillOnce(Invoke([this, &source] (
          jobject obj,
          jclass cls,
          jmethodID method_id,
          const jvalue* arguments) {
        EXPECT_TRUE(jni_.IsSameObject(source.get(), obj));
        EXPECT_TRUE(jni_.IsSameObject(
            cls,
            fake_jni_.GetStockClass(FakeJni::StockClass::MyClass1)));
        return fake_jni_.CreateNewJavaString("neanderthal");
      }));

  MethodCallResult result = caller.Call(true, source.get(), {});

  EXPECT_EQ(MethodCallResult::Type::Success, result.result_type());
  EXPECT_EQ(JType::Object, result.return_value().type());
  EXPECT_EQ("neanderthal", JniToNativeString(result.return_ref()));
}


TEST_F(JniMethodCallerTest, Exception) {
  auto method = StaticMethod("Lcom/prod/MyClass1;", "myMethod", "()V");
  RegisterMethod(method);

  JniMethodCaller caller;
  EXPECT_TRUE(Bind(&caller, method));

  EXPECT_CALL(jni_, CallStaticVoidMethodA(_, _, _))
      .WillOnce(Return());

  JniLocalRef exception_object(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass3));

  EXPECT_CALL(jni_, ExceptionCheck())
      .WillOnce(Return(true));

  EXPECT_CALL(jni_, ExceptionOccurred())
      .WillOnce(Return(static_cast<jthrowable>(
          jni_.NewLocalRef(exception_object.get()))));

  MethodCallResult result = caller.Call(false, nullptr, {});

  EXPECT_EQ(MethodCallResult::Type::JavaException, result.result_type());
  EXPECT_TRUE(jni_.IsSameObject(
      exception_object.get(),
      result.exception()));
}

}  // namespace cdbg
}  // namespace devtools
