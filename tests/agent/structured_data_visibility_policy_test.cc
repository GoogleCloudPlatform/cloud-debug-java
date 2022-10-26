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

#include "src/agent/structured_data_visibility_policy.h"

#include "gtest/gtest.h"
#include "tests/agent/fake_jni.h"
#include "tests/agent/mock_jvmti_env.h"

using testing::Invoke;
using testing::NotNull;
using testing::WithArg;

namespace devtools {
namespace cdbg {

static const jclass kClass = reinterpret_cast<jclass>(0x12345678);

class StructuredDataVisibilityPolicyTest : public ::testing::Test {
 protected:
  StructuredDataVisibilityPolicyTest()
      : fake_jni_(&jvmti_),
        global_jvm_(&jvmti_, fake_jni_.jni()) {
  }

  void SetUp() override {
    EXPECT_CALL(jvmti_, Deallocate(NotNull()))
        .WillRepeatedly(Invoke([] (unsigned char* buffer) {
          delete[] buffer;
          return JVMTI_ERROR_NONE;
        }));
  }

  void SetClassSignature(const std::string& signature) {
    EXPECT_CALL(jvmti_, GetClassSignature(kClass, NotNull(), nullptr))
        .WillRepeatedly(WithArg<1>(Invoke([signature] (char** buffer) {
          *buffer = new char[signature.size() + 1];
          memcpy(*buffer, signature.c_str(), signature.size() + 1);
          return JVMTI_ERROR_NONE;
        })));
  }

 protected:
  MockJvmtiEnv jvmti_;
  FakeJni fake_jni_;
  GlobalJvmEnv global_jvm_;
};


TEST_F(StructuredDataVisibilityPolicyTest, Empty) {
  const std::string test_cases[] = {
      "LMyClass;",
      "LMyClass$Inner1;",
      "LMyClass$Inner1$Inner2;",
      "Lcom/MyClass$Inner1$Inner2;",
      "Lcom/something/MyClass$Inner1$Inner2;",
      "Lcom/something/more/MyClass$Inner1$Inner2;",
      "",                    // Invalid input.
      "L",                   // Invalid input.
      "Lcom",                // Invalid input.
      "Lcom/",               // Invalid input.
      "Lcom/MyClass",        // Invalid input.
      "Lcom/MyClass$Inner",  // Invalid input.
      "L;",                  // Invalid input.
      ";",                   // Invalid input.
  };

  StructuredDataVisibilityPolicy::Config config;
  config.packages["org/whatever"] = {};

  StructuredDataVisibilityPolicy data_visibility;
  data_visibility.SetConfig(config);
  for (const std::string& test_case : test_cases) {
    SetClassSignature(test_case);
    EXPECT_EQ(nullptr, data_visibility.GetClassVisibility(kClass));
  }
}


TEST_F(StructuredDataVisibilityPolicyTest, PackageInvisible) {
  const std::string test_cases[] = {"Lcom/secret/MyClass;",
                                    "Lcom/secret/MyClass$Inner1;",
                                    "Lcom/secret/MyClass$Inner1$Inner2;"};

  StructuredDataVisibilityPolicy::Config config;
  config.packages["com/secret"].invisible = true;

  StructuredDataVisibilityPolicy data_visibility;
  data_visibility.SetConfig(config);
  for (const std::string& test_case : test_cases) {
    SetClassSignature(test_case);
    auto class_visibility = data_visibility.GetClassVisibility(kClass);
    ASSERT_NE(nullptr, class_visibility);

    EXPECT_FALSE(class_visibility->IsFieldVisible("someField", 0));
    EXPECT_FALSE(class_visibility->IsVariableVisible("myMethod", "()V", "var"));
  }
}


TEST_F(StructuredDataVisibilityPolicyTest, ParentTopLevelClassInvisible) {
  const std::string test_cases[] = {"Lcom/secret/MyClass$Inner1;",
                                    "Lcom/secret/MyClass$Inner1$Inner2;"};

  StructuredDataVisibilityPolicy::Config config;
  config.packages["com/secret"].classes["MyClass"].invisible = true;

  StructuredDataVisibilityPolicy data_visibility;
  data_visibility.SetConfig(config);
  for (const std::string& test_case : test_cases) {
    SetClassSignature(test_case);
    auto class_visibility = data_visibility.GetClassVisibility(kClass);
    ASSERT_NE(nullptr, class_visibility);

    EXPECT_FALSE(class_visibility->IsFieldVisible("someField", 0));
    EXPECT_FALSE(class_visibility->IsVariableVisible("myMethod", "()V", "var"));
  }
}


TEST_F(StructuredDataVisibilityPolicyTest, ParentNestedClassInvisible) {
  const struct {
    std::string signature;
    bool expected_visible;
  } test_cases[] = {
      { "Lcom/secret/MyClass;", true },
      { "Lcom/secret/MyClass$Inner1;", false },
      { "Lcom/secret/MyClass$Inner1$Inner2;", false },
      { "Lcom/secret/MyClass$Inner1$Inner2$Inner3;", false }
  };

  StructuredDataVisibilityPolicy::Config config;
  config.packages["com/secret"]
        .classes["MyClass"]
        .nested_classes["Inner1"]
        .invisible = true;

  StructuredDataVisibilityPolicy data_visibility;
  data_visibility.SetConfig(config);
  for (const auto& test_case : test_cases) {
    SetClassSignature(test_case.signature);
    auto class_visibility = data_visibility.GetClassVisibility(kClass);
    ASSERT_NE(nullptr, class_visibility) << test_case.signature;

    EXPECT_EQ(test_case.expected_visible,
              class_visibility->IsFieldVisible("someField", 0));
    std::string reason;
    EXPECT_TRUE(class_visibility->IsFieldDataVisible("somefield", 0, &reason));
    EXPECT_EQ(test_case.expected_visible,
              class_visibility->IsVariableVisible("myMethod", "()V", "var"));
    EXPECT_TRUE(class_visibility->IsVariableDataVisible(
        "myMethod", "()V", "var", &reason));
  }
}


TEST_F(StructuredDataVisibilityPolicyTest, FieldInvisible) {
  StructuredDataVisibilityPolicy::Config config;
  auto& fields = config.packages[""].classes["MyClass"].fields;
  fields.resize(2);
  fields[0].name = "f1";
  fields[0].invisible = true;
  fields[1].name = "f2";
  fields[1].invisible = false;

  StructuredDataVisibilityPolicy data_visibility;
  data_visibility.SetConfig(config);
  SetClassSignature("LMyClass;");
  auto class_visibility = data_visibility.GetClassVisibility(kClass);
  ASSERT_NE(nullptr, class_visibility);

  std::string reason;
  EXPECT_FALSE(class_visibility->IsFieldVisible("f1", 0));
  EXPECT_TRUE(class_visibility->IsFieldVisible("f2", 0));
  EXPECT_TRUE(class_visibility->IsFieldDataVisible("f2", 0, &reason));
  EXPECT_TRUE(class_visibility->IsFieldVisible("f3", 0));
  EXPECT_TRUE(class_visibility->IsFieldDataVisible("f3", 0, &reason));
}


TEST_F(StructuredDataVisibilityPolicyTest, VariableInvisible) {
  const struct {
    std::string method_name;
    std::string method_signature;
    std::string variable_name;
    bool expected_visibility;
  } test_cases[] = {
      { "otherMethod", "()J", "v1", true },
      { "myMethod", "()D", "v1", true },
      { "myMethod", "()J", "v1", false },
      { "myMethod", "()J", "v2", true },
      { "myMethod", "()J", "v3", true }
  };

  StructuredDataVisibilityPolicy::Config config;
  auto& methods = config.packages[""].classes["MyClass"].methods;
  methods.resize(1);
  methods[0].name = "myMethod";
  methods[0].signature = "()J";
  methods[0].variables.resize(2);
  methods[0].variables[0].name = "v1";
  methods[0].variables[0].invisible = true;
  methods[0].variables[1].name = "v2";
  methods[0].variables[1].invisible = false;

  StructuredDataVisibilityPolicy data_visibility;
  data_visibility.SetConfig(config);
  SetClassSignature("LMyClass;");
  auto class_visibility = data_visibility.GetClassVisibility(kClass);
  ASSERT_NE(nullptr, class_visibility);

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(
        test_case.expected_visibility,
        class_visibility->IsVariableVisible(
            test_case.method_name,
            test_case.method_signature,
            test_case.variable_name));
    std::string reason;
    EXPECT_TRUE(
        class_visibility->IsVariableDataVisible(
            test_case.method_name,
            test_case.method_signature,
            test_case.variable_name,
            &reason));
  }
}

}  // namespace cdbg
}  // namespace devtools
