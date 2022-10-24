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

#include "gmock/gmock.h"

#include "expression_evaluator.h"
#include "expression_util.h"
#include "fake_jni.h"
#include "jni_utils.h"
#include "local_variable_reader.h"
#include "messages.h"
#include "mock_array_reader.h"
#include "mock_class_indexer.h"
#include "mock_jni_env.h"
#include "mock_jvmti_env.h"
#include "mock_method_caller.h"
#include "mock_readers_factory.h"
#include "model.h"
#include "model_util.h"

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::NotNull;
using testing::SetArgPointee;
using testing::WithArgs;

namespace devtools {
namespace cdbg {

class ExpressionEvaluatorTest : public ::testing::Test {
 protected:
  ExpressionEvaluatorTest()
      : fake_jni_(&jvmti_),
        global_jvm_(&jvmti_, fake_jni_.jni()) {
  }

  struct PositiveTestCase {
    std::string input;
    std::string expected_result;
  };

  void SetUp() override {
    readers_factory_.SetUpDefault();

    EXPECT_CALL(readers_factory_, FindClassByName(_, NotNull()))
        .WillRepeatedly(Invoke([this](const std::string& class_name,
                                      FormatMessageModel* error_message) {
          std::string class_signature;
          class_signature += 'L';
          for (char ch : class_name) {
            class_signature += ((ch == '.') ? '/' : ch);
          }
          class_signature += ';';

          JniLocalRef ref(fake_jni_.FindClassBySignature(class_signature));
          if (ref == nullptr) {
            *error_message = { InvalidIdentifier, { class_name } };
          }

          return ref;
        }));

    EXPECT_CALL(readers_factory_, IsAssignable(_, _))
        .WillRepeatedly(Return(false));

    EXPECT_CALL(readers_factory_,
                IsAssignable("Ljava/lang/String;", "Ljava/lang/Object;"))
        .WillRepeatedly(Return(true));

    EXPECT_CALL(readers_factory_, IsMethodCallAllowed(_))
        .WillRepeatedly(Invoke([](const ClassMetadataReader::Method& method) {
          const std::string& class_signature =
              method.class_signature.object_signature;
          return class_signature == "LSourceObj;" ||
              class_signature == "Lcom/myprod/MyClass;" ||
              class_signature == "Lcom/myprod/AnotherClass;";
        }));
  }

  void RunPositiveTestCases(const std::vector<PositiveTestCase>& test_cases) {
    for (const auto& test_case : test_cases) {
      // Compile the expression.
      CompiledExpression compiled_expression = CompileExpression(
          test_case.input,
          &readers_factory_);
      if (compiled_expression.evaluator == nullptr) {
        FAIL() << "Expression could not be compiled: " << test_case.input
               << std::endl
               << "Error message: " << compiled_expression.error_message;
        continue;
      }

      EXPECT_EQ(std::string(), compiled_expression.error_message.format);
      EXPECT_EQ(std::vector<std::string>(),
                compiled_expression.error_message.parameters);

      // Execute the expression evaluation.
      EvaluationContext evaluation_context;
      evaluation_context.method_caller = &method_caller_;

      ErrorOr<JVariant> result =
          compiled_expression.evaluator->Evaluate(evaluation_context);
      if (result.is_error()) {
        FAIL() << "Compiled expression could not be executed: "
               << test_case.input << std::endl
               << "Error message: " << result.error_message();
        continue;
      }

      EXPECT_EQ(std::vector<std::string>(), result.error_message().parameters);

      EXPECT_EQ(test_case.expected_result, result.value().ToString(false))
          << "Input: " << test_case.input;
    }
  }

  void RunEvaluationFailureTestCases(
      const std::vector<std::pair<std::string, FormatMessageModel>>&
          test_cases) {
    for (const auto& test_case : test_cases) {
      // Compile the expression.
      CompiledExpression compiled_expression = CompileExpression(
          test_case.first,
          &readers_factory_);
      if (compiled_expression.evaluator == nullptr) {
        FAIL() << "Expression could not be compiled: " << test_case.first
               << std::endl
               << "Error message: " << compiled_expression.error_message;
        continue;
      }

      EXPECT_EQ(std::string(), compiled_expression.error_message.format);
      EXPECT_EQ(std::vector<std::string>(),
                compiled_expression.error_message.parameters);

      // Try to execute the expression evaluation.
      EvaluationContext evaluation_context;
      evaluation_context.method_caller = &method_caller_;

      ErrorOr<JVariant> result =
          compiled_expression.evaluator->Evaluate(evaluation_context);
      EXPECT_TRUE(result.is_error())
          << "Compiled expression unexpectedly evaluated successfully: "
          << test_case.first;

      VerifyFormatMessage(test_case.second, result.error_message());
    }
  }

  void RunCompilationFailureTestCases(
      const std::vector<std::pair<std::string, FormatMessageModel>>&
          test_cases) {
    for (const auto& test_case : test_cases) {
      LOG(INFO) << "Negative compilation test case: " << test_case.first;

      EXPECT_NE("", test_case.second.format);

      // Compile the expression.
      CompiledExpression compiled_expression = CompileExpression(
          test_case.first,
          &readers_factory_);

      EXPECT_TRUE(compiled_expression.evaluator == nullptr);

      VerifyFormatMessage(test_case.second, compiled_expression.error_message);
    }
  }

  void VerifyFormatMessage(
      const FormatMessageModel& expected_error_message,
      const FormatMessageModel& actual_error_message) {
    EXPECT_EQ(expected_error_message.format, actual_error_message.format);

    EXPECT_EQ(
        expected_error_message.parameters.size(),
        actual_error_message.parameters.size());

    if (actual_error_message.format == INTERNAL_ERROR_MESSAGE.format) {
      EXPECT_EQ(2, actual_error_message.parameters.size());
      EXPECT_NE("", actual_error_message.parameters[0]);
      EXPECT_GT(std::stoi(actual_error_message.parameters[1]), 0);
    } else {
      EXPECT_EQ(
          expected_error_message.parameters,
          actual_error_message.parameters);
    }
  }

 protected:
  MockJvmtiEnv jvmti_;
  FakeJni fake_jni_;
  GlobalJvmEnv global_jvm_;
  MockReadersFactory readers_factory_;
  MockMethodCaller method_caller_;
};


TEST_F(ExpressionEvaluatorTest, ParserNegative) {
  RunCompilationFailureTestCases({
    { "2 +",  { ExpressionParserError } },
    { "7 <<< 8",  { ExpressionParserError } },
    { "7 * (8 - 3))",  { ExpressionParserError } }
  });
}


TEST_F(ExpressionEvaluatorTest, WalkNegative) {
  RunCompilationFailureTestCases({
    {
      "a.b.",  { ExpressionParserError }
    },
    {
      "0x111111111",
      { BadNumericLiteral, { "0x111111111" } }
    },
    {
      "3000000000",
      { BadNumericLiteral, { "3000000000" } }
    },
    {
      "0x11111111111111111L",
      { BadNumericLiteral, { "0x11111111111111111L" } }
    },
    {
      "0077777777777777777",
      { BadNumericLiteral, { "0077777777777777777" } }
    }
  });
}


TEST_F(ExpressionEvaluatorTest, LiteralsPositive) {
  RunPositiveTestCases({
    { "true", "<boolean>true" },
    { "false", "<boolean>false" },
    { "null", "null" },
    { "'A'", "<char>65" },
    { "382", "<int>382" },
    { "378629384723423L", "<long>378629384723423" },
    { "2.1f", "<float>2.1" },
    { "2.41", "<double>2.41" },
  });
}


TEST_F(ExpressionEvaluatorTest, StringsPositive) {
  RunPositiveTestCases({
      {"\"vlad\"", "<Object>"},
      {"\"\"", "<Object>"},
      {"\"" + std::string(2000, 'x') + "\"", "<Object>"},
  });
}


TEST_F(ExpressionEvaluatorTest, ArithmeticNumericPromotionDouble) {
  readers_factory_.AddFakeLocal<jbyte>("myByte", -112);
  readers_factory_.AddFakeLocal<jshort>("myShort", 27491);

  RunPositiveTestCases({
    { "myByte-1.11", "<double>-113.11" },
    { "myShort+1.2", "<double>27492.2" },
    { "113.1+myByte", "<double>1.1" },
    { "1.2+myShort", "<double>27492.2" },
    { "'A'+3.4", "<double>68.4" },
    { "3.4+'A'", "<double>68.4" },
    { "71+4.1", "<double>75.1" },
    { "4.1+71", "<double>75.1" },
    { "111111111111111L+4.1", "<double>1.111111111e+14" },
    { "4.1+111111111111111L", "<double>1.111111111e+14" },
    { "4.1f+3.4", "<double>7.499999905" },
    { "3.4+4.1f", "<double>7.499999905" },
    { "11.72+3.4", "<double>15.12" },
  });
}


TEST_F(ExpressionEvaluatorTest, ArithmeticNumericPromotionFloat) {
  readers_factory_.AddFakeLocal<jbyte>("myByte", -112);
  readers_factory_.AddFakeLocal<jshort>("myShort", 27491);

  RunPositiveTestCases({
    { "myByte-1.11f", "<float>-113.11" },
    { "myShort+1.2f", "<float>27492.2" },
    { "113.1f+myByte", "<float>1.1" },
    { "1.2f+myShort", "<float>27492.2" },
    { "'A'+3.4f", "<float>68.4" },
    { "3.4f+'A'", "<float>68.4" },
    { "71+4.1f", "<float>75.1" },
    { "4.1f+71", "<float>75.1" },
    { "123L+4.1f", "<float>127.1" },
    { "4.1f+123L", "<float>127.1" },
    { "11.72f+3.4f", "<float>15.12" },
  });
}


TEST_F(ExpressionEvaluatorTest, ArithmeticNumericPromotionLong) {
  readers_factory_.AddFakeLocal<jbyte>("myByte", -112);
  readers_factory_.AddFakeLocal<jshort>("myShort", 27491);

  RunPositiveTestCases({
    { "myByte-1L", "<long>-113" },
    { "myShort+2L", "<long>27493" },
    { "113L+myByte", "<long>1" },
    { "2L+myShort", "<long>27493" },
    { "'A'+1L", "<long>66" },
    { "1L+'A'", "<long>66" },
    { "1+32L", "<long>33" },
    { "32L+1", "<long>33" },
    { "123L+456L", "<long>579" },
  });
}


TEST_F(ExpressionEvaluatorTest, ArithmeticNumericPromotionInt) {
  readers_factory_.AddFakeLocal<jbyte>("myByte", -112);
  readers_factory_.AddFakeLocal<jshort>("myShort", 27491);

  RunPositiveTestCases({
    { "myByte-1", "<int>-113" },
    { "myShort+2", "<int>27493" },
    { "113+myByte", "<int>1" },
    { "2+myShort", "<int>27493" },
    { "'A'+1", "<int>66" },
    { "1+'A'", "<int>66" },
    { "1+32", "<int>33" },
    { "32+1", "<int>33" },
  });
}


TEST_F(ExpressionEvaluatorTest, ArithmeticInvalidNumericPromotion) {
  readers_factory_.AddFakeLocal<jbyte>("myByte", -112);
  readers_factory_.AddFakeLocal<jshort>("myShort", 27491);

  RunCompilationFailureTestCases({
    { "true+myByte", { TypeMismatch } },
    { "myByte+true", { TypeMismatch } },
    { "true+myShort", { TypeMismatch } },
    { "myShort+true", { TypeMismatch } },
    { "true+1.5", { TypeMismatch } },
    { "1.5+true", { TypeMismatch } },
    { "true+1.5f", { TypeMismatch } },
    { "1.5f+true", { TypeMismatch } },
    { "true+4l", { TypeMismatch } },
    { "4l+true", { TypeMismatch } },
    { "true+7", { TypeMismatch } },
    { "7+true", { TypeMismatch } },
    { "null+1.5", { TypeMismatch } },
    { "1.5+null", { TypeMismatch } },
    { "null+1.5f", { TypeMismatch } },
    { "1.5f+null", { TypeMismatch } },
    { "null+4l", { TypeMismatch } },
    { "4l+null", { TypeMismatch } },
    { "null+7", { TypeMismatch } },
    { "7+null", { TypeMismatch } }
  });
}


TEST_F(ExpressionEvaluatorTest, ArithmeticComputerDouble) {
  RunPositiveTestCases({
    { "4.1 + 3.4", "<double>7.5" },
    { "4.1 - 13.4", "<double>-9.3" },
    { "4.1 * 13.4", "<double>54.94" },
    { "4.1 / 13.4", "<double>0.3059701493" },
    { "13.4 % 4.1", "<double>1.1" },
  });
}


TEST_F(ExpressionEvaluatorTest, ArithmeticComputerFloat) {
  RunPositiveTestCases({
    { "4.1f + 3.4f", "<float>7.5" },
    { "4.1f - 13.4f", "<float>-9.3" },
    { "4.1f * 13.4f", "<float>54.94" },
    { "4.1f / 13.4f", "<float>0.30597" },
    { "13.4f % 4.1f", "<float>1.1" },
  });
}


TEST_F(ExpressionEvaluatorTest, ArithmeticComputerLong) {
  RunPositiveTestCases({
    { "4L + 3L", "<long>7" },
    { "4L - 13L", "<long>-9" },
    { "4L * 13L", "<long>52" },
    { "13L / 4L", "<long>3" },
    { "13L % 4L", "<long>1" },
    { "(-9223372036854775807L - 1) * -1", "<long>-9223372036854775808" },
  });
}


TEST_F(ExpressionEvaluatorTest, ArithmeticComputerInt) {
  RunPositiveTestCases({
    { "4 + 3", "<int>7" },
    { "4 - 13", "<int>-9" },
    { "4 * 13", "<int>52" },
    { "13 / 4", "<int>3" },
    { "13 % 4", "<int>1" },
    { "(-2147483647 - 1) * -1", "<int>-2147483648" },
  });
}


TEST_F(ExpressionEvaluatorTest, IntegerOverflow) {
  RunEvaluationFailureTestCases({
    { "1 / 0", { DivisionByZero } },
    { "1L / 0", { DivisionByZero } },
    { "1 / 0L", { DivisionByZero } },
    { "1L / 0L", { DivisionByZero } },
    { "1 % 0", { DivisionByZero } },
    { "1L % 0", { DivisionByZero } },
    { "1 % 0L", { DivisionByZero } },
    { "1L % 0L", { DivisionByZero } },
    { "(-2147483647 - 1) / -1", { IntegerDivisionOverflow } },
    { "(-9223372036854775807L - 1) / -1", { IntegerDivisionOverflow } },
    { "(-9223372036854775807L - 1) / -1L", { IntegerDivisionOverflow } },
    { "(-2147483647 - 1) % -1", { IntegerDivisionOverflow } },
    { "(-9223372036854775807L - 1) % -1", { IntegerDivisionOverflow } },
    { "(-9223372036854775807L - 1) % -1L", { IntegerDivisionOverflow } },
  });
}


TEST_F(ExpressionEvaluatorTest, TypeCastNumericFailureOperation) {
  RunCompilationFailureTestCases({
    { "(boolean)1", { TypeCastCompileInvalid , { "boolean", "int" } } },
    { "(long)true", { TypeCastCompileInvalid , { "long", "boolean" } } },
    { "(int)\"abc\"",
        { TypeCastUnsupported , { "java.lang.String", "int" } } },
    { "(java.lang.Boolean)true",
        { TypeCastUnsupported , { "boolean", "java.lang.Boolean" } } },
    { "(java.lang.String)1",
        { TypeCastUnsupported , { "int", "java.lang.String" } } },
    { "(int)true",
        { TypeCastCompileInvalid , { "int", "boolean" } } },
    { "(java.lang.Boolean)1",
        { TypeCastUnsupported , { "int", "java.lang.Boolean" } } },
    { "(java.lang.Integer)1",
        { TypeCastUnsupported , { "int", "java.lang.Integer" } } },
    { "(java.lang.Float)1",
        { TypeCastUnsupported , { "int", "java.lang.Float" } } },
    { "(java.lang.Double)1.0",
        { TypeCastUnsupported , { "double", "java.lang.Double" } } },
    { "(java.lang.String)1.5",
        { TypeCastUnsupported , { "double", "java.lang.String" } } },
    { "(java.lang.Float)1.5",
        { TypeCastUnsupported , { "double", "java.lang.Float" } } },
    { "(java.lang.Long)21474836506L",
        { TypeCastUnsupported , { "long", "java.lang.Long" } } },
    { "(java.lang.Short)21474836506L",
        { TypeCastUnsupported , { "long", "java.lang.Short" } } },
    { "(java.lang.Char)\'a\'",
        { TypeCastUnsupported , { "char", "java.lang.Char" } } },
  });
}


TEST_F(ExpressionEvaluatorTest, TypeCastNumericValidOperation) {
  RunPositiveTestCases({
    { "(long)1", "<long>1" },
    // The number gets truncated to int.
    { "(int)1.2", "<int>1" },
    // jchar is unsigned char, the least 16 bits remain.
    { "(char)1004566", "<char>21526" },
    { "(short)1.2", "<short>1" },
    { "(double)1", "<double>1" },
    { "(int)\'a\'", "<int>97" },
    { "(double)\'a\'", "<double>97" },
    { "(long)\'a\'", "<long>97" },
    { "(boolean)true", "<boolean>true" },
    { "(int)1111111111111111L", "<int>-1223331385" },
    { "(float)100", "<float>100" },
    { "(byte)1111111111111111L", "<byte>-57" },
    { "(short)1111111111111111L", "<short>29127" },
    { "(int)1111111111111111L", "<int>-1223331385" },
    { "(long)1111111111111111L", "<long>1111111111111111" },
    { "(float)1111111111111111L", "<float>1.11111e+15" },
    { "(double)1111111111111111L", "<double>1.111111111e+15" },
    { "(short)3.2f", "<short>3" },
    { "(short)5.6d", "<short>5" },
    { "(byte)5.6d", "<byte>5" },
  });
}


TEST_F(ExpressionEvaluatorTest, DoubleTypeCastNumericValidOperation) {
  RunPositiveTestCases({
    { "(int)(short)5.6d", "<int>5" },
    { "(int)(short)5.6d", "<int>5" },
    { "(long)(int)1111111111111111L", "<long>-1223331385" },
    { "(byte)(long)\'a\'", "<byte>97" },
    { "(float)(long)123456", "<float>123456" },
    { "(char)(long)123456d", "<char>57920" },
  });
}


TEST_F(ExpressionEvaluatorTest, DoubleTypeCastNumericFailureOperation) {
  RunCompilationFailureTestCases({
    {
      "(long)(boolean)1",
      { TypeCastCompileInvalid , { "boolean", "int" } }
    },
    {
      "(boolean)(long)1.2",
      { TypeCastCompileInvalid , { "boolean", "long" } }
    },
    {
      "(boolean)(char)1",
      { TypeCastCompileInvalid , { "boolean", "char" } }
    },
    {
      "(Boolean)(byte)1",
      { TypeCastUnsupported , { "byte", "Boolean" } }
    },
    {
      "(Java.lang.Long)(byte)1",
      { TypeCastUnsupported , { "byte", "Java.lang.Long" } }
    },
    {
      "(Java.lang.Double)(short)1",
      { TypeCastUnsupported , { "short", "Java.lang.Double" } }
    },
    {
      "(Java.lang.Float)(char)1",
      { TypeCastUnsupported , { "char", "Java.lang.Float" } }
    },
    {
      "(Boolean)(byte)1",
      { TypeCastUnsupported , { "byte", "Boolean" } }
    },
  });
}


TEST_F(ExpressionEvaluatorTest, TypeCastObjectInvalidOperation) {
  // FakeJni returns false for IsInstanceOf call when source is not null.
  RunEvaluationFailureTestCases({
    {
      "(com.prod.MyClass1) \"myobject1\"",
      {
        TypeCastEvaluateInvalid,
        { "java.lang.String", "com.prod.MyClass1" }
      }
    },
  });
}


TEST_F(ExpressionEvaluatorTest, TypeCastObjectDeferredClass) {
  RunCompilationFailureTestCases({
    {
      "(com.unknown.WhatIsThis)null",
      { InvalidIdentifier , { "com.unknown.WhatIsThis" } }
    },
  });
}


TEST_F(ExpressionEvaluatorTest, TypeCastObjectValidOperation) {
  // FakeJni returns true for IsInstanceOf when source is null.
  RunPositiveTestCases({
    { "(java.lang.String)null", "null" },
  });
}

TEST_F(ExpressionEvaluatorTest, InstanceofFailureOperation) {
  JVariant my_obj = JVariant::LocalRef(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));
  readers_factory_.AddFakeLocal("myObj", "LSourceObj;", my_obj);

  JVariant int_arr = JVariant::LocalRef(
      fake_jni_.CreateNewObject(FakeJni::StockClass::IntArray));
  readers_factory_.AddFakeLocal("intArr", "[I", int_arr);

  JVariant str_arr = JVariant::LocalRef(
      fake_jni_.CreateNewObject(FakeJni::StockClass::StringArray));
  readers_factory_.AddFakeLocal("strArr", "[Ljava/lang/String;", str_arr);

  RunCompilationFailureTestCases({
      {"myObj instanceof int", {InvalidIdentifier, {"int"}}},
      {"myObj instanceof null", {ExpressionParserError}},
      {"myObj instanceof com.unknown.Class",
       {InvalidIdentifier, {"com.unknown.Class"}}},
      {"123 instanceof com.prod.MyClass1", {ReferenceTypeNotFound, {"int"}}},

      {"int_arr instanceof int[]", {ExpressionParserError}},
      {"str_arr instanceof String[]", {ExpressionParserError}},
  });
}

TEST_F(ExpressionEvaluatorTest, InstanceofValidOperation) {
  JVariant my_obj = JVariant::LocalRef(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));
  readers_factory_.AddFakeLocal("myObj", "LSourceObj;", my_obj);

  RunPositiveTestCases({
      {"null instanceof java.lang.String", "<boolean>true"},
      {"myObj instanceof com.prod.MyClass1", "<boolean>true"},
      {"myObj instanceof com.prod.MyClass2", "<boolean>false"},
      {"\"123\" instanceof java.lang.String", "<boolean>true"},
  });

  // More complicated compound expressions.
  JVariant other_obj = JVariant::LocalRef(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass2));
  readers_factory_.AddFakeInstanceField("LSourceObj;", "fieldOtherObj",
                                        "LOtherObj;", other_obj);

  RunPositiveTestCases({
      {"myObj.fieldOtherObj instanceof com.prod.MyClass2", "<boolean>true"},
      {"myObj instanceof com.prod.MyClass1 && true", "<boolean>true"},
  });
}

TEST_F(ExpressionEvaluatorTest, ArrayAccessPositive) {
  // Mock access to integer array returning "-i" for "a[i]"
  EXPECT_CALL(readers_factory_, CreateArrayReader(_))
      .WillRepeatedly(InvokeWithoutArgs([this] () {
        std::unique_ptr<MockArrayReader> reader(new MockArrayReader);
        EXPECT_CALL(*reader, ReadValue(_, _))
            .WillRepeatedly(WithArgs<1>(Invoke([this] (
                const JVariant& index) {
              jlong index_value = 0;
              EXPECT_TRUE(index.get<jlong>(&index_value));

              return JVariant::Int(-index_value);
            })));

        return reader;
      }));

  JVariant jobj = JVariant::LocalRef(
      fake_jni_.CreateNewObject(FakeJni::StockClass::IntArray));
  readers_factory_.AddFakeLocal("myarr", "[I", jobj);

  RunPositiveTestCases({
    { "myarr[181]", "<int>-181" },
    { "myarr[myarr == null ? 3 : 8]", "<int>-8" },
    { "(myarr[1] + myarr[2]) * myarr[3] - myarr[4]", "<int>13" },
    { "myarr['A']", "<int>-65" },
  });
}


TEST_F(ExpressionEvaluatorTest, ArrayAccessEvaluationError) {
  EXPECT_CALL(readers_factory_, CreateArrayReader(_))
      .WillRepeatedly(InvokeWithoutArgs([] () {
        std::unique_ptr<MockArrayReader> reader(new MockArrayReader);
        EXPECT_CALL(*reader, ReadValue(_, _))
            .WillRepeatedly(InvokeWithoutArgs([] {
              return FormatMessageModel { "something bad failed" };
            }));

        return reader;
      }));

  JVariant jobj = JVariant::LocalRef(
      fake_jni_.CreateNewObject(FakeJni::StockClass::IntArray));
  readers_factory_.AddFakeLocal("myarr", "[I", jobj);

  RunEvaluationFailureTestCases({
    { "myarr[1]", { "something bad failed" } }
  });
}


TEST_F(ExpressionEvaluatorTest, ArrayAccessCompilationFailure) {
  EXPECT_CALL(readers_factory_, CreateArrayReader(_))
      .WillRepeatedly(InvokeWithoutArgs([] () {
        std::unique_ptr<MockArrayReader> reader(new MockArrayReader);
        EXPECT_CALL(*reader, ReadValue(_, _))
            .Times(0);

        return reader;
      }));

  JVariant jstr_platypus = JVariant::LocalRef(
      fake_jni_.CreateNewJavaString("platypus"));
  readers_factory_.AddFakeLocal("pl", kJavaStringClassSignature, jstr_platypus);

  JVariant jobj = JVariant::LocalRef(
      fake_jni_.CreateNewObject(FakeJni::StockClass::IntArray));
  readers_factory_.AddFakeLocal("myarr", "[I", jobj);

  RunCompilationFailureTestCases({
    { "pl[1]", { ArrayTypeExpected, { "java.lang.String" } } },
    { "myarr[\"aa\"]", { ArrayIndexNotInteger, { "java.lang.String" } } },
    { "myarr[what]", { InvalidIdentifier, { "what" } } },
    { "where[1]", { InvalidIdentifier, { "where" } } },
  });
}


TEST_F(ExpressionEvaluatorTest, ArrayReaderNotAvailable) {
  EXPECT_CALL(readers_factory_, CreateArrayReader(_))
      .WillRepeatedly(InvokeWithoutArgs([] () {
        return nullptr;
      }));

  JVariant jobj = JVariant::LocalRef(
      fake_jni_.CreateNewObject(FakeJni::StockClass::IntArray));
  readers_factory_.AddFakeLocal("myarr", "[I", jobj);

  RunCompilationFailureTestCases({
    { "myarr[1]", INTERNAL_ERROR_MESSAGE },
  });
}


TEST_F(ExpressionEvaluatorTest, ConditionalObjectComputer) {
  RunPositiveTestCases({
    { "null == null", "<boolean>true" },
    { "null != null", "<boolean>false" },
  });
}


TEST_F(ExpressionEvaluatorTest, ConditionalObjectInvalidOperation) {
  RunCompilationFailureTestCases({
    { "null && null", { TypeMismatch } },
    { "null || null", { TypeMismatch } },
    { "null <= null", { TypeMismatch } },
    { "null >= null", { TypeMismatch } },
    { "null < null", { TypeMismatch } },
    { "null > null", { TypeMismatch } },
    { "null && true", { TypeMismatch } },
    { "null || false", { TypeMismatch } },
    { "null == 1", { TypeMismatch } },
    { "null == false", { TypeMismatch } },
    { "null != 1", { TypeMismatch } },
    { "null != false", { TypeMismatch } },
    { "null <= 1", { TypeMismatch } },
    { "null >= 1", { TypeMismatch } },
    { "null < 1", { TypeMismatch } },
    { "null > 1", { TypeMismatch } }
  });
}


TEST_F(ExpressionEvaluatorTest, ConditionalStringComputer) {
  readers_factory_.AddFakeLocal(
      "nullString1",
      kJavaStringClassSignature,
      nullptr);

  readers_factory_.AddFakeLocal(
      "nullString2",
      kJavaStringClassSignature,
      nullptr);

  JVariant jstr_platypus = JVariant::LocalRef(
      fake_jni_.CreateNewJavaString("platypus"));
  readers_factory_.AddFakeLocal("pl", kJavaStringClassSignature, jstr_platypus);

  RunPositiveTestCases({
    { "\"vlad\" == \"vlad\"", "<boolean>true" },
    { "\"vladL\" == \"vlad\"", "<boolean>false" },
    { "\"vlad\" == \"vladL\"", "<boolean>false" },
    { "\"vlad\" != \"vlad\"", "<boolean>false" },
    { "pl == \"vlad\"", "<boolean>false" },
    { "pl == \"platypus\"", "<boolean>true" },
    { "pl != \"vlad\"", "<boolean>true" },
    { "nullString1 == nullString2", "<boolean>true" },
    { "nullString1 != nullString2", "<boolean>false" },
    { "pl == nullString1", "<boolean>false" },
    { "nullString2 != pl", "<boolean>true" },
    { "\"vlad\" != nullString1", "<boolean>true" },
    { "nullString2 == \"vlad\"", "<boolean>false" },
    { "\"\" == \"\"", "<boolean>true" }
  });
}


TEST_F(ExpressionEvaluatorTest, ConditionalStringInvalidOperation) {
  RunCompilationFailureTestCases({
    { "\"vlad\" + 1", { TypeMismatch } },
    { "\"vlad\" || true", { TypeMismatch } },
    { "\"vlad\" ? 1 : 2", { TypeMismatch } }
  });
}


TEST_F(ExpressionEvaluatorTest, ConditionalBooleanValid) {
  RunPositiveTestCases({
      {"true && false", "<boolean>false"},
      {"true & false", "<boolean>false"},
      {"true || false", "<boolean>true"},
      {"true | false", "<boolean>true"},
      {"true ^ false", "<boolean>true"},
      {"false ^ false", "<boolean>false"},
      {"true == false", "<boolean>false"},
      {"true != false", "<boolean>true"},
      // short-circuit "&&" and "||": the expressions on the right side are
      // designed to fail, so they should never be evaluated.
      {"false && ((1 / 0) == 1)", "<boolean>false"},
      {"true || ((1 / 0) == 1)", "<boolean>true"},
  });
}


TEST_F(ExpressionEvaluatorTest, ConditionalBooleanInvalid) {
  RunCompilationFailureTestCases({
    { "true <= false", { TypeMismatch } },
    { "true >= false", { TypeMismatch } },
    { "true < false", { TypeMismatch } },
    { "true > false", { TypeMismatch } },
    { "true && 1", { TypeMismatch } },
    { "1 && true", { TypeMismatch } },
    { "true & 1", { TypeMismatch } },
    { "1 & true", { TypeMismatch } },
    { "true || 1", { TypeMismatch } },
    { "1 || true", { TypeMismatch } },
    { "true | 1", { TypeMismatch } },
    { "1 | true", { TypeMismatch } },
    { "true ^ 1", { TypeMismatch } },
    { "1 ^ true", { TypeMismatch } },
    { "true == 1", { TypeMismatch } },
    { "1 == true", { TypeMismatch } },
    { "true != 1", { TypeMismatch } },
    { "1 != true", { TypeMismatch } },
    { "true <= 1", { TypeMismatch } },
    { "1 <= true", { TypeMismatch } },
    { "true >= 1", { TypeMismatch } },
    { "1 >= true", { TypeMismatch } },
    { "true < 1", { TypeMismatch } },
    { "1 < true", { TypeMismatch } },
    { "true > 1", { TypeMismatch } },
    { "1 > true", { TypeMismatch } }
  });
}


TEST_F(ExpressionEvaluatorTest, ConditionalNumericValid) {
  readers_factory_.AddFakeLocal<jbyte>("byte65", 65);
  readers_factory_.AddFakeLocal<jshort>("short65", 65);
  readers_factory_.AddFakeLocal<jbyte>("byte66", 66);
  readers_factory_.AddFakeLocal<jshort>("short66", 66);

  const std::string small_literals[] = {"byte65", "short65",
                                        "'A'",  // = 65
                                        "65",     "65l",     "65.0f", "65.0"};

  const std::string big_literals[] = {"byte66", "short66",
                                      "'B'",  // = 66
                                      "66",     "66l",     "66.0f", "66.0"};

  for (const std::string& small_literal : small_literals) {
    for (const std::string& big_literal : big_literals) {
      RunPositiveTestCases({
        { small_literal + " == " + big_literal, "<boolean>false" },
        { big_literal + " == " + small_literal, "<boolean>false" },
        { small_literal + " == " + small_literal, "<boolean>true" },
        { big_literal + " == " + big_literal, "<boolean>true" },

        { small_literal + " != " + big_literal, "<boolean>true" },
        { big_literal + " != " + small_literal, "<boolean>true" },
        { small_literal + " != " + small_literal, "<boolean>false" },
        { big_literal + " != " + big_literal, "<boolean>false" },

        { small_literal + " <= " + big_literal, "<boolean>true" },
        { big_literal + " <= " + small_literal, "<boolean>false" },
        { small_literal + " <= " + small_literal, "<boolean>true" },
        { big_literal + " <= " + big_literal, "<boolean>true" },

        { small_literal + " >= " + big_literal, "<boolean>false" },
        { big_literal + " >= " + small_literal, "<boolean>true" },
        { small_literal + " >= " + small_literal, "<boolean>true" },
        { big_literal + " >= " + big_literal, "<boolean>true" },

        { small_literal + " < " + big_literal, "<boolean>true" },
        { big_literal + " < " + small_literal, "<boolean>false" },
        { small_literal + " < " + small_literal, "<boolean>false" },
        { big_literal + " < " + big_literal, "<boolean>false" },

        { small_literal + " > " + big_literal, "<boolean>false" },
        { big_literal + " > " + small_literal, "<boolean>true" },
        { small_literal + " > " + small_literal, "<boolean>false" },
        { big_literal + " > " + big_literal, "<boolean>false" },
      });
    }
  }
}


TEST_F(ExpressionEvaluatorTest, ConditionalNumericInvalid) {
  RunCompilationFailureTestCases({
    { "'A' && 'B'", { TypeMismatch } },
    { "1 && 2", { TypeMismatch } },
    { "1l && 2l", { TypeMismatch } },
    { "1.4f && 2.2f", { TypeMismatch } },
    { "1.4 && 2.2", { TypeMismatch } },
    { "1 || 2", { TypeMismatch } },
    { "1l || 2l", { TypeMismatch } },
    { "1.4f || 2.2f", { TypeMismatch } },
    { "1.4 || 2.2", { TypeMismatch } }
  });
}


TEST_F(ExpressionEvaluatorTest, BitwiseNumericPromotions) {
  readers_factory_.AddFakeLocal<jbyte>("byte1", 1);
  readers_factory_.AddFakeLocal<jbyte>("byte3", 3);
  readers_factory_.AddFakeLocal<jshort>("short1", 1);
  readers_factory_.AddFakeLocal<jshort>("short3", 3);

  RunPositiveTestCases({
    { "byte3 & byte1", "<int>1" },
    { "short3 & byte1", "<int>1" },
    { "'A' & byte1", "<int>1" },
    { "3 & byte1", "<int>1" },
    { "3l & byte1", "<long>1" },

    { "byte3 & short1", "<int>1" },
    { "short3 & short1", "<int>1" },
    { "'A' & short1", "<int>1" },
    { "3 & short1", "<int>1" },
    { "3l & short1", "<long>1" },

    { "byte3 & 1", "<int>1" },
    { "short3 & 1", "<int>1" },
    { "'A' & 1", "<int>1" },
    { "3 & 1", "<int>1" },

    { "byte3 & 1L", "<long>1" },
    { "short3 & 1L", "<long>1" },
    { "'A' & 1l", "<long>1" },
    { "3 & 1l", "<long>1" },
    { "3l & 1l", "<long>1" },
  });
}


TEST_F(ExpressionEvaluatorTest, BitwiseValid) {
  // Bitwise operations on booleans are covered in Conditional_Boolean_Valid.

  RunPositiveTestCases({
    { "7 & 3", "<int>3" },
    { "7 | 3", "<int>7" },
    { "7 ^ 3", "<int>4" },

    { "7l & 3l", "<long>3" },
    { "7l | 3l", "<long>7" },
    { "7l ^ 3l", "<long>4" },
  });
}


TEST_F(ExpressionEvaluatorTest, BitwiseInvalid) {
  // Invalid bitwise operations on booleans are covered in
  // ConditionalBooleanInvalid.

  RunCompilationFailureTestCases({
    { "1.0f & 1", { TypeMismatch } },
    { "1 & 1.0f", { TypeMismatch } },
    { "1.0 & 1", { TypeMismatch } },
    { "1 & 1.0", { TypeMismatch } },
    { "1.0f | 1", { TypeMismatch } },
    { "1 | 1.0f", { TypeMismatch } },
    { "1.0 | 1", { TypeMismatch } },
    { "1 | 1.0", { TypeMismatch } },
    { "1.0f ^ 1", { TypeMismatch } },
    { "1 ^ 1.0f", { TypeMismatch } },
    { "1.0 ^ 1", { TypeMismatch } },
    { "1 ^ 1.0", { TypeMismatch } },

    { "null & 1", { TypeMismatch } },
    { "null & true", { TypeMismatch } },
    { "1 & null", { TypeMismatch } },
    { "true & null", { TypeMismatch } },
    { "null | 1", { TypeMismatch } },
    { "null | true", { TypeMismatch } },
    { "1 | null", { TypeMismatch } },
    { "true | null", { TypeMismatch } },
    { "null ^ 1", { TypeMismatch } },
    { "null ^ true", { TypeMismatch } },
    { "1 ^ null", { TypeMismatch } },
    { "true ^ null", { TypeMismatch } }
  });
}


TEST_F(ExpressionEvaluatorTest, ShiftNumericPromotion) {
  readers_factory_.AddFakeLocal<jbyte>("byte1", 1);
  readers_factory_.AddFakeLocal<jshort>("short1", 1);

  RunPositiveTestCases({
    { "byte1 << 1", "<int>2" },
    { "1 << byte1", "<int>2" },

    { "short1 << 1", "<int>2" },
    { "1 << short1", "<int>2" },

    { "byte1 << short1", "<int>2" },

    { "'A' << 1", "<int>130" },
    { "1 << 1", "<int>2" },
    { "1 << 'A'", "<int>2" },
    { "'A' << 1l", "<int>130" },

    { "1l << 1", "<long>2" },
    { "1l << 1l", "<long>2" },
    { "1l << 'A'", "<long>2" },
  });
}


TEST_F(ExpressionEvaluatorTest, ShiftValid) {
  const std::string arg2_suffixes[] = {"", "l"};
  for (const std::string& arg2_suffix : arg2_suffixes) {
    RunPositiveTestCases({
      { "3 << 2" + arg2_suffix, "<int>12" },
      { "3 >> 1" + arg2_suffix, "<int>1" },
      { "3 << 31"  + arg2_suffix + " >>> 31" + arg2_suffix, "<int>1" },
      { "3 << 31" + arg2_suffix + " >> 31" + arg2_suffix, "<int>-1" },
      { "1 << 34" + arg2_suffix, "<int>4" },

      { "3l << 2" + arg2_suffix, "<long>12" },
      { "3l >> 1" + arg2_suffix, "<long>1" },
      { "3l << 31" + arg2_suffix + " >>> 31" + arg2_suffix, "<long>3" },
      { "3l << 31"  + arg2_suffix + " >> 31" + arg2_suffix, "<long>3" },
      { "3l << 63" + arg2_suffix + " >>> 63" + arg2_suffix, "<long>1" },
      { "3l << 63"  + arg2_suffix + " >> 63" + arg2_suffix, "<long>-1" },
      { "1l << 34" + arg2_suffix, "<long>17179869184" },
      { "1l << 66" + arg2_suffix, "<long>4" },
    });
  }
}


TEST_F(ExpressionEvaluatorTest, ShiftInvalid) {
  const std::string shift_operators[] = {"<<", ">>", ">>>"};
  for (const std::string& shift_operator : shift_operators) {
    RunCompilationFailureTestCases({
      { "1.0f " + shift_operator + " 1", { TypeMismatch } },
      { "1 " + shift_operator + " 1.0f", { TypeMismatch } },
      { "1.0 " + shift_operator + " 1", { TypeMismatch } },
      { "1 " + shift_operator + " 1.0", { TypeMismatch } },
      { "true " + shift_operator + " 1", { TypeMismatch } },
      { "1 " + shift_operator + " true", { TypeMismatch } },
      { "null " + shift_operator + " 1", { TypeMismatch } },
      { "1 " + shift_operator + " null", { TypeMismatch } }
    });
  }
}


TEST_F(ExpressionEvaluatorTest, BooleanConditionalOperator) {
  RunPositiveTestCases({
    { "true ? true : false", "<boolean>true" },
    { "true ? false : true", "<boolean>false" },
    { "false ? false : true", "<boolean>true" },
  });
}


TEST_F(ExpressionEvaluatorTest, NumericConditionalOperator) {
  readers_factory_.AddFakeLocal<jbyte>("byte1", 1);
  readers_factory_.AddFakeLocal<jbyte>("byte2", 2);
  readers_factory_.AddFakeLocal<jshort>("short1", 1);
  readers_factory_.AddFakeLocal<jshort>("short2", 2);

  const struct {
    std::string expression1;
    std::string expression2;
    std::string result_if_1;
    std::string result_if_2;
  } test_cases[] = {
    { "byte1", "byte2", "<int>1", "<int>2" },
    { "short1", "short2", "<int>1", "<int>2" },
    { "short1", "byte2", "<int>1", "<int>2" },
    { "byte1", "short2", "<int>1", "<int>2" },
    { "1", "byte2", "<int>1", "<int>2" },
    { "byte1", "2", "<int>1", "<int>2" },
    { "1L", "byte2", "<long>1", "<long>2" },
    { "byte1", "2L", "<long>1", "<long>2" },
    { "1", "short2", "<int>1", "<int>2" },
    { "short1", "2", "<int>1", "<int>2" },
    { "1L", "short2", "<long>1", "<long>2" },
    { "short1", "2L", "<long>1", "<long>2" },
    { "1", "2", "<int>1", "<int>2" },
    { "1l", "2", "<long>1", "<long>2" },
    { "1l", "2l", "<long>1", "<long>2" },
    { "1.1f", "byte2", "<float>1.1", "<float>2" },
    { "1.1f", "short2", "<float>1.1", "<float>2" },
    { "1.1f", "2", "<float>1.1", "<float>2" },
    { "1.1f", "23l", "<float>1.1", "<float>23" },
    { "1.1f", "2.2f", "<float>1.1", "<float>2.2" },
    { "1.1", "byte2", "<double>1.1", "<double>2" },
    { "1.1", "short2", "<double>1.1", "<double>2" },
    { "1.1", "2.2f", "<double>1.1", "<double>2.200000048" },
    { "1.1", "2.2", "<double>1.1", "<double>2.2" }
  };

  for (const auto& test_case : test_cases) {
    RunPositiveTestCases({
      {
        "true ? " + test_case.expression1 + " : " + test_case.expression2,
        test_case.result_if_1
      },
      {
        "false ? " + test_case.expression1 + " : " + test_case.expression2,
        test_case.result_if_2
      },
      {
        "true ? " + test_case.expression2 + " : " + test_case.expression1,
        test_case.result_if_2
      },
      {
        "false ? " + test_case.expression2 + " : " + test_case.expression1,
        test_case.result_if_1
      }
    });
  }
}


TEST_F(ExpressionEvaluatorTest, ReferenceConditionalOperator) {
  RunPositiveTestCases({
    { "true ? null : null", "null" }
  });
}


TEST_F(ExpressionEvaluatorTest, ConditionalOperatorInvalid) {
  const struct {
    std::string condition;
    std::string expression1;
    std::string expression2;
    FormatMessageModel expected_error_message;
  } test_cases[] = {
    { "1", "true", "false", { TypeMismatch }  },
    { "1l", "true", "false", { TypeMismatch }  },
    { "1.0f", "true", "false", { TypeMismatch }  },
    { "1.2", "true", "false", { TypeMismatch }  },
    { "null", "true", "false", { TypeMismatch } },

    { "true", "1", "true", { TypeMismatch } },
    { "true", "1l", "true", { TypeMismatch } },
    { "true", "1.1f", "true", { TypeMismatch } },
    { "true", "1.2", "true", { TypeMismatch } },

    { "true", "null", "true", { TypeMismatch } },
    { "true", "null", "1", { TypeMismatch } },
    { "true", "null", "1l", { TypeMismatch } },
    { "true", "null", "1.1f", { TypeMismatch } },
    { "true", "null", "2.2", { TypeMismatch } },
  };

  for (const auto& test_case : test_cases) {
    RunCompilationFailureTestCases({
      {
        test_case.condition +
        " ? " +
        test_case.expression1 +
        " : " +
        test_case.expression2,
        test_case.expected_error_message
      },
      {
        test_case.condition +
        " ? " +
        test_case.expression2 +
        " : " +
        test_case.expression1,
        test_case.expected_error_message
      }
    });
  }
}


TEST_F(ExpressionEvaluatorTest, UnaryExpressions) {
  readers_factory_.AddFakeLocal<jbyte>("byte1", 1);
  readers_factory_.AddFakeLocal<jshort>("short1", 1);

  RunPositiveTestCases({
    { "+byte1", "<int>1" },
    { "+short1", "<int>1" },
    { "+1", "<int>1" },
    { "+1l", "<long>1" },
    { "+1.1f", "<float>1.1" },
    { "+1.1", "<double>1.1" },

    { "-byte1", "<int>-1" },
    { "-short1", "<int>-1" },
    { "-1", "<int>-1" },
    { "-1l", "<long>-1" },
    { "-1.1f", "<float>-1.1" },
    { "-1.1", "<double>-1.1" },

    { "~1862336341", "<int>-1862336342" },
    { "~7998550172656598869L", "<long>-7998550172656598870" },

    { "!true", "<boolean>false" },
    { "!false", "<boolean>true" }
  });
}


TEST_F(ExpressionEvaluatorTest, UnaryExpressionsInvalid) {
  RunCompilationFailureTestCases({
    { "-true", { TypeMismatch } },
    { "-null", { TypeMismatch } },

    { "+true", { TypeMismatch } },
    { "+null", { TypeMismatch } },

    { "~1.1f", { TypeMismatch } },
    { "~1.1", { TypeMismatch } },
    { "~true", { TypeMismatch } },
    { "~null", { TypeMismatch } },

    { "!1", { TypeMismatch } },
    { "!1l", { TypeMismatch } },
    { "!1.1f", { TypeMismatch } },
    { "!1.1", { TypeMismatch } },
    { "!null", { TypeMismatch } }
  });
}


TEST_F(ExpressionEvaluatorTest, Parenthesis) {
  RunPositiveTestCases({
    { "(2 + 2) * 2", "<int>8" },
    { "false || (false || (false || (true)))", "<boolean>true" },
    { "(((((((((((((1)))))))))))))", "<int>1" },
    { "(1 > 2) || (4 > 5)", "<boolean>false" },
  });
}


TEST_F(ExpressionEvaluatorTest, LocalVariables) {
  readers_factory_.AddFakeLocal<jboolean>("mybool", true);
  readers_factory_.AddFakeLocal<jbyte>("mybyte", -13);
  readers_factory_.AddFakeLocal<jchar>("mychar", 'A');
  readers_factory_.AddFakeLocal<jshort>("myshort", 12345);
  readers_factory_.AddFakeLocal<jint>("myint", -348953478);
  readers_factory_.AddFakeLocal<jlong>("mylong", 39573476573845L);
  readers_factory_.AddFakeLocal<jfloat>("myfloat", 1.23f);
  readers_factory_.AddFakeLocal<jdouble>("mydouble", 4.567);
  readers_factory_.AddFakeLocal("mynull", kJavaStringClassSignature, nullptr);

  JVariant object1 = JVariant::LocalRef(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));
  readers_factory_.AddFakeLocal("mycomposite", "Lcom/prod/MyClass1;", object1);

  RunPositiveTestCases({
    { "mybool", "<boolean>true" },
    { "!mybool", "<boolean>false" },
    { "mybyte", "<byte>-13" },
    { "mychar", "<char>65" },
    { "myshort", "<short>12345" },
    { "myint", "<int>-348953478" },
    { "mylong", "<long>39573476573845" },
    { "myfloat", "<float>1.23" },
    { "mydouble", "<double>4.567" },
    { "mynull", "null" },
    { "mycomposite", "<Object>" },
  });
}


TEST_F(ExpressionEvaluatorTest, InvalidIndentifier) {
  RunCompilationFailureTestCases({
    { "a", { InvalidIdentifier, { "a" } } },
    { "unknown + 3", { InvalidIdentifier, { "unknown" } } },
    { "!myflag",  { InvalidIdentifier, { "myflag" } } }
  });
}


TEST_F(ExpressionEvaluatorTest, InstanceField) {
  JVariant my_obj = JVariant::LocalRef(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));
  readers_factory_.AddFakeLocal("myObj", "LSourceObj;", my_obj);

  readers_factory_.AddFakeInstanceField<jlong>(
      "LSourceObj;",
      "fieldLong",
      183);

  JVariant other_obj = JVariant::LocalRef(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass2));
  readers_factory_.AddFakeInstanceField(
      "LSourceObj;",
      "fieldOtherObj",
      "LOtherObj;", other_obj);

  readers_factory_.AddFakeInstanceField<jboolean>(
      "LOtherObj;",
      "fieldNested",
      true);

  RunPositiveTestCases({
    { "myObj", "<Object>" },
    { "myObj.fieldLong", "<long>183" },
    { "myObj.fieldOtherObj", "<Object>" },
    { "myObj.fieldOtherObj.fieldNested", "<boolean>true" },
  });
}


TEST_F(ExpressionEvaluatorTest, InvalidInstanceField) {
  JVariant my_obj = JVariant::LocalRef(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));
  readers_factory_.AddFakeLocal("myObj", "LSourceObj;", my_obj);

  // Just to verify that "myObj" local variable is set up correctly.
  RunPositiveTestCases({ { "myObj", "<Object>" } });

  RunCompilationFailureTestCases({
    { "myObj.a", { InvalidIdentifier, { "myObj.a" } } },
    { "myObj.unknown + 3", { InvalidIdentifier, { "myObj.unknown" } } },
    { "!myObj.myflag", { InvalidIdentifier, { "myObj.myflag" } } }
  });
}


TEST_F(ExpressionEvaluatorTest, ImplicitLocalInstance) {
  readers_factory_.AddFakeLocal<jboolean>("myBool", true);

  JVariant local_instance_obj = JVariant::LocalRef(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));
  readers_factory_.SetFakeLocalInstance("LSourceObj;", &local_instance_obj);

  readers_factory_.AddFakeInstanceField<jlong>(
      "LSourceObj;",
      "fieldLong",
      183);

  RunPositiveTestCases({
    { "myBool", "<boolean>true" },
    { "fieldLong", "<long>183" },
    { "fieldLong * 10", "<long>1830" }
  });
}


TEST_F(ExpressionEvaluatorTest, InvalidImplicitLocalInstance) {
  JVariant local_instance_obj = JVariant::LocalRef(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));
  readers_factory_.SetFakeLocalInstance("LSourceObj;", &local_instance_obj);

  RunCompilationFailureTestCases({
    { "a", { InvalidIdentifier, { "a" } } },
    { "unknown + 3", { InvalidIdentifier, { "unknown" } } },
    { "!myflag", { InvalidIdentifier, { "myflag" } } }
  });
}


TEST_F(ExpressionEvaluatorTest, PrimitiveTypeInstanceField) {
  readers_factory_.AddFakeLocal<jint>("myint", 31);

  // Just to verify that "myint" local variable is set up correctly.
  RunPositiveTestCases({ { "myint", "<int>31" } });

  RunCompilationFailureTestCases({
    { "myint.a", { PrimitiveTypeField, { "int", "a" } } },
    { "myint.unknown + 3", { PrimitiveTypeField, { "int", "unknown" } } }
  });
}


TEST_F(ExpressionEvaluatorTest, NullDereferenceLocalVariable) {
  readers_factory_.AddFakeLocal("myObj", "LSourceObj;", nullptr);
  readers_factory_.AddFakeInstanceField<jint>("LSourceObj;", "x", 1);

  RunEvaluationFailureTestCases({
    { "myObj.x", { NullPointerDereference } },
    { "2+myObj.x", { NullPointerDereference } },
    { "2*(7 - (4+myObj.x))", { NullPointerDereference } },
    { "-myObj.x", { NullPointerDereference } }
  });
}


TEST_F(ExpressionEvaluatorTest, NullDereferenceLocalInstance) {
  JVariant null_source_obj = JVariant::Null();

  readers_factory_.SetFakeLocalInstance("LSourceObj;", &null_source_obj);
  readers_factory_.AddFakeInstanceField<jint>("LSourceObj;", "x", 1);

  RunEvaluationFailureTestCases({
    { "x", { NullPointerDereference } }
  });
}


TEST_F(ExpressionEvaluatorTest, NonQualifiedStaticField) {
  readers_factory_.AddFakeStaticField<jint>("myStaticInt", 831);

  JVariant my_obj = JVariant::LocalRef(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));
  readers_factory_.AddFakeStaticField(
      "myStaticObj",
      "LMyClass1;",
      static_cast<const JVariant&>(my_obj));

  readers_factory_.AddFakeInstanceField<jlong>(
      "LMyClass1;",
      "innerLong",
      12345678987654321L);

  RunPositiveTestCases({
    { "myStaticInt", "<int>831" },
    { "myStaticInt + 6", "<int>837" },
    { "myStaticObj", "<Object>" },
    { "myStaticObj.innerLong", "<long>12345678987654321" },
  });
}


TEST_F(ExpressionEvaluatorTest, FullyQualifiedStaticField) {
  readers_factory_.AddFakeStaticField<jboolean>(
      "RootClass",
      "myStaticBoolean",
      true);

  readers_factory_.AddFakeStaticField<jint>(
      "com.prod.MyClass1",
      "myStaticInt",
      831);

  JVariant my_obj = JVariant::LocalRef(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));
  readers_factory_.AddFakeStaticField(
      "com.prod.MyClass1",
      "myStaticObj",
      "LMyClass2;",
      my_obj);

  readers_factory_.AddFakeInstanceField<jlong>(
      "LMyClass2;",
      "innerLong",
      12345678987654321L);

  RunPositiveTestCases({
    { "RootClass.myStaticBoolean", "<boolean>true" },
    { "com.prod.MyClass1.myStaticInt", "<int>831" },
    { "com.prod.MyClass1.myStaticObj", "<Object>" },
    { "com.prod.MyClass1.myStaticObj.innerLong", "<long>12345678987654321" },
  });
}


TEST_F(ExpressionEvaluatorTest, TooLongExpression) {
  std::string long_name(kMaxExpressionLength - 2, 'a');
  readers_factory_.AddFakeLocal<jint>(long_name, 1);

  RunPositiveTestCases({
    {
      long_name + "+9",
      "<int>10"
    }
  });

  RunCompilationFailureTestCases({
    { long_name + "+10",  { ExpressionTooLong } }
  });
}


TEST_F(ExpressionEvaluatorTest, TooDeepExpression) {
  RunCompilationFailureTestCases({
    {
      "2+2+2+2+2+2+2+2+2+2+2+2+2+2+2+2+2+2+2+2+2+2+2+2+2+2+2+2+2+2",
      { ExpressionTreeTooDeep }
    }
  });
}


TEST_F(ExpressionEvaluatorTest, StringOutOfMemory) {
  // When FakeJni is asked to create a new Java String with "magic-memory-loss"
  // content, it will return out of memory. This is a simple way to test
  // handling of this out of memory condition.
  RunCompilationFailureTestCases({
    {
      "\"magic-memory-loss\"",  { OutOfMemory }
    }
  });
}


TEST_F(ExpressionEvaluatorTest, LocalInstanceMethodCall) {
  JVariant local_instance_obj = JVariant::LocalRef(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  readers_factory_.SetFakeLocalInstance(
      "Lcom/myprod/MyClass;",
      &local_instance_obj);

  std::vector<ClassMetadataReader::Method> methods = {
      InstanceMethod("Lcom/myprod/MyClass;", "myMethod", "()I")
  };

  EXPECT_CALL(readers_factory_, FindLocalInstanceMethods("myMethod"))
      .WillRepeatedly(Return(methods));

  JVariant return_value = JVariant::Int(18);
  EXPECT_CALL(method_caller_, Invoke(
      "class = Lcom/myprod/MyClass;, method name = myMethod, "
      "method signature = ()I, source = <Object>, arguments = ()"))
      .WillRepeatedly(Return(&return_value));

  RunPositiveTestCases({
    { "myMethod()", "<int>18" },
    { "1+myMethod()", "<int>19" }
  });
}


TEST_F(ExpressionEvaluatorTest, ImplicitStaticInstanceMethodCall) {
  std::vector<ClassMetadataReader::Method> methods = {
      StaticMethod("Lcom/myprod/MyClass;", "myMethod", "()I")
  };

  EXPECT_CALL(readers_factory_, FindStaticMethods("myMethod"))
      .WillRepeatedly(Return(methods));

  EXPECT_CALL(method_caller_, Invoke(
      "class = Lcom/myprod/MyClass;, method name = myMethod, "
      "method signature = ()I, source = <void>, arguments = ()"))
      .WillRepeatedly(Return(FormatMessageModel { "error in call" }));

  RunEvaluationFailureTestCases({
    { "myMethod()", { "error in call" } },
    { "1+myMethod()", { "error in call" } }
  });
}


TEST_F(ExpressionEvaluatorTest, InstanceMethodCall) {
  JVariant my_obj = JVariant::LocalRef(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  readers_factory_.AddFakeLocal("myObj", "LSourceObj;", my_obj);

  std::vector<ClassMetadataReader::Method> methods = {
      InstanceMethod("LSourceObj;", "myMethod", "()F")
  };

  EXPECT_CALL(
      readers_factory_,
      FindInstanceMethods("LSourceObj;", "myMethod", _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(methods), Return(true)));

  JVariant return_value = JVariant::Float(1.23f);
  EXPECT_CALL(method_caller_, Invoke(
      "class = LSourceObj;, method name = myMethod, "
      "method signature = ()F, source = <Object>, arguments = ()"))
      .WillRepeatedly(Return(&return_value));

  RunPositiveTestCases({
    { "myObj.myMethod()", "<float>1.23" },
    { "1+myObj.myMethod()", "<float>2.23" }
  });
}


TEST_F(ExpressionEvaluatorTest, InstanceMethodCall_ClassNotLoaded) {
  JVariant my_obj = JVariant::LocalRef(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  readers_factory_.AddFakeLocal("myObj", "LSourceObj;", my_obj);

  EXPECT_CALL(
      readers_factory_,
      FindInstanceMethods("LSourceObj;", "myMethod", _, _))
      .WillRepeatedly(
          DoAll(
              SetArgPointee<3>(
                  FormatMessageModel { ClassNotLoaded, { "not loaded", "" } }),
              Return(false)));

  RunCompilationFailureTestCases({
    {
      "myObj.myMethod()",
      { ClassNotLoaded, { "not loaded", "" } }
    }
  });
}


TEST_F(ExpressionEvaluatorTest, FullyQualifiedStaticMethodCall) {
  std::vector<ClassMetadataReader::Method> methods = {
      StaticMethod("Lcom/prod/MyClass;", "myMethod", "()J")
  };

  EXPECT_CALL(readers_factory_,
              FindStaticMethods("com.prod.MyClass", "myMethod", _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(methods), Return(true)));

  JVariant return_value = JVariant::Long(-73);
  EXPECT_CALL(method_caller_, Invoke(
      "class = Lcom/prod/MyClass;, method name = myMethod, "
      "method signature = ()J, source = <void>, arguments = ()"))
      .WillRepeatedly(Return(&return_value));

  RunPositiveTestCases({
    { "com.prod.MyClass.myMethod()", "<long>-73" },
    { "1+com.prod.MyClass.myMethod()", "<long>-72" }
  });
}


TEST_F(ExpressionEvaluatorTest, FullyQualifiedStaticMethodCall_ClassNotLoaded) {
  std::vector<ClassMetadataReader::Method> methods = {
      StaticMethod("Lcom/prod/MyClass;", "myMethod", "()J")
  };

  EXPECT_CALL(
      readers_factory_,
      FindStaticMethods("com.prod.MyClass", "myMethod", _, _))
      .WillRepeatedly(
          DoAll(
              SetArgPointee<3>(
                  FormatMessageModel { ClassNotLoaded, { "not loaded" , "" } }),
              Return(false)));

  RunCompilationFailureTestCases({
    {
      "com.prod.MyClass.myMethod()",
      { ClassNotLoaded, { "not loaded", "" } }
    }
  });
}


TEST_F(ExpressionEvaluatorTest, MethodCallUnknownMethod) {
  readers_factory_.AddFakeLocal("myObj", "Lcom/why/SourceObj$Inner;", nullptr);

  EXPECT_CALL(readers_factory_, GetEvaluationPointClassName())
      .WillRepeatedly(Return("com.prod.MyClass"));

  RunCompilationFailureTestCases({
    {
      "myMethod()",
      { ImplicitMethodNotFound, { "myMethod", "com.prod.MyClass" } }
    },
    {
      "theirMethod(1,2,3)",
      { ImplicitMethodNotFound, { "theirMethod", "com.prod.MyClass" } }
    },
    {
      "myObj.bestMethod(\"abc\")",
      { InstanceMethodNotFound, { "bestMethod", "com.why.SourceObj.Inner" } }
    },
    {
      "com.what.for.SmartClass.mediocreMethod(true)",
      { StaticMethodNotFound, { "mediocreMethod", "com.what.for.SmartClass" } }
    }
  });
}


TEST_F(ExpressionEvaluatorTest, MethodCallMultipleMatch) {
  std::vector<ClassMetadataReader::Method> methods = {
      InstanceMethod("LSourceObj;", "myMethod", "()I"),
      InstanceMethod("LSourceObj;", "myMethod", "()I")
  };

  EXPECT_CALL(readers_factory_, FindStaticMethods("myMethod"))
      .WillRepeatedly(Return(methods));

  RunCompilationFailureTestCases({
    { "myMethod()",  { AmbiguousMethodCall, { "myMethod" } } },
    { "true || myMethod()",  { AmbiguousMethodCall, { "myMethod" } } }
  });
}


TEST_F(ExpressionEvaluatorTest, MethodArgumentMismatch) {
  JVariant local_instance_obj = JVariant::LocalRef(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  readers_factory_.SetFakeLocalInstance(
      "Lcom/myprod/MyClass;",
      &local_instance_obj);

  const struct {
    std::string method_signature;
    std::string invocation_arguments;
  } test_cases[] = {
    { "()V", "1" },
    { "(I)V", "" },
    { "(I)V", "true" },
    { "(Z)V", "12" },
    { "(III)V", "12,13,14,15" },
    { "(Ljava/lang/String;I)V", "\"abc\", true" },
    { "(LMyClass;)V", "\"abc\"" },
  };

  EXPECT_CALL(readers_factory_, GetEvaluationPointClassName())
      .WillRepeatedly(Return("com.prod.MyClass"));

  for (const auto& test_case : test_cases) {
    std::vector<ClassMetadataReader::Method> methods = {
        InstanceMethod(
            "Lcom/myprod/MyClass;",
            "myMethod",
            test_case.method_signature)
    };

    EXPECT_CALL(readers_factory_, FindLocalInstanceMethods("myMethod"))
        .WillRepeatedly(Return(methods));

    RunCompilationFailureTestCases({
      {
        "myMethod(" + test_case.invocation_arguments + ")",
        { MethodCallArgumentsMismatchSingleCandidate, { "myMethod" } }
      }
    });
  }
}


TEST_F(ExpressionEvaluatorTest, MethodCallImplicitCast) {
  EXPECT_CALL(readers_factory_, GetEvaluationPointClassName())
      .WillRepeatedly(Return("com.prod.MyClass"));

  JVariant local_instance_obj = JVariant::LocalRef(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  std::vector<ClassMetadataReader::Method> methods = {
      StaticMethod(
          "Lcom/myprod/MyClass;",
          "staticMethod",
          "(Ljava/lang/Object;)Z")
  };

  EXPECT_CALL(readers_factory_,
              FindStaticMethods("com.myprod.MyClass", "staticMethod", _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(methods), Return(true)));

  JVariant return_value = JVariant::Boolean(true);
  EXPECT_CALL(method_caller_, Invoke(
      "class = Lcom/myprod/MyClass;, method name = staticMethod, "
      "method signature = (Ljava/lang/Object;)Z, "
      "source = <void>, arguments = (<Object>)"))
      .WillRepeatedly(Return(&return_value));

  RunPositiveTestCases({
    { "com.myprod.MyClass.staticMethod(\"abc\")", "<boolean>true" },
  });
}


TEST_F(ExpressionEvaluatorTest, MethodCallOnPrimitiveType) {
  EXPECT_CALL(readers_factory_, GetEvaluationPointClassName())
      .WillRepeatedly(Return("com.prod.MyClass"));

  readers_factory_.AddFakeLocal<jint>("myInt", 12);

  RunCompilationFailureTestCases({
    {
      "myInt.myMethod()",
      { MethodCallOnPrimitiveType, { "myMethod", "int" } }
    }
  });
}


TEST_F(ExpressionEvaluatorTest, MethodCallNullImplicitCast) {
  EXPECT_CALL(readers_factory_, GetEvaluationPointClassName())
      .WillRepeatedly(Return("com.prod.MyClass"));

  JVariant local_instance_obj = JVariant::LocalRef(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  std::vector<ClassMetadataReader::Method> methods = {
      StaticMethod(
          "Lcom/myprod/MyClass;",
          "staticMethod",
          "(Ljava/lang/String;)Z")
  };

  EXPECT_CALL(readers_factory_,
              FindStaticMethods("com.myprod.MyClass", "staticMethod", _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(methods), Return(true)));

  JVariant return_value = JVariant::Boolean(true);
  EXPECT_CALL(
      method_caller_,
      Invoke("class = Lcom/myprod/MyClass;, method name = staticMethod, "
             "method signature = (Ljava/lang/String;)Z, "
             "source = <void>, arguments = (null)"))
      .WillRepeatedly(Return(&return_value));

  RunPositiveTestCases({
    { "com.myprod.MyClass.staticMethod(null)", "<boolean>true" },
  });
}


TEST_F(ExpressionEvaluatorTest, MethodCallMultipleArguments) {
  JVariant local_instance_obj = JVariant::LocalRef(
      fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));

  readers_factory_.SetFakeLocalInstance(
      "Lcom/myprod/MyClass;",
      &local_instance_obj);

  std::vector<JVariant> return_values;
  for (int arguments_count = 0; arguments_count < 11; ++arguments_count) {
    return_values.push_back(JVariant::Float(10 + arguments_count / 10.0f));
  }

  std::vector<ClassMetadataReader::Method> methods;
  for (int arguments_count = 0; arguments_count < 11; ++arguments_count) {
    std::string signature = "(" + std::string(arguments_count, 'I') + ")F";
    methods.push_back(
        InstanceMethod("Lcom/myprod/MyClass;", "myMethod", signature));

    std::string expected_arguments;
    for (int i = 0; i < arguments_count; ++i) {
      if (i > 0) {
        expected_arguments += ", ";
      }
      expected_arguments += "<int>";
      expected_arguments += std::to_string(arguments_count * 100 + i);
    }

    EXPECT_CALL(method_caller_, Invoke(
        "class = Lcom/myprod/MyClass;, method name = myMethod, "
        "method signature = " + signature + ", source = <Object>, "
        "arguments = (" + expected_arguments + ")"))
        .WillRepeatedly(Return(&return_values[arguments_count]));
  }

  EXPECT_CALL(readers_factory_, FindLocalInstanceMethods("myMethod"))
      .WillRepeatedly(Return(methods));

  RunPositiveTestCases({
    { "myMethod()", "<float>10" },
    { "myMethod(100)", "<float>10.1" },
    { "myMethod(200, 201)", "<float>10.2" },
    { "myMethod(300, 301, 302)", "<float>10.3" },
    { "myMethod(400, 401, 402, 403)", "<float>10.4" },
    { "myMethod(500, 501, 502, 503, 504)", "<float>10.5" },
    { "myMethod(600, 601, 602, 603, 604, 605)", "<float>10.6" },
    { "myMethod(700, 701, 702, 703, 704, 705, 706)", "<float>10.7" },
    { "myMethod(800, 801, 802, 803, 804, 805, 806, 807)", "<float>10.8" },
    { "myMethod(900, 901, 902, 903, 904, 905, 906, 907, 908)", "<float>10.9" },
    {
      "myMethod(1000, 1001, 1002, 1003, 1004, 1005, 1006, 1007, 1008, 1009)",
      "<float>11"
    }
  });

  RunCompilationFailureTestCases({
    {
      "myMethod(1,1,1,1,1,1,1,1,1,1,1)",
      { MethodCallArgumentsMismatchMultipleCandidates, { "myMethod" } }
    }
  });
}


TEST_F(ExpressionEvaluatorTest, UnsafeMethodCall) {
  std::vector<ClassMetadataReader::Method> methods = {
      StaticMethod(
          "Lcom/myprod/UnsafeClass;",
          "myMethod",
          "()Z")
  };

  EXPECT_CALL(readers_factory_, FindStaticMethods("myMethod"))
      .WillRepeatedly(Return(methods));

  EXPECT_CALL(method_caller_, Invoke(
      "class = Lcom/myprod/UnsafeClass;, "
      "method name = myMethod, method signature = ()Z, "
      "source = <void>, arguments = ()"))
      .WillRepeatedly(Return(
          FormatMessageModel { MethodNotSafe, { "myMethod" } }));

  RunEvaluationFailureTestCases({
    {
      "myMethod()",
      { MethodNotSafe, { "myMethod" } }
    }
  });
}

}  // namespace cdbg
}  // namespace devtools
