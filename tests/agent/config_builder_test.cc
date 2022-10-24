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

#include "src/agent/config_builder.h"

#include "gmock/gmock.h"

#include "tests/agent/fake_jni.h"
#include "tests/agent/mock_jvmti_env.h"
#include "src/agent/type_util.h"

ABSL_DECLARE_FLAG(bool, enable_safe_caller);

namespace devtools {
namespace cdbg {

class ConfigBuilderTest : public ::testing::Test {
 protected:
  ConfigBuilderTest() : global_jvm_(fake_jni_.jvmti(), fake_jni_.jni()) {}

 protected:
  FakeJni fake_jni_;
  GlobalJvmEnv global_jvm_;
};


TEST_F(ConfigBuilderTest, SafeCallerDisabledDefaultBlock) {
  absl::FlagSaver fs;

  absl::SetFlag(&FLAGS_enable_safe_caller, false);

  std::unique_ptr<Config> configs[] = { DefaultConfig() };

  for (const auto& config : configs) {
    EXPECT_EQ(
        Config::Method::CallAction::Block,
        config->GetMethodRule("LMy;", "LMy;", "my", "()V").action);
  }
}


TEST_F(ConfigBuilderTest, Empty) {
  // We don't care about the outcome as long as it doesn't crash.
  DefaultConfig()->GetMethodRule(std::string(), std::string(), std::string(),
                                 std::string());
}


TEST_F(ConfigBuilderTest, SafeCallerEnabledDefaultInterpret) {
  absl::FlagSaver fs;

  absl::SetFlag(&FLAGS_enable_safe_caller, true);

  EXPECT_EQ(
      Config::Method::CallAction::Interpret,
      DefaultConfig()->GetMethodRule("LMy;", "LMy;", "my", "()V").action);
}


TEST_F(ConfigBuilderTest, AllowAllRule) {
  EXPECT_EQ(
      Config::Method::CallAction::Allow,
      DefaultConfig()->GetMethodRule(
          "Ljava/lang/Math;",
          "Ljava/lang/Math;",
          "nextUp",
          "(F)F").action);

  EXPECT_EQ(
      Config::Method::CallAction::Allow,
      DefaultConfig()->GetMethodRule(
          "Ljava/math/BigDecimal;",
          "Ljava/math/BigDecimal;",
          "toString",
          "()Ljava/lang/String").action);

  EXPECT_EQ(
      Config::Method::CallAction::Allow,
      DefaultConfig()->GetMethodRule(
          "Ljava/math/BigInteger;",
          "Ljava/math/BigInteger;",
          "toString",
          "()Ljava/lang/String").action);

  EXPECT_EQ(
      Config::Method::CallAction::Allow,
      DefaultConfig()->GetMethodRule(
          "Ljava/lang/String;",
          "Ljava/lang/String;",
          "concat",
          "(Ljava/lang/String;)Ljava/lang/String;").action);

  EXPECT_NE(
      Config::Method::CallAction::Allow,
      DefaultConfig()->GetMethodRule(
          "Ljava/lang/String;",
          "Ljava/lang/String;",
          "getChars",
          "([CI)V").action);
}


TEST_F(ConfigBuilderTest, GetClass) {
  EXPECT_EQ(
      Config::Method::CallAction::Allow,
      DefaultConfig()->GetMethodRule(
          "Ljava/lang/Object;",
          "Lcom/prod/MyClass;",
          "getClass",
          "()Ljava/lang/Class;").action);

  EXPECT_NE(
      Config::Method::CallAction::Allow,
      DefaultConfig()->GetMethodRule(
          "Ljava/lang/Object;",
          "Lcom/prod/MyClass;",
          "getClass",
          "(III)Ljava/lang/Class;").action);
}


TEST_F(ConfigBuilderTest, Iterator) {
  EXPECT_EQ(
      Config::Method::CallAction::Allow,
      DefaultConfig()->GetMethodRule(
          "Ljava/util/Vector;",
          "Ljava/util/Stack;",
          "iterator",
          "()Ljava/util/Iterator;").action);

  EXPECT_NE(
      Config::Method::CallAction::Allow,
      DefaultConfig()->GetMethodRule(
          "Ljava/util/Vector;",
          "Lcom/prod/MyDerivedEvilStack;",
          "iterator",
          "()Ljava/util/Iterator;").action);
}

}  // namespace cdbg
}  // namespace devtools

