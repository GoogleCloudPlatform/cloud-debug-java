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

#include "src/agent/method_locals.h"

#include "gtest/gtest.h"
#include "src/agent/common.h"
#include "src/agent/local_variable_reader.h"
#include "src/agent/readers_factory.h"
#include "src/agent/structured_data_visibility_policy.h"
#include "tests/agent/mock_jni_env.h"
#include "tests/agent/mock_jvmti_env.h"

using testing::_;
using testing::AnyNumber;
using testing::DoAll;
using testing::Eq;
using testing::IsNull;
using testing::NotNull;
using testing::Test;
using testing::Return;
using testing::ReturnNew;
using testing::WithArgs;
using testing::Invoke;
using testing::InvokeArgument;
using testing::SetArgPointee;

namespace devtools {
namespace cdbg {

static const jthread thread = reinterpret_cast<jthread>(0x67125374);
static const jmethodID method = reinterpret_cast<jmethodID>(0x8726534);
static constexpr int frame_depth = 4;
static constexpr char kMyClassSignature[] = "Lcom/prod/MyClass1;";
static const jclass kMyClass = reinterpret_cast<jclass>(0x1111);


class MethodLocalsTest : public ::testing::Test {
 protected:
  MethodLocalsTest() : global_jvm_(&jvmti_, &jni_) {
  }

  void SetUp() override {
    // Ignore deallocations in most cases.
    EXPECT_CALL(jvmti_, Deallocate(NotNull()))
        .WillRepeatedly(Return(JVMTI_ERROR_NONE));

    // Add wiring between kMyClass and method.
    EXPECT_CALL(
        jvmti_,
        GetMethodDeclaringClass(method, NotNull()))
        .WillRepeatedly(DoAll(
            SetArgPointee<1>(kMyClass),
            Return(JVMTI_ERROR_NONE)));

    // Called when fetching class visibility.
    EXPECT_CALL(jvmti_, GetClassSignature(_, NotNull(), nullptr))
        .WillRepeatedly(DoAll(
            SetArgPointee<1>(const_cast<char*>("LMyClass;")),
            Return(JVMTI_ERROR_NONE)));

    EXPECT_CALL(jni_, GetObjectRefType(kMyClass))
        .WillRepeatedly(Return(JNILocalRefType));

    EXPECT_CALL(jni_, DeleteLocalRef(kMyClass))
        .WillRepeatedly(Return());
  }

 protected:
  MockJvmtiEnv jvmti_;
  MockJNIEnv jni_;
  GlobalJvmEnv global_jvm_;
};


// Test factory of LocalVariableReader instances.
TEST_F(MethodLocalsTest, LocalReadersFactory) {
  jvmtiLocalVariableEntry table[] = {
    {
      100,  // start_location
      0,  // length
      const_cast<char*>("local_boolean"),  // name
      const_cast<char*>("Z"),  // signature
      nullptr,  // generic_signature
      100  // slot
    },
    {
      100,  // start_location
      0,  // length
      const_cast<char*>("local_long"),  // name
      const_cast<char*>("J"),  // signature
      nullptr,  // generic_signature
      105  // slot
    }
  };

  jvmtiLocalVariableEntry* ptable = table;

  EXPECT_CALL(jvmti_, GetMethodModifiers(method, NotNull()))
      .WillOnce(DoAll(
          SetArgPointee<1>(JVM_ACC_STATIC),
          Return(JVMTI_ERROR_NONE)));

  EXPECT_CALL(
      jvmti_,
      GetLocalVariableTable(method, NotNull(), NotNull()))
      .WillOnce(DoAll(
          SetArgPointee<1>(arraysize(table)),
          SetArgPointee<2>(ptable),
          Return(JVMTI_ERROR_NONE)));

  EXPECT_CALL(jvmti_, GetArgumentsSize(method, NotNull()))
      .WillOnce(DoAll(
          SetArgPointee<1>(0),
          Return(JVMTI_ERROR_NONE)));

  StructuredDataVisibilityPolicy data_visibility_policy;
  MethodLocals method_locals(&data_visibility_policy);
  auto entry = method_locals.GetLocalVariables(method);

  EvaluationContext evaluation_context;
  JVariant result;
  FormatMessageModel error;

  ASSERT_EQ(2, entry->locals.size());
  EXPECT_EQ("local_boolean", entry->locals[0]->GetName());
  EXPECT_EQ(JType::Boolean, entry->locals[0]->GetStaticType().type);
  EXPECT_TRUE(entry->locals[0]->ReadValue(evaluation_context, &result, &error));
  EXPECT_EQ("local_long", entry->locals[1]->GetName());
  EXPECT_EQ(JType::Long, entry->locals[1]->GetStaticType().type);
  EXPECT_TRUE(entry->locals[1]->ReadValue(evaluation_context, &result, &error));
}


TEST_F(MethodLocalsTest, LocalInstanceReaderFactory) {
  const int table_size = 0;
  jvmtiLocalVariableEntry* ptable = nullptr;

  EXPECT_CALL(jvmti_, GetMethodModifiers(method, NotNull()))
      .WillOnce(DoAll(
          SetArgPointee<1>(0),
          Return(JVMTI_ERROR_NONE)));

  EXPECT_CALL(
      jvmti_,
      GetClassSignature(kMyClass, NotNull(), NotNull()))
      .WillOnce(DoAll(
          SetArgPointee<1>(const_cast<char*>(kMyClassSignature)),
          SetArgPointee<2>(nullptr),
          Return(JVMTI_ERROR_NONE)));

  EXPECT_CALL(
      jvmti_,
      GetLocalVariableTable(method, NotNull(), NotNull()))
      .WillOnce(DoAll(
          SetArgPointee<1>(table_size),
          SetArgPointee<2>(ptable),
          Return(JVMTI_ERROR_NONE)));

  EXPECT_CALL(jvmti_, GetArgumentsSize(method, NotNull()))
      .WillRepeatedly(DoAll(
          SetArgPointee<1>(0),
          Return(JVMTI_ERROR_NONE)));

  StructuredDataVisibilityPolicy data_visibility_policy;
  MethodLocals method_locals(&data_visibility_policy);
  auto entry = method_locals.GetLocalVariables(method);

  ASSERT_TRUE(entry->local_instance != nullptr);
  EXPECT_EQ("this", entry->local_instance->GetName());
  EXPECT_EQ(JType::Object, entry->local_instance->GetStaticType().type);
  EXPECT_EQ(std::string(kMyClassSignature),
            entry->local_instance->GetStaticType().object_signature);
  EXPECT_TRUE(entry->local_instance->IsDefinedAtLocation(0));
  EXPECT_TRUE(entry->local_instance->IsDefinedAtLocation(0xFFFFFFFF));
  EXPECT_TRUE(entry->local_instance->IsDefinedAtLocation(-0xFFFFFFFF));
  EXPECT_TRUE(entry->local_instance->IsDefinedAtLocation(0x7FFFFFFFFFFFFFF0L));
  EXPECT_TRUE(entry->local_instance->IsDefinedAtLocation(-0x7FFFFFFFFFFFFFF0L));
}


// Verify that local variable table is cached properly.
TEST_F(MethodLocalsTest, Cache) {
  const jvmtiError ret_vals[] = {
    JVMTI_ERROR_NONE,
    JVMTI_ERROR_ABSENT_INFORMATION,
    JVMTI_ERROR_NATIVE_METHOD
  };

  const int table_size = 0;
  jvmtiLocalVariableEntry* ptable = nullptr;

  EXPECT_CALL(jvmti_, GetArgumentsSize(method, NotNull()))
      .Times(AnyNumber())
      .WillRepeatedly(DoAll(
          SetArgPointee<1>(0),
          Return(JVMTI_ERROR_NONE)));

  for (jvmtiError ret_val : ret_vals) {
    StructuredDataVisibilityPolicy data_visibility_policy;
    MethodLocals method_locals(&data_visibility_policy);

    EXPECT_CALL(jvmti_, GetMethodModifiers(method, NotNull()))
        .WillOnce(DoAll(
            SetArgPointee<1>(JVM_ACC_STATIC),
            Return(JVMTI_ERROR_NONE)));

    EXPECT_CALL(
        jvmti_,
        GetLocalVariableTable(method, NotNull(), NotNull()))
        .WillOnce(DoAll(
            SetArgPointee<1>(table_size),
            SetArgPointee<2>(ptable),
            Return(ret_val)));

    method_locals.GetLocalVariables(method);
    method_locals.GetLocalVariables(method);
  }
}


// Verify that when method is unloaded, the cache is invalidated and memory
// properly released.
TEST_F(MethodLocalsTest, Unload) {
  StructuredDataVisibilityPolicy data_visibility_policy;
  MethodLocals method_locals(&data_visibility_policy);

  const int table_size = 0;
  jvmtiLocalVariableEntry* ptable = nullptr;

  EXPECT_CALL(jvmti_, GetMethodModifiers(method, NotNull()))
      .Times(2)
      .WillRepeatedly(DoAll(
          SetArgPointee<1>(JVM_ACC_STATIC),
          Return(JVMTI_ERROR_NONE)));

  EXPECT_CALL(
      jvmti_,
      GetLocalVariableTable(method, NotNull(), NotNull()))
      .Times(2)
      .WillRepeatedly(DoAll(
          SetArgPointee<1>(table_size),
          SetArgPointee<2>(ptable),
          Return(JVMTI_ERROR_NONE)));

  EXPECT_CALL(jvmti_, GetArgumentsSize(method, NotNull()))
      .WillRepeatedly(DoAll(
          SetArgPointee<1>(0),
          Return(JVMTI_ERROR_NONE)));

  method_locals.GetLocalVariables(method);

  {
    GlobalNoJni no_jni;
    method_locals.JvmtiOnCompiledMethodUnload(method);
  }

  method_locals.GetLocalVariables(method);
}


// Verify that error returned by GetLocalVariableTable results in empty set and
// that the failure is not cached.
TEST_F(MethodLocalsTest, GetLocalVariableTable_Failure) {
  StructuredDataVisibilityPolicy data_visibility_policy;
  MethodLocals method_locals(&data_visibility_policy);

  EXPECT_CALL(jvmti_, GetMethodModifiers(method, NotNull()))
      .Times(2)
      .WillRepeatedly(DoAll(
          SetArgPointee<1>(JVM_ACC_STATIC),
          Return(JVMTI_ERROR_NONE)));

  EXPECT_CALL(
      jvmti_,
      GetLocalVariableTable(method, NotNull(), NotNull()))
      .Times(2)
      .WillRepeatedly(Return(JVMTI_ERROR_INVALID_METHODID));

  EXPECT_CALL(jvmti_, GetArgumentsSize(method, NotNull()))
      .WillRepeatedly(DoAll(
          SetArgPointee<1>(0),
          Return(JVMTI_ERROR_NONE)));

  method_locals.GetLocalVariables(method);
  method_locals.GetLocalVariables(method);
}


TEST_F(MethodLocalsTest, ArgumentsDetection) {
  jvmtiLocalVariableEntry table[] = {
    {
      0,  // start_location
      0,  // length
      const_cast<char*>("this"),  // name
      const_cast<char*>(kMyClassSignature),  // signature
      nullptr,  // generic_signature
      0  // slot
    },
    {
      0,  // start_location
      0,  // length
      const_cast<char*>("arg1"),  // name
      const_cast<char*>("Z"),  // signature
      nullptr,  // generic_signature
      1  // slot
    },
    {
      0,  // start_location
      0,  // length
      const_cast<char*>("arg2"),  // name
      const_cast<char*>("Z"),  // signature
      nullptr,  // generic_signature
      2  // slot
    },
    {
      0,  // start_location
      0,  // length
      const_cast<char*>("local1"),  // name
      const_cast<char*>("Z"),  // signature
      nullptr,  // generic_signature
      3  // slot
    },
    {
      0,  // start_location
      0,  // length
      const_cast<char*>("local2"),  // name
      const_cast<char*>("Z"),  // signature
      nullptr,  // generic_signature
      4  // slot
    }
  };

  jvmtiLocalVariableEntry* ptable = table;

  // Class has no method modifiers.
  EXPECT_CALL(jvmti_, GetMethodModifiers(method, NotNull()))
      .WillOnce(DoAll(
          SetArgPointee<1>(0),
          Return(JVMTI_ERROR_NONE)));

  // Class has a signature but no generic signature.
  EXPECT_CALL(
      jvmti_,
      GetClassSignature(kMyClass, NotNull(), NotNull()))
      .WillOnce(DoAll(
          SetArgPointee<1>(const_cast<char*>(kMyClassSignature)),
          SetArgPointee<2>(nullptr),
          Return(JVMTI_ERROR_NONE)));

  // Local variable table is as stored above.
  EXPECT_CALL(
      jvmti_,
      GetLocalVariableTable(method, NotNull(), NotNull()))
      .WillOnce(DoAll(
          SetArgPointee<1>(arraysize(table)),
          SetArgPointee<2>(ptable),
          Return(JVMTI_ERROR_NONE)));

  // The first 3 local variables are method arguments.
  EXPECT_CALL(jvmti_, GetArgumentsSize(method, NotNull()))
      .WillOnce(DoAll(
          SetArgPointee<1>(3),
          Return(JVMTI_ERROR_NONE)));

  StructuredDataVisibilityPolicy data_visibility_policy;
  MethodLocals method_locals(&data_visibility_policy);
  auto entry = method_locals.GetLocalVariables(method);

  ASSERT_EQ(5, entry->locals.size());

  EXPECT_EQ("this", entry->locals[0]->GetName());
  EXPECT_TRUE(entry->locals[0]->IsArgument());

  EXPECT_EQ("arg1", entry->locals[1]->GetName());
  EXPECT_TRUE(entry->locals[1]->IsArgument());

  EXPECT_EQ("arg2", entry->locals[2]->GetName());
  EXPECT_TRUE(entry->locals[2]->IsArgument());

  EXPECT_EQ("local1", entry->locals[3]->GetName());
  EXPECT_FALSE(entry->locals[3]->IsArgument());

  EXPECT_EQ("local2", entry->locals[4]->GetName());
  EXPECT_FALSE(entry->locals[4]->IsArgument());

  EXPECT_TRUE(entry->local_instance->IsArgument());
}

TEST_F(MethodLocalsTest, MemoryAllocation) {
  // NOTE:  The string values in this table are not representative of what one
  // would see in a real run environment; they have been modified so that values
  // are distinct for the purposes of making memory allocation/deallocation
  // tests more effective.
  jvmtiLocalVariableEntry table[] = {
      {
          0,                                     // start_location
          0,                                     // length
          const_cast<char*>("this"),             // name
          const_cast<char*>("class_signature"),  // signature
          nullptr,                               // generic_signature
          0                                      // slot
      },
      {
          0,                          // start_location
          0,                          // length
          const_cast<char*>("arg1"),  // name
          const_cast<char*>("Z1"),    // signature
          nullptr,                    // generic_signature
          1                           // slot
      },
      {
          0,                          // start_location
          0,                          // length
          const_cast<char*>("arg2"),  // name
          const_cast<char*>("Z2"),    // signature
          const_cast<char*>("G"),     // generic_signature
          2                           // slot
      },
      {
          0,                            // start_location
          0,                            // length
          const_cast<char*>("local1"),  // name
          const_cast<char*>("Z3"),      // signature
          nullptr,                      // generic_signature
          3                             // slot
      },
      {
          0,                            // start_location
          0,                            // length
          const_cast<char*>("local2"),  // name
          const_cast<char*>("Z4"),      // signature
          nullptr,                      // generic_signature
          4                             // slot
      }};

  jvmtiLocalVariableEntry* ptable = table;

  // Override the default behaviour: Fail the test if any unexpected
  // deallocations occur.
  EXPECT_CALL(jvmti_, Deallocate(_)).Times(0);

  // Class has no modifiers.
  EXPECT_CALL(jvmti_, GetMethodModifiers(method, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>(0), Return(JVMTI_ERROR_NONE)));

  // Class has a signature but no generic signature.
  EXPECT_CALL(jvmti_, GetClassSignature(kMyClass, NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>(const_cast<char*>(kMyClassSignature)),
                      SetArgPointee<2>(nullptr), Return(JVMTI_ERROR_NONE)));

  // Class signature must be deallocated.
  EXPECT_CALL(jvmti_, Deallocate(reinterpret_cast<unsigned char*>(
                          const_cast<char*>(kMyClassSignature))))
      .WillOnce(Return(JVMTI_ERROR_NONE));

  // Local variable table is as stored above.
  EXPECT_CALL(jvmti_, GetLocalVariableTable(method, NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>(arraysize(table)),
                      SetArgPointee<2>(ptable), Return(JVMTI_ERROR_NONE)));

  // First 3 locals are arguments (irrelevant).
  EXPECT_CALL(jvmti_, GetArgumentsSize(method, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>(3), Return(JVMTI_ERROR_NONE)));

  // The table needs to be deallocated.
  EXPECT_CALL(jvmti_, Deallocate(reinterpret_cast<unsigned char*>(ptable)))
      .WillOnce(Return(JVMTI_ERROR_NONE));

  // Name, signature and generic_signature on each table entry must be
  // deallocated separately.
  for (const jvmtiLocalVariableEntry& entry : table) {
    EXPECT_CALL(jvmti_, Deallocate(reinterpret_cast<unsigned char*>(
                            const_cast<char*>(entry.name))))
        .WillOnce(Return(JVMTI_ERROR_NONE));
    EXPECT_CALL(jvmti_, Deallocate(reinterpret_cast<unsigned char*>(
                            const_cast<char*>(entry.signature))))
        .WillOnce(Return(JVMTI_ERROR_NONE));
    // Skip generic_signature if it's null.
    if (entry.generic_signature != nullptr) {
      EXPECT_CALL(jvmti_, Deallocate(reinterpret_cast<unsigned char*>(
                              const_cast<char*>(entry.generic_signature))))
          .WillOnce(Return(JVMTI_ERROR_NONE));
    }
  }

  // Deallocate the signature that was fetched when determining visibility.
  EXPECT_CALL(jvmti_, Deallocate(reinterpret_cast<unsigned char*>(
                          const_cast<char*>("LMyClass;"))))
      .WillOnce(Return(JVMTI_ERROR_NONE));

  StructuredDataVisibilityPolicy data_visibility_policy;
  MethodLocals method_locals(&data_visibility_policy);
  auto entry = method_locals.GetLocalVariables(method);

  // No assertions necessary: EXPECT_CALLs on Deallocate are the real test.
}

}  // namespace cdbg
}  // namespace devtools
