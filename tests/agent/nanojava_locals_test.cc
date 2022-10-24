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

#include "src/agent/nanojava_locals.h"

#include <cstdint>

#include "gmock/gmock.h"

#include "src/agent/test_util/fake_jni.h"
#include "src/agent/test_util/mock_jvmti_env.h"
#include "src/agent/test_util/mock_nanojava_internal_error_provider.h"

using testing::_;
using testing::Exactly;
using testing::Return;
using testing::StrictMock;

namespace devtools {
namespace cdbg {
namespace nanojava {

class NanoJavaLocalsTest : public testing::Test {
 public:
  NanoJavaLocalsTest() : global_jvm_(fake_jni_.jvmti(), fake_jni_.jni()) {}

  void SetUp() override {
    EXPECT_CALL(internal_error_provider_, method_name())
        .WillRepeatedly(Return(std::string()));

    EXPECT_CALL(internal_error_provider_, FormatCallStack())
        .WillRepeatedly(Return(std::string()));
  }

  // "NanoJavaLocals" does not free local references. It assumes that they
  // go away when the method execution is done. This causes "FakeJni" to
  // complain about leaking references. This function sets all the local
  // variables to integer, thus releasing all local variables.
  void ResetLocals(NanoJavaLocals* locals) {
    EXPECT_CALL(internal_error_provider_, SetResult(_))
        .WillRepeatedly(Return());

    for (int i = 0; i < 100; ++i) {
      locals->SetLocal(i, Slot::Type::Int, 0);
    }
  }

 protected:
  FakeJni fake_jni_;
  GlobalJvmEnv global_jvm_;
  StrictMock<MockNanoJavaInternalErrorProvider> internal_error_provider_;
};


TEST_F(NanoJavaLocalsTest, NoLocals) {
  NanoJavaLocals locals(&internal_error_provider_, 0);
}


TEST_F(NanoJavaLocalsTest, LocalObjectString) {
  NanoJavaLocals locals(&internal_error_provider_, 1);
  locals.SetLocalObject(0, JniToJavaString("hello").get());
  EXPECT_EQ(
      "hello",
      JniToNativeString(locals.GetLocalObject(0)));

  ResetLocals(&locals);
}


TEST_F(NanoJavaLocalsTest, LocalObjectNull) {
  NanoJavaLocals locals(&internal_error_provider_, 1);
  locals.SetLocalObject(0, nullptr);
  EXPECT_EQ(nullptr, locals.GetLocalObject(0));
}


TEST_F(NanoJavaLocalsTest, BadLocalObjectIndex) {
  EXPECT_CALL(internal_error_provider_, SetResult(_))
      .Times(Exactly(2));

  NanoJavaLocals locals(&internal_error_provider_, 1);
  locals.SetLocalObject(-1, nullptr);
  locals.SetLocalObject(1, nullptr);
}


TEST_F(NanoJavaLocalsTest, BadLocalObjectSlot) {
  EXPECT_CALL(internal_error_provider_, SetResult(_))
      .Times(Exactly(2));

  NanoJavaLocals locals(&internal_error_provider_, 1);
  locals.GetLocalObject(0);
  locals.SetLocal(0, Slot::Type::Int, 0);
  locals.GetLocalObject(0);
}


TEST_F(NanoJavaLocalsTest, SingleSlotPrimitiveLocal) {
  NanoJavaLocals locals(&internal_error_provider_, 2);
  locals.SetLocal(0, Slot::Type::Int, 15);
  locals.SetLocal(1, Slot::Type::Float, as<int32_t>(3.14f));

  EXPECT_EQ(15, locals.GetLocal(0, Slot::Type::Int));
  EXPECT_EQ(3.14f, as<float>(locals.GetLocal(1, Slot::Type::Float)));
}


TEST_F(NanoJavaLocalsTest, BadSingleSlotPrimitiveIndex) {
  EXPECT_CALL(internal_error_provider_, SetResult(_))
      .Times(Exactly(2));

  NanoJavaLocals locals(&internal_error_provider_, 1);
  locals.SetLocal(-1, Slot::Type::Int, 0);
  locals.SetLocal(1, Slot::Type::Int, 0);
}


TEST_F(NanoJavaLocalsTest, DoubleSlotPrimitiveLocal) {
  NanoJavaLocals locals(&internal_error_provider_, 4);
  locals.SetLocal2(0, Slot::Type::Long, 3485348763534345L);
  locals.SetLocal2(2, Slot::Type::Double, as<int64_t>(23423.4423432));

  EXPECT_EQ(3485348763534345L, locals.GetLocal2(0, Slot::Type::Long));
  EXPECT_EQ(23423.4423432, as<double>(locals.GetLocal2(2, Slot::Type::Double)));
}


TEST_F(NanoJavaLocalsTest, BadDoubleSlotPrimitiveIndex) {
  EXPECT_CALL(internal_error_provider_, SetResult(_))
      .Times(Exactly(3));

  NanoJavaLocals locals(&internal_error_provider_, 3);
  locals.SetLocal2(-1, Slot::Type::Long, 0);
  locals.SetLocal2(2, Slot::Type::Long, 0);
  locals.SetLocal2(2, Slot::Type::Long, 0);
}


TEST_F(NanoJavaLocalsTest, BadSinglePrimitiveSlot) {
  EXPECT_CALL(internal_error_provider_, SetResult(_))
      .Times(Exactly(4));

  NanoJavaLocals locals(&internal_error_provider_, 2);
  locals.GetLocal(0, Slot::Type::Int);

  locals.SetLocal(0, Slot::Type::Int, 0);
  locals.GetLocal(0, Slot::Type::Float);

  locals.SetLocal2(0, Slot::Type::Long, 0);
  locals.GetLocal(0, Slot::Type::Int);

  locals.SetLocalObject(0, nullptr);
  locals.GetLocal(0, Slot::Type::Int);
}


TEST_F(NanoJavaLocalsTest, BadDoublePrimitiveSlot) {
  EXPECT_CALL(internal_error_provider_, SetResult(_))
      .Times(Exactly(5));

  NanoJavaLocals locals(&internal_error_provider_, 2);
  locals.GetLocal2(0, Slot::Type::Long);

  locals.SetLocal2(0, Slot::Type::Long, 0);
  locals.GetLocal2(0, Slot::Type::Double);

  locals.SetLocal2(0, Slot::Type::Long, 0);
  locals.SetLocal(0, Slot::Type::Int, 0);
  locals.GetLocal2(0, Slot::Type::Long);

  locals.SetLocal2(0, Slot::Type::Double, 0);
  locals.SetLocal(0, Slot::Type::Int, 0);
  locals.GetLocal2(0, Slot::Type::Double);

  locals.SetLocalObject(0, nullptr);
  locals.SetLocalObject(1, nullptr);
  locals.GetLocal2(0, Slot::Type::Double);
}

}  // namespace nanojava
}  // namespace cdbg
}  // namespace devtools


