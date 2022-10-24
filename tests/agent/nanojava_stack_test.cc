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

#include "src/agent/nanojava_stack.h"

#include <cstdint>

#include "gmock/gmock.h"

#include "tests/agent/fake_jni.h"
#include "tests/agent/mock_jvmti_env.h"
#include "tests/agent/mock_nanojava_internal_error_provider.h"

using testing::_;
using testing::Exactly;
using testing::Return;
using testing::StrictMock;

namespace devtools {
namespace cdbg {
namespace nanojava {

class NanoJavaStackTest : public testing::Test {
 public:
  NanoJavaStackTest() : global_jvm_(fake_jni_.jvmti(), fake_jni_.jni()) {}

  void SetUp() override {
    EXPECT_CALL(internal_error_provider_, method_name())
        .WillRepeatedly(Return(std::string()));

    EXPECT_CALL(internal_error_provider_, FormatCallStack())
        .WillRepeatedly(Return(std::string()));
  }

  MOCK_METHOD(void, RaiseNullPointerException, ());

 protected:
  FakeJni fake_jni_;
  GlobalJvmEnv global_jvm_;
  StrictMock<MockNanoJavaInternalErrorProvider> internal_error_provider_;
  std::function<void()> fn_raise_null_pointer_exception_
      { [this] () { RaiseNullPointerException(); } };
};


TEST_F(NanoJavaStackTest, EmptyStack) {
  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      0);
}


TEST_F(NanoJavaStackTest, ObjectString) {
  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      1);
  stack.PushStackObject(JniToJavaString("hello").get());
  EXPECT_EQ("hello", JniToNativeString(stack.PopStackObject().get()));
}


TEST_F(NanoJavaStackTest, PopStackObjectNonNullPositive) {
  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      1);
  stack.PushStackObject(JniToJavaString("hello").get());
  EXPECT_EQ("hello", JniToNativeString(stack.PopStackObjectNonNull().get()));
}


TEST_F(NanoJavaStackTest, PopStackObjectNonNullNegative) {
  EXPECT_CALL(*this, RaiseNullPointerException())
      .Times(Exactly(1));

  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      1);
  stack.PushStackObject(nullptr);
  stack.PopStackObjectNonNull();
}


TEST_F(NanoJavaStackTest, PopStackObjectInstanceOfPositive) {
  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      1);

  stack.PushStackObject(JniToJavaString("hello").get());
  EXPECT_EQ(
      "hello",
      JniToNativeString(stack.PopStackObjectInstanceOf(
          fake_jni_.GetStockClass(FakeJni::StockClass::String)).get()));
}


TEST_F(NanoJavaStackTest, PopStackObjectInstanceOfNegativeNull) {
  EXPECT_CALL(*this, RaiseNullPointerException())
      .Times(Exactly(1));

  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      1);
  stack.PushStackObject(nullptr);
  stack.PopStackObjectInstanceOf(
      fake_jni_.GetStockClass(FakeJni::StockClass::String));
}


TEST_F(NanoJavaStackTest, PopStackObjectInstanceOfNegativeType) {
  EXPECT_CALL(internal_error_provider_, SetResult(_))
      .Times(Exactly(1));

  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      1);

  JniLocalRef obj(fake_jni_.CreateNewObject(FakeJni::StockClass::Object));
  stack.PushStackObject(obj.get());

  stack.PopStackObjectInstanceOf(
      fake_jni_.GetStockClass(FakeJni::StockClass::String));
}


TEST_F(NanoJavaStackTest, ObjectNull) {
  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      1);
  stack.PushStackObject(nullptr);
  EXPECT_EQ(nullptr, stack.PopStackObject().get());
}


TEST_F(NanoJavaStackTest, PushObjectStackOverflow) {
  EXPECT_CALL(internal_error_provider_, SetResult(_))
      .Times(Exactly(1));

  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      0);
  stack.PushStackObject(nullptr);
}


TEST_F(NanoJavaStackTest, PopObjectStackUnderflow) {
  EXPECT_CALL(internal_error_provider_, SetResult(_))
      .Times(Exactly(1));

  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      0);
  stack.PopStackObject();
}


TEST_F(NanoJavaStackTest, SingleSlotPrimitive) {
  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      2);
  stack.PushStack(Slot::Type::Int, 53);
  stack.PushStack(Slot::Type::Float, as<int32_t>(3.14f));
  EXPECT_EQ(3.14f, as<float>(stack.PopStack(Slot::Type::Float)));
  EXPECT_EQ(53, stack.PopStack(Slot::Type::Int));
}


TEST_F(NanoJavaStackTest, SingleSlotPrimitiveStackOverflow) {
  EXPECT_CALL(internal_error_provider_, SetResult(_))
      .Times(Exactly(1));

  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      0);
  stack.PushStack(Slot::Type::Int, 0);
}


TEST_F(NanoJavaStackTest, SingleSlotPrimitiveStackUnderflow) {
  EXPECT_CALL(internal_error_provider_, SetResult(_))
      .Times(Exactly(1));

  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      0);
  stack.PopStack(Slot::Type::Int);
}


TEST_F(NanoJavaStackTest, BadSinglePrimitiveSlotType) {
  EXPECT_CALL(internal_error_provider_, SetResult(_))
      .Times(Exactly(4));

  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      10);

  stack.PushStack(Slot::Type::Int, 0);
  stack.PopStack(Slot::Type::Float);

  stack.PushStack(Slot::Type::Float, 0);
  stack.PopStack(Slot::Type::Int);

  stack.PushStackObject(nullptr);
  stack.PopStack(Slot::Type::Int);

  stack.PushStack2(Slot::Type::Long, 0);
  stack.PopStack(Slot::Type::Int);
}


TEST_F(NanoJavaStackTest, DoubleSlotPrimitive) {
  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      4);
  stack.PushStack2(Slot::Type::Long, 53);
  stack.PushStack2(Slot::Type::Double, as<int64_t>(3.14));
  EXPECT_EQ(3.14, as<double>(stack.PopStack2(Slot::Type::Double)));
  EXPECT_EQ(53, stack.PopStack2(Slot::Type::Long));
}


TEST_F(NanoJavaStackTest, DoubleSlotPrimitiveStackOverflow) {
  EXPECT_CALL(internal_error_provider_, SetResult(_))
      .Times(Exactly(1));

  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      1);
  stack.PushStack2(Slot::Type::Long, 0);
}


TEST_F(NanoJavaStackTest, DoubleSlotPrimitiveStackUnderflow) {
  EXPECT_CALL(internal_error_provider_, SetResult(_))
      .Times(Exactly(1));

  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      1);
  stack.PushStack(Slot::Type::Int, 0);
  stack.PopStack2(Slot::Type::Long);
}


TEST_F(NanoJavaStackTest, BadDoublePrimitiveSlotType) {
  EXPECT_CALL(internal_error_provider_, SetResult(_))
      .Times(Exactly(4));

  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      20);

  stack.PushStack2(Slot::Type::Long, 0);
  stack.PopStack2(Slot::Type::Double);

  stack.PushStack2(Slot::Type::Double, 0);
  stack.PopStack2(Slot::Type::Long);

  stack.PushStackObject(nullptr);
  stack.PushStackObject(nullptr);
  stack.PopStack2(Slot::Type::Long);

  stack.PushStack2(Slot::Type::Long, 0);
  stack.Discard();
  stack.PushStack(Slot::Type::Int, 0);
  stack.PopStack2(Slot::Type::Long);
}


TEST_F(NanoJavaStackTest, PushStackAnyVoid) {
  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      0);

  stack.PushStackAny(JVariant());
}


TEST_F(NanoJavaStackTest, PushStackAnySingleSlot) {
  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      1);

  stack.PushStackAny(JVariant::Boolean(true));
  EXPECT_EQ(1, stack.PopStack(Slot::Type::Int));

  stack.PushStackAny(JVariant::Byte(42));
  EXPECT_EQ(42, stack.PopStack(Slot::Type::Int));

  stack.PushStackAny(JVariant::Char(52342));
  EXPECT_EQ(52342, stack.PopStack(Slot::Type::Int));

  stack.PushStackAny(JVariant::Short(-22342));
  EXPECT_EQ(-22342, stack.PopStack(Slot::Type::Int));

  stack.PushStackAny(JVariant::Int(348379845));
  EXPECT_EQ(348379845, stack.PopStack(Slot::Type::Int));

  stack.PushStackAny(JVariant::Float(3.14f));
  EXPECT_EQ(3.14f, as<float>(stack.PopStack(Slot::Type::Float)));

  stack.PushStackAny(JVariant::LocalRef(nullptr));
  EXPECT_EQ(nullptr, stack.PopStackObject());

  stack.PushStackAny(JVariant::LocalRef(JniToJavaString("hello")));
  EXPECT_EQ("hello", JniToNativeString(stack.PopStackObject().get()));
}


TEST_F(NanoJavaStackTest, PushStackAnyDoubleSlot) {
  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      2);

  stack.PushStackAny(JVariant::Long(3489379845345345L));
  EXPECT_EQ(3489379845345345L, stack.PopStack2(Slot::Type::Long));

  stack.PushStackAny(JVariant::Double(2345.2134123));
  EXPECT_EQ(2345.2134123, as<double>(stack.PopStack2(Slot::Type::Double)));
}


TEST_F(NanoJavaStackTest, PopStackAnySingleSlot) {
  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      1);

  stack.PushStack(Slot::Type::Int, 123);
  EXPECT_EQ(
      "<boolean>true",
      stack.PopStackAny(JType::Boolean).ToString(false));

  stack.PushStack(Slot::Type::Int, 0x4407);
  EXPECT_EQ(
      "<byte>7",
      stack.PopStackAny(JType::Byte).ToString(false));

  stack.PushStack(Slot::Type::Int, 55555);
  EXPECT_EQ(
      "<char>55555",
      stack.PopStackAny(JType::Char).ToString(false));

  stack.PushStack(Slot::Type::Int, -22222);
  EXPECT_EQ(
      "<short>-22222",
      stack.PopStackAny(JType::Short).ToString(false));

  stack.PushStack(Slot::Type::Int, 390459837);
  EXPECT_EQ(
      "<int>390459837",
      stack.PopStackAny(JType::Int).ToString(false));

  stack.PushStack(Slot::Type::Float, as<int32_t>(3.14f));
  EXPECT_EQ(
      "<float>3.14",
      stack.PopStackAny(JType::Float).ToString(false));
}


TEST_F(NanoJavaStackTest, PopStackAnyDoubleSlot) {
  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      2);

  stack.PushStack2(Slot::Type::Long, 39045983452337L);
  EXPECT_EQ(
      "<long>39045983452337",
      stack.PopStackAny(JType::Long).ToString(false));

  stack.PushStack2(Slot::Type::Double, as<int64_t>(38947.2134));
  EXPECT_EQ(
      "<double>38947.2134",
      stack.PopStackAny(JType::Double).ToString(false));
}


TEST_F(NanoJavaStackTest, PeekStackObjectString) {
  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      1);
  stack.PushStackObject(JniToJavaString("hello").get());
  EXPECT_EQ("hello", JniToNativeString(stack.PeekStackObject()));
  stack.Discard();
}


TEST_F(NanoJavaStackTest, PeekStackObjectNull) {
  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      1);
  stack.PushStackObject(nullptr);
  EXPECT_EQ(nullptr, stack.PeekStackObject());
}


TEST_F(NanoJavaStackTest, PeekStackObjectUnderflow) {
  EXPECT_CALL(internal_error_provider_, SetResult(_))
      .Times(Exactly(1));

  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      0);
  stack.PeekStackObject();
}


TEST_F(NanoJavaStackTest, PeekStackObjectBadType) {
  EXPECT_CALL(internal_error_provider_, SetResult(_))
      .Times(Exactly(1));

  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      1);
  stack.PushStack(Slot::Type::Int, 0);
  stack.PeekStackObject();
}


TEST_F(NanoJavaStackTest, StackDupInteger) {
  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      2);
  stack.PushStack(Slot::Type::Int, 52);

  stack.StackDup();

  EXPECT_EQ(52, stack.PopStack(Slot::Type::Int));
  EXPECT_EQ(52, stack.PopStack(Slot::Type::Int));
}


TEST_F(NanoJavaStackTest, StackDupString) {
  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      2);
  stack.PushStackObject(JniToJavaString("hello").get());

  stack.StackDup();

  EXPECT_EQ("hello", JniToNativeString(stack.PopStackObject().get()));
  EXPECT_EQ("hello", JniToNativeString(stack.PopStackObject().get()));
}


TEST_F(NanoJavaStackTest, StackDupNull) {
  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      2);
  stack.PushStackObject(nullptr);

  stack.StackDup();

  EXPECT_EQ(nullptr, stack.PopStackObject());
  EXPECT_EQ(nullptr, stack.PopStackObject());
}


TEST_F(NanoJavaStackTest, StackDupOverflow) {
  EXPECT_CALL(internal_error_provider_, SetResult(_))
      .Times(Exactly(1));

  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      1);
  stack.PushStack(Slot::Type::Int, 52);
  stack.StackDup();
}


TEST_F(NanoJavaStackTest, StackDupUnderflow) {
  EXPECT_CALL(internal_error_provider_, SetResult(_))
      .Times(Exactly(1));

  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      1);
  stack.StackDup();
}


TEST_F(NanoJavaStackTest, StackDup2SingleSlotPrimitive) {
  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      4);
  stack.PushStack(Slot::Type::Int, 18);
  stack.PushStack(Slot::Type::Int, 19);

  stack.StackDup2();

  EXPECT_EQ(19, stack.PopStack(Slot::Type::Int));
  EXPECT_EQ(18, stack.PopStack(Slot::Type::Int));
  EXPECT_EQ(19, stack.PopStack(Slot::Type::Int));
  EXPECT_EQ(18, stack.PopStack(Slot::Type::Int));
}


TEST_F(NanoJavaStackTest, StackDup2DoubleSlotPrimitive) {
  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      4);
  stack.PushStack2(Slot::Type::Double, as<int64_t>(123.456));

  stack.StackDup2();

  EXPECT_EQ(123.456, as<double>(stack.PopStack2(Slot::Type::Double)));
  EXPECT_EQ(123.456, as<double>(stack.PopStack2(Slot::Type::Double)));
}


TEST_F(NanoJavaStackTest, StackDup2String) {
  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      4);
  stack.PushStackObject(JniToJavaString("first").get());
  stack.PushStackObject(JniToJavaString("second").get());

  stack.StackDup2();

  EXPECT_EQ("second", JniToNativeString(stack.PopStackObject().get()));
  EXPECT_EQ("first", JniToNativeString(stack.PopStackObject().get()));
  EXPECT_EQ("second", JniToNativeString(stack.PopStackObject().get()));
  EXPECT_EQ("first", JniToNativeString(stack.PopStackObject().get()));
}


TEST_F(NanoJavaStackTest, StackDup2Null) {
  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      4);
  stack.PushStackObject(nullptr);
  stack.PushStackObject(nullptr);

  stack.StackDup2();

  EXPECT_EQ(nullptr, stack.PopStackObject());
  EXPECT_EQ(nullptr, stack.PopStackObject());
  EXPECT_EQ(nullptr, stack.PopStackObject());
  EXPECT_EQ(nullptr, stack.PopStackObject());
}


TEST_F(NanoJavaStackTest, StackDup2Overflow) {
  EXPECT_CALL(internal_error_provider_, SetResult(_))
      .Times(Exactly(1));

  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      3);
  stack.PushStack2(Slot::Type::Long, 52);
  stack.StackDup2();
}


TEST_F(NanoJavaStackTest, StackDup2Underflow) {
  EXPECT_CALL(internal_error_provider_, SetResult(_))
      .Times(Exactly(1));

  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      1);
  stack.PushStack(Slot::Type::Int, 0);
  stack.StackDup2();
}


TEST_F(NanoJavaStackTest, SwapPositive) {
  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      4);
  stack.PushStack(Slot::Type::Int, 1);
  stack.PushStackObject(JniToJavaString("second").get());
  stack.PushStack(Slot::Type::Int, 3);
  stack.PushStack(Slot::Type::Int, 4);

  stack.Swap(2, 3);

  EXPECT_EQ(4, stack.PopStack(Slot::Type::Int));
  EXPECT_EQ("second", JniToNativeString(stack.PopStackObject().get()));
  EXPECT_EQ(3, stack.PopStack(Slot::Type::Int));
  EXPECT_EQ(1, stack.PopStack(Slot::Type::Int));
}


TEST_F(NanoJavaStackTest, SwapBadIndexes) {
  const std::pair<int, int> test_cases[] = {
      std::make_pair(0, 1),
      std::make_pair(-1, 1),
      std::make_pair(2, 2),
      std::make_pair(5, 2),
  };

  EXPECT_CALL(internal_error_provider_, SetResult(_))
      .Times(Exactly(arraysize(test_cases) * 2));

  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      4);
  stack.PushStack(Slot::Type::Int, 1);
  stack.PushStack(Slot::Type::Int, 2);
  stack.PushStack(Slot::Type::Int, 3);
  stack.PushStack(Slot::Type::Int, 4);

  for (const auto& test_case : test_cases) {
    stack.Swap(test_case.first, test_case.second);
    stack.Swap(test_case.second, test_case.first);
  }
}


TEST_F(NanoJavaStackTest, DiscardString) {
  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      2);

  stack.PushStackObject(JniToJavaString("first").get());
  stack.PushStackObject(JniToJavaString("second").get());

  stack.Discard();

  EXPECT_EQ("first", JniToNativeString(stack.PopStackObject().get()));
}


TEST_F(NanoJavaStackTest, DiscardSingleSlotPrimitive) {
  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      2);

  stack.PushStack(Slot::Type::Float, as<int32_t>(22.22f));
  stack.PushStack(Slot::Type::Int, 1);

  stack.Discard();

  EXPECT_EQ(22.22f, as<float>(stack.PopStack(Slot::Type::Float)));
}


TEST_F(NanoJavaStackTest, DiscardDoubleSlotPrimitive) {
  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      3);

  stack.PushStack(Slot::Type::Int, 123);
  stack.PushStack2(Slot::Type::Double, as<int64_t>(11.11));

  stack.Discard();
  stack.Discard();

  EXPECT_EQ(123, stack.PopStack(Slot::Type::Int));
}


TEST_F(NanoJavaStackTest, DiscardStackUnderflow) {
  EXPECT_CALL(internal_error_provider_, SetResult(_))
      .Times(Exactly(1));

  NanoJavaStack stack(
      &internal_error_provider_,
      fn_raise_null_pointer_exception_,
      1);

  stack.Discard();
}

}  // namespace nanojava
}  // namespace cdbg
}  // namespace devtools


