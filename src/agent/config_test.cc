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

#include "config.h"

#include "gmock/gmock.h"

#include "src/agent/test_util/fake_jni.h"
#include "src/agent/test_util/mock_jvmti_env.h"
#include "type_util.h"

namespace devtools {
namespace cdbg {

class ConfigTest : public ::testing::Test {
 protected:
  ConfigTest() : global_jvm_(fake_jni_.jvmti(), fake_jni_.jni()) {}

 protected:
  FakeJni fake_jni_;
  GlobalJvmEnv global_jvm_;
};


TEST_F(ConfigTest, DefaultBlockAll) {
  std::unique_ptr<Config> config = Config::Builder().Build();

  EXPECT_EQ(
      Config::Method::CallAction::Block,
      config->GetMethodRule(
          "Ljava/lang/String;",
          "Ljava/lang/String;",
          "concat",
          "(Ljava/lang/String;)Ljava/lang/String;").action);

  EXPECT_EQ(
      Config::Method::CallAction::Block,
      config->GetMethodRule(
          "Lcom/prod/MyClass;",
          "Lcom/prod/MyClass;",
          "myMethod",
          "(Ljava/lang/String;)Ljava/lang/String;").action);
}


TEST_F(ConfigTest, DefaultQuotaInterpreterDisabled) {
  std::unique_ptr<Config> config = Config::Builder().Build();

  const Config::MethodCallQuotaType types[] = {
    Config::EXPRESSION_EVALUATION,
    Config::PRETTY_PRINTERS,
    Config::DYNAMIC_LOG
  };

  for (Config::MethodCallQuotaType type : types) {
    auto quota = config->GetQuota(type);
    EXPECT_EQ(0, quota.max_interpreter_instructions);
    EXPECT_EQ(0, quota.max_classes_load);
  }
}


TEST_F(ConfigTest, DefaultMethodConfig) {
  Config::Method default_method_config;
  default_method_config.action = Config::Method::CallAction::Allow;

  std::unique_ptr<Config> config = Config::Builder()
      .SetDefaultMethodRule(default_method_config)
      .Build();

  EXPECT_EQ(
      Config::Method::CallAction::Allow,
      config->GetMethodRule(
          "Lcom/prod/MyClass;",
          "Lcom/prod/MyClass;",
          "myMethod",
          "(Ljava/lang/String;)Ljava/lang/String;").action);
}


TEST_F(ConfigTest, MethodConfigName) {
  Config::Method method_config;
  method_config.name = "myMethod";
  method_config.action = Config::Method::CallAction::Allow;

  std::unique_ptr<Config> config = Config::Builder()
      .SetClassConfig("Lcom/prod/MyClass;", { method_config })
      .Build();

  EXPECT_EQ(
      Config::Method::CallAction::Block,
      config->GetMethodRule(
          "Ljava/lang/String;",
          "Ljava/lang/String;",
          "concat",
          "(Ljava/lang/String;)Ljava/lang/String;").action);

  EXPECT_EQ(
      Config::Method::CallAction::Allow,
      config->GetMethodRule(
          "Lcom/prod/MyClass;",
          "Lcom/prod/MyClass;",
          "myMethod",
          "(Ljava/lang/String;)Ljava/lang/String;").action);
}


TEST_F(ConfigTest, MethodConfigSignature) {
  Config::Method method_config;
  method_config.name = "myMethod";
  method_config.signature = "(II)Z";
  method_config.action = Config::Method::CallAction::Allow;

  std::unique_ptr<Config> config = Config::Builder()
      .SetClassConfig("Lcom/prod/MyClass;", { method_config })
      .Build();

  EXPECT_EQ(
      Config::Method::CallAction::Block,
      config->GetMethodRule(
          "Ljava/lang/String;",
          "Ljava/lang/String;",
          "concat",
          "(Ljava/lang/String;)Ljava/lang/String;").action);

  EXPECT_EQ(
      Config::Method::CallAction::Block,
      config->GetMethodRule(
          "Lcom/prod/MyClass;",
          "Lcom/prod/MyClass;",
          "myMethod",
          "(Ljava/lang/String;)Ljava/lang/String;").action);

  EXPECT_EQ(
      Config::Method::CallAction::Allow,
      config->GetMethodRule(
          "Lcom/prod/MyClass;",
          "Lcom/prod/MyClass;",
          "myMethod",
          "(II)Z").action);
}


TEST_F(ConfigTest, AddMethodRule) {
  Config::Method method_config1;
  method_config1.name = "myMethod";
  method_config1.signature = "(II)Z";
  method_config1.action = Config::Method::CallAction::Allow;

  Config::Method method_config2;
  method_config2.name = "myMethod";
  method_config2.action = Config::Method::CallAction::Interpret;

  std::unique_ptr<Config> config = Config::Builder()
      .SetClassConfig("Lcom/prod/MyClass;", { method_config2 })
      .AddMethodRule("Lcom/prod/MyClass;", method_config1)
      .Build();

  EXPECT_EQ(
      Config::Method::CallAction::Block,
      config->GetMethodRule(
          "Ljava/lang/String;",
          "Ljava/lang/String;",
          "concat",
          "(Ljava/lang/String;)Ljava/lang/String;").action);

  EXPECT_EQ(
      Config::Method::CallAction::Interpret,
      config->GetMethodRule(
          "Lcom/prod/MyClass;",
          "Lcom/prod/MyClass;",
          "myMethod",
          "(Ljava/lang/String;)Ljava/lang/String;").action);

  EXPECT_EQ(
      Config::Method::CallAction::Allow,
      config->GetMethodRule(
          "Lcom/prod/MyClass;",
          "Lcom/prod/MyClass;",
          "myMethod",
          "(II)Z").action);
}


TEST_F(ConfigTest, DerivedClasses) {
  Config::Method method_config1;
  method_config1.name = "myMethod1";
  method_config1.action = Config::Method::CallAction::Allow;
  method_config1.applies_to_derived_classes = true;

  Config::Method method_config2;
  method_config2.name = "myMethod2";
  method_config2.action = Config::Method::CallAction::Interpret;

  std::unique_ptr<Config> config = Config::Builder()
      .SetClassConfig("Lcom/prod/Base;", { method_config2 })
      .AddMethodRule("Lcom/prod/Base;", method_config1)
      .Build();

  EXPECT_EQ(
      Config::Method::CallAction::Allow,
      config->GetMethodRule(
          "Lcom/prod/Base;",
          "Lcom/prod/Base;",
          "myMethod1",
          "(II)Z").action);

  EXPECT_EQ(
      Config::Method::CallAction::Interpret,
      config->GetMethodRule(
          "Lcom/prod/Base;",
          "Lcom/prod/Base;",
          "myMethod2",
          "(II)Z").action);

  EXPECT_EQ(
      Config::Method::CallAction::Allow,
      config->GetMethodRule(
          "Lcom/prod/Base;",
          "Lcom/prod/Derived;",
          "myMethod1",
          "(II)Z").action);

  EXPECT_EQ(
      Config::Method::CallAction::Block,
      config->GetMethodRule(
          "Lcom/prod/Base;",
          "Lcom/prod/Derived;",
          "myMethod2",
          "(II)Z").action);
}


TEST_F(ConfigTest, Precedence) {
  Config::Method method_config_base;
  method_config_base.name = "myMethod";
  method_config_base.action = Config::Method::CallAction::Allow;
  method_config_base.applies_to_derived_classes = true;

  Config::Method method_config_derived;
  method_config_derived.name = "myMethod";
  method_config_derived.action = Config::Method::CallAction::Interpret;

  std::unique_ptr<Config> config = Config::Builder()
      .SetClassConfig("Lcom/prod/Base;", { method_config_base })
      .SetClassConfig("Lcom/prod/Derived;", { method_config_derived })
      .Build();

  EXPECT_EQ(
      Config::Method::CallAction::Allow,
      config->GetMethodRule(
          "Lcom/prod/Base;",
          "Lcom/prod/Base;",
          "myMethod",
          "(II)Z").action);

  EXPECT_EQ(
      Config::Method::CallAction::Interpret,
      config->GetMethodRule(
          "Lcom/prod/Base;",
          "Lcom/prod/Derived;",
          "myMethod",
          "(II)Z").action);
}

}  // namespace cdbg
}  // namespace devtools

