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

#include "src/agent/type_util.h"

#include "gtest/gtest.h"
#include "tests/agent/fake_jni.h"
#include "tests/agent/mock_jvmti_env.h"

namespace devtools {
namespace cdbg {

class TypeUtilTest : public ::testing::Test {
 protected:
  TypeUtilTest() : global_jvm_(fake_jni_.jvmti(), fake_jni_.jni()) {}

 protected:
  FakeJni fake_jni_;
  GlobalJvmEnv global_jvm_;
};

TEST_F(TypeUtilTest, JTypeFromSignature) {
  const struct {
    std::string signature;
    JSignature expected_jsignature;
  } test_cases[] = {
      {"Z", {JType::Boolean}},
      {"C", {JType::Char}},
      {"B", {JType::Byte}},
      {"S", {JType::Short}},
      {"I", {JType::Int}},
      {"J", {JType::Long}},
      {"F", {JType::Float}},
      {"D", {JType::Double}},
      {"Ljava/lang/String;", {JType::Object, "Ljava/lang/String;"}},
      {"[[Ljava/lang/String;", {JType::Object, "[[Ljava/lang/String;"}},
      {std::string(), {JType::Void}},  // Invalid signature.
      {"\0", {JType::Void}},           // Invalid signature.
      {"junk", {JType::Void}}          // Invalid signature.
  };

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(
        test_case.expected_jsignature.type,
        JTypeFromSignature(test_case.signature));

    EXPECT_EQ(
        test_case.expected_jsignature.type,
        JSignatureFromSignature(test_case.signature).type);
    EXPECT_EQ(
        test_case.expected_jsignature.object_signature,
        JSignatureFromSignature(test_case.signature).object_signature);

    EXPECT_EQ(
        test_case.expected_jsignature.type,
        JTypeFromSignature(test_case.signature.c_str()));

    EXPECT_EQ(
        test_case.expected_jsignature.type,
        JSignatureFromSignature(test_case.signature.c_str()).type);
    EXPECT_EQ(
        test_case.expected_jsignature.object_signature,
        JSignatureFromSignature(test_case.signature.c_str()).object_signature);

    if (!test_case.signature.empty()) {
      EXPECT_EQ(
          test_case.expected_jsignature.type,
          JTypeFromSignature(test_case.signature[0]));
    }
  }
}

TEST_F(TypeUtilTest, SignatureFromJSignature) {
  const struct {
    JSignature signature;
    std::string expected;
  } test_cases[] = {
    { { JType::Void }, "V" },
    { { JType::Boolean }, "Z" },
    { { JType::Char }, "C" },
    { { JType::Byte }, "B" },
    { { JType::Short }, "S" },
    { { JType::Int }, "I" },
    { { JType::Long }, "J" },
    { { JType::Float }, "F" },
    { { JType::Double }, "D" },
    { { JType::Object, "Ljava/lang/String;" }, "Ljava/lang/String;" },
    { { JType::Object, "[[Ljava/lang/String;" }, "[[Ljava/lang/String;" }
  };

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(
        test_case.expected,
        SignatureFromJSignature(test_case.signature));
  }
}

TEST_F(TypeUtilTest, ParseJMethodSignaturePositive) {
  const struct {
    std::string input;
    JMethodSignature expected_output;
  } test_cases[] = {
    {
      "()V",
      {
        { JType::Void },
        {
        }
      }
    },
    {
      "(IF)Z",
      {
        { JType::Boolean },
        {
          { JType::Int },
          { JType::Float }
        }
      }
    },
    {
      "(Ljava/lang/String;)[Ljava/lang/String;",
      {
        { JType::Object, "[Ljava/lang/String;" },
        {
          { JType::Object, "Ljava/lang/String;" }
        }
      }
    },
    {
      "(I[Ljava/lang/String;Z)Ljava/lang/String;",
      {
        { JType::Object, "Ljava/lang/String;" },
        {
          { JType::Int },
          { JType::Object, "[Ljava/lang/String;" },
          { JType::Boolean }
        }
      }
    },
    {
      "(I[J[[F[[[Z[[[C)[D",
      {
        { JType::Object, "[D" },
        {
          { JType::Int },
          { JType::Object, "[J" },
          { JType::Object, "[[F" },
          { JType::Object, "[[[Z" },
          { JType::Object, "[[[C" },
        }
      }
    }
  };

  for (const auto& test_case : test_cases) {
    JMethodSignature actual_output;
    EXPECT_TRUE(ParseJMethodSignature(test_case.input, &actual_output))
        << test_case.input;

    EXPECT_EQ(
        test_case.expected_output.return_type.type,
        actual_output.return_type.type)
        << "Return type of " << test_case.input;

    EXPECT_EQ(
        test_case.expected_output.return_type.object_signature,
        actual_output.return_type.object_signature)
        << "Return type of " << test_case.input;

    ASSERT_EQ(
        test_case.expected_output.arguments.size(),
        actual_output.arguments.size())
        << "Number of arguments in " << test_case.input;

    for (int i = 0; i < actual_output.arguments.size(); ++i) {
      EXPECT_EQ(
          test_case.expected_output.arguments[i].type,
          actual_output.arguments[i].type)
          << "Argument " << i << " in " << test_case.input;

      EXPECT_EQ(
          test_case.expected_output.arguments[i].object_signature,
          actual_output.arguments[i].object_signature)
          << "Argument " << i << " in " << test_case.input;
    }
  }
}


TEST_F(TypeUtilTest, ParseJMethodSignatureNegative) {
  const std::string test_cases[] = {"(",
                                    "()",
                                    "(Z)",
                                    "(Lcom/prod/MyClass)V",
                                    "(Lcom/prod/MyClass;",
                                    "(Lcom/prod/MyClass;)",
                                    "([[",
                                    "([[I",
                                    "([[L",
                                    "([[)L",
                                    "(V)[I;",
                                    "(IZV)[I;"};

  for (const std::string& test_case : test_cases) {
    JMethodSignature actual_output;
    EXPECT_FALSE(ParseJMethodSignature(test_case, &actual_output))
        << test_case;
  }
}


TEST_F(TypeUtilTest, TrimReturnTypePositive) {
  const struct {
    std::string method_signature;
    std::string expected;
  } test_cases[] = {
    { "()I", "()" },
    { "(Ljava/lang/String;)Ljava/lang/String;", "(Ljava/lang/String;)" },
    { "([IJ[[Z)[[F", "([IJ[[Z)" },
    { "(IIII)[[Lcom/prod/MyClass;", "(IIII)" },
    { "()[[Lcom/prod/MyClass;", "()" }
  };

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(test_case.expected, TrimReturnType(test_case.method_signature));
  }
}


TEST_F(TypeUtilTest, TrimReturnTypeNegative) {
  const std::string test_cases[] = {
      "", "(", ")", "(III", "(III)", ")(",
  };

  for (const std::string& test_case : test_cases) {
    EXPECT_EQ(test_case, TrimReturnType(test_case));
  }
}

TEST_F(TypeUtilTest, IsArrayObjectType) {
  EXPECT_TRUE(IsArrayObjectType({ JType::Object, "[Z" }));
  EXPECT_TRUE(IsArrayObjectType({ JType::Object, "[java/lang/String;" }));
  EXPECT_FALSE(IsArrayObjectType({ JType::Object, "java/lang/String;" }));
  EXPECT_FALSE(IsArrayObjectType({ JType::Int }));
  EXPECT_FALSE(IsArrayObjectType({ JType::Boolean }));
}


TEST_F(TypeUtilTest, GetArrayElementJSignature) {
  const struct {
    std::string object_signature;
    JSignature expected_array_element_signature;
  } test_cases[] = {
    { "[Z", { JType::Boolean } },
    { "[C", { JType::Char } },
    { "[B", { JType::Byte } },
    { "[S", { JType::Short } },
    { "[I", { JType::Int } },
    { "[J", { JType::Long } },
    { "[F", { JType::Float } },
    { "[D", { JType::Double } },
    { "[Ljava/lang/String;", { JType::Object, "Ljava/lang/String;" } },
    { "[[Ljava/lang/String;", { JType::Object, "[Ljava/lang/String;" } },
    { "[[Z", { JType::Object, "[Z" } },
    { "[[[B", { JType::Object, "[[B" } }
  };

  for (const auto& test_case : test_cases) {
    const JSignature array_signature = {
      JType::Object,
      test_case.object_signature
    };

    const JSignature actual_array_element_signature =
        GetArrayElementJSignature(array_signature);

    EXPECT_EQ(
        test_case.expected_array_element_signature.type,
        actual_array_element_signature.type)
        << test_case.object_signature;

    EXPECT_EQ(
        test_case.expected_array_element_signature.object_signature,
        actual_array_element_signature.object_signature)
        << test_case.object_signature;
  }

  // Just verify that this calls with invalid arguments doesn't cause a crash.
  GetArrayElementJSignature({ JType::Object, "[" });
}


TEST_F(TypeUtilTest, WellKnownJClassFromSignature) {
  const struct {
    JSignature signature;
    WellKnownJClass expected_well_known_jclass;
  } test_cases[] = {
    { { JType::Boolean }, WellKnownJClass::Unknown },
    { { JType::Char }, WellKnownJClass::Unknown },
    { { JType::Byte }, WellKnownJClass::Unknown },
    { { JType::Short }, WellKnownJClass::Unknown },
    { { JType::Int }, WellKnownJClass::Unknown },
    { { JType::Long }, WellKnownJClass::Unknown },
    { { JType::Float }, WellKnownJClass::Unknown },
    { { JType::Double }, WellKnownJClass::Unknown },
    { { JType::Object, "Ljava/lang/String;" }, WellKnownJClass::String },
    { { JType::Object, "[Ljava/lang/String;" }, WellKnownJClass::Array },
    { { JType::Object, "[Z" }, WellKnownJClass::Array },
    { { JType::Object, "[[B" }, WellKnownJClass::Array },
    { { JType::Object, "LMyObject;" }, WellKnownJClass::Unknown },
  };

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(test_case.expected_well_known_jclass,
              WellKnownJClassFromSignature(test_case.signature));
  }
}


TEST_F(TypeUtilTest, TypeNameFromSignature) {
  const struct {
    JSignature signature;
    std::string expected_type_name;
  } test_cases[] = {
      // Regular Java types.
      {{JType::Boolean}, "boolean"},
      {{JType::Byte}, "byte"},
      {{JType::Char}, "char"},
      {{JType::Short}, "short"},
      {{JType::Int}, "int"},
      {{JType::Long}, "long"},
      {{JType::Float}, "float"},
      {{JType::Double}, "double"},
      {{JType::Object, "[Z"}, "boolean[]"},
      {{JType::Object, "[[I"}, "int[][]"},
      {{JType::Object, std::string()}, "java.lang.Object"},
      {{JType::Object, "Ljava/lang/String;"}, "java.lang.String"},
      {{JType::Object, "LMyClass;"}, "MyClass"},
      {{JType::Object, "Lcom/MyClass;"}, "com.MyClass"},
      {{JType::Object, "Lcom/ne/or/ed/MyClass;"}, "com.ne.or.ed.MyClass"},
      {{JType::Object, "Lcom/MyClass$Inner;"}, "com.MyClass.Inner"},
      {{JType::Object, "Lcom/MyClass$A$B;"}, "com.MyClass.A.B"},
      {{JType::Object, "Lcom/MyClass$0"}, "com.MyClass$0"},
      {{JType::Object, "Lcom/MyClass$743"}, "com.MyClass$743"},
      {{JType::Object, "Lc/MyCl$2$Real$3$Unreal"}, "c.MyCl$2.Real$3.Unreal"},
      {{JType::Object, "[[Ljava/lang/String;"}, "java.lang.String[][]"},
      {{JType::Object, "LA;"}, "A"},

      // Scala singletons.
      {{JType::Object, "LA$;"}, "A$"},
      {{JType::Object, "Lcom/MyClass$;"}, "com.MyClass$"},
      {{JType::Object, "Lcom/MyClass$A$B$;"}, "com.MyClass.A.B$"},
      {{JType::Object, "Lcom/MyClass$$Inner"}, "com.MyClass$.Inner"},

      // Invalid types.
      {{JType::Object, "Lcom/MyClass$"}, "com.MyClass."},
      {{JType::Object, "Lcom/MyClass$2$"}, "com.MyClass$2."},
      {{JType::Object, "L"}, ""},
      {{JType::Object, "L;"}, ""},
      {{JType::Object, ";"}, ""},
      {{JType::Object, "A;"}, "A"},
      {{JType::Object, "LA"}, "A"},
  };

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(
        test_case.expected_type_name,
        TypeNameFromSignature(test_case.signature));
  }
}


TEST_F(TypeUtilTest, TrimJObjectSignature) {
  const struct {
    std::string signature;
    std::string expected_type_name;
  } test_cases[] = {
    // Regular Java types.
    { "Ljava/lang/String;", "java/lang/String" },
    { "LMyClass;", "MyClass" },
    { "Lcom/MyClass;", "com/MyClass" },
    { "Lcom/ne/or/ed/MyClass;", "com/ne/or/ed/MyClass" },
    { "Lcom/MyClass$Inner;", "com/MyClass$Inner" },
    { "Lcom/MyClass$A$B;", "com/MyClass$A$B" },
    { "Lcom/MyClass$0", "com/MyClass$0" },
    { "Lcom/MyClass$743", "com/MyClass$743" },
    { "Lc/MyCl$2$Real$3$Unreal", "c/MyCl$2$Real$3$Unreal" },
    { "LA;", "A" },

    // Scala singletons.
    { "LMyClass$;", "MyClass$" },
    { "Lcom/MyClass$;", "com/MyClass$" },
    { "Lcom/MyClass$A$B$;", "com/MyClass$A$B$" },
    { "Lcom/MyClass$$Inner;", "com/MyClass$$Inner" },

    // Invalid types.
    { "Lcom/MyClass$", "com/MyClass$" },
    { "Lcom/MyClass$2$", "com/MyClass$2$" },
    { "", "" },
    { "A", "A" },
    { "L", "" },
    { "L;", "" },
    { ";", "" },
    { "A;", "A" },
    { "LA", "A" },
  };

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(
        test_case.expected_type_name,
        TrimJObjectSignature(test_case.signature));
  }
}


TEST_F(TypeUtilTest, BinaryNameFromJObjectSignature) {
  const struct {
    std::string object_signature;
    std::string expected_binary_name;
  } test_cases[] = {
    // Regular Java types.
    { "Ljava/lang/String;", "java.lang.String" },
    { "Lcom/prod/MyClass$Inner;", "com.prod.MyClass$Inner" },
    { "Lcom/prod/MyClass$Inner1$Inner2;", "com.prod.MyClass$Inner1$Inner2" },
    { "[Ljava/util/Map;", "[Ljava.util.Map;" },
    { "[Ljava/util/Map$Node;", "[Ljava.util.Map$Node;" },

    // Scala singletons.
    { "Lcom/prod/MyClass$;", "com.prod.MyClass$" },
    { "Lcom/prod/MyClass$Inner$;", "com.prod.MyClass$Inner$" },
    { "Lcom/prod/MyClass$$Inner;", "com.prod.MyClass$$Inner" },

    // Invalid types.
    { "[B", "[B" },
    { "[C", "[C" },
    { "", "" },
    { "a", "a" },
    { "L", "L" },
    { "La", "" },
    { "[", "[" },
  };

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(
        test_case.expected_binary_name,
        BinaryNameFromJObjectSignature(test_case.object_signature))
        << "Input: " << test_case.object_signature;
  }
}


TEST_F(TypeUtilTest, PrimitiveTypeNameToJType) {
  const struct {
    std::string type_name;
    JType type;
    bool expected_return_value;
  } test_cases[] = {
    { "byte", JType::Byte, true },
    { "char", JType::Char, true },
    { "short", JType::Short, true },
    { "int", JType::Int, true },
    { "long", JType::Long, true },
    { "float", JType::Float, true },
    { "double", JType::Double, true },
    { "boolean", JType::Boolean, true },
    { "MyClass", JType::Object, false },
    { "MyClass$", JType::Object, false },
  };

  for (const auto& test_case : test_cases) {
    JType type;
    EXPECT_EQ(
        test_case.expected_return_value,
        PrimitiveTypeNameToJType(test_case.type_name, &type));
    if (test_case.expected_return_value) {
      EXPECT_EQ(test_case.type, type);
    }
  }
}


TEST_F(TypeUtilTest, NumericTypeNameToJType) {
  const struct {
    std::string type_name;
    JType type;
    bool expected_return_value;
  } test_cases[] = {
    { "byte", JType::Byte, true },
    { "char", JType::Char, true },
    { "short", JType::Short, true },
    { "int", JType::Int, true },
    { "long", JType::Long, true },
    { "float", JType::Float, true },
    { "double", JType::Double, true },
    { "boolean", JType::Boolean, false },
    { "MyClass", JType::Object, false },
  };

  for (const auto& test_case : test_cases) {
    JType type;
    EXPECT_EQ(
        test_case.expected_return_value,
        NumericTypeNameToJType(test_case.type_name, &type));
    if (test_case.expected_return_value) {
      EXPECT_EQ(test_case.type, type);
    }
  }
}


TEST_F(TypeUtilTest, IsNumericTypeName) {
  const struct {
    std::string type_name;
    bool expected_return_value;
  } test_cases[] = {
    { "byte", true },
    { "char", true },
    { "short", true },
    { "int", true },
    { "long", true },
    { "float", true },
    { "double", true },
    { "boolean", false },
    { "MyClass", false },
    { "MyClass$", false },
  };

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(
        test_case.expected_return_value,
        IsNumericTypeName(test_case.type_name));
  }
}


TEST_F(TypeUtilTest, IsNumericJType) {
  const struct {
    JType type;
    bool expected_return_value;
  } test_cases[] = {
    { JType::Byte, true },
    { JType::Char, true },
    { JType::Short, true },
    { JType::Int, true },
    { JType::Long, true },
    { JType::Float, true },
    { JType::Double, true },
    { JType::Boolean, false },
    { JType::Object, false },
  };

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(test_case.expected_return_value, IsNumericJType(test_case.type));
  }
}


TEST_F(TypeUtilTest, FormatArrayIndexName) {
  const struct {
    int n;
    std::string expected_index_name;
  } test_cases[] = {
    { 0, "[0]" },
    { 100, "[100]" },
    { 19999, "[19999]" },
    { -1, "[-1]" },
    { std::numeric_limits<int>::max(), "[2147483647]" },
    { std::numeric_limits<int>::min(), "[-2147483648]" },
  };

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(
        test_case.expected_index_name,
        FormatArrayIndexName(test_case.n));
  }
}


TEST_F(TypeUtilTest, AppendArgumentToDescriptor) {
  const struct {
    std::string descriptor;
    std::string extra_argument;
    Nullable<std::string> expected_descriptor;
  } test_cases[] = {
      {"()V", "Ljava/lang/String;",
       Nullable<std::string>("(Ljava/lang/String;)V")},
      {"()Ljava/lang/String;", "Ljava/lang/String;",
       Nullable<std::string>("(Ljava/lang/String;)Ljava/lang/String;")},
      {"(Ljava/lang/Class;)V", "Ljava/lang/String;",
       Nullable<std::string>("(Ljava/lang/Class;Ljava/lang/String;)V")},
      {"(invalid", "Ljava/lang/String;", Nullable<std::string>()},
  };

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(
        test_case.expected_descriptor,
        AppendExtraArgumentToDescriptor(
            test_case.descriptor,
            test_case.extra_argument));
  }
}


TEST_F(TypeUtilTest, PrependArgumentToDescriptor) {
  const struct {
    std::string descriptor;
    std::string extra_argument;
    Nullable<std::string> expected_descriptor;
  } test_cases[] = {
      {"()V", "Ljava/lang/String;",
       Nullable<std::string>("(Ljava/lang/String;)V")},
      {"()Ljava/lang/String;", "Ljava/lang/String;",
       Nullable<std::string>("(Ljava/lang/String;)Ljava/lang/String;")},
      {"(Ljava/lang/Class;)V", "Ljava/lang/String;",
       Nullable<std::string>("(Ljava/lang/String;Ljava/lang/Class;)V")},
      {"invalid)", "Ljava/lang/String;", Nullable<std::string>()},
  };

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(
        test_case.expected_descriptor,
        PrependExtraArgumentToDescriptor(
            test_case.descriptor,
            test_case.extra_argument));
  }
}

}  // namespace cdbg
}  // namespace devtools

