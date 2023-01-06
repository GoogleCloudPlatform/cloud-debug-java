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


#include "JavaExpressionCompiler.hpp"
#include "JavaExpressionLexer.hpp"
#include "JavaExpressionParser.hpp"
#include "gtest/gtest.h"
#include "src/agent/common.h"
#include "src/agent/java_expression.h"
#include "tests/agent/fake_jni.h"
#include "tests/agent/mock_jvmti_env.h"

namespace devtools {
namespace cdbg {

struct PositiveParserTestCase {
  std::string input;
  std::string canonical_form;
};

static std::string RepeatString(const std::string source, int n) {
  std::string repeated;
  while (n-- > 0) {
    repeated += source;
  }

  return repeated;
}

// Wraps string in double quotes. This is just a convenience function to make
// the test cases look less cumbersome.
std::string WrapDoubleQuotes(const std::string& s) { return '"' + s + '"'; }

// Common test cases code to parse expression expecting success.
static std::unique_ptr<JavaExpression> ParseExpression(const std::string& input,
                                                       std::string* ast) {
  ast->clear();

  std::istringstream input_stream(input);
  JavaExpressionLexer lexer(input_stream);
  JavaExpressionParser parser(lexer);
  parser.Init();

  parser.statement();
  /**
   * TODO: FIX
  if (parser.ActiveException()) {
    parser.reportError(parser.ActiveException()->getMessage());
  }
  */

  if (parser.num_errors() > 0) {
    ADD_FAILURE() << "Expression parsing failed" << std::endl
                  << "Input: " << input << std::endl
                  << "Parser error: " << parser.errors()[0];
    return nullptr;
  }

  JavaExpressionCompiler compiler;
  compiler.Init();

  *ast = parser.getAST()->toStringTree();

  std::unique_ptr<JavaExpression> expression = compiler.Walk(parser.getAST());
  if (expression == nullptr) {
    ADD_FAILURE() << "Tree walking on parsed expression failed" << std::endl
                  << "Input: " << input << std::endl << "AST: " << *ast;
  }

  return expression;
}

// Common code for positive parser test cases.
static void ParserPositiveCommon(
    std::initializer_list<PositiveParserTestCase> test_cases) {
  for (const auto& test_case : test_cases) {
    std::string ast;
    std::unique_ptr<JavaExpression> expression =
        ParseExpression(test_case.input, &ast);
    if (expression == nullptr) {
      continue;  // All failures are reported in "ParseExpression".
    }

    std::ostringstream str_stream;
    expression->Print(&str_stream, false);

    EXPECT_EQ(test_case.canonical_form, str_stream.str())
        << "Input: " << test_case.input << std::endl << "AST: " << ast;
  }
}


class JavaExpressionTest : public ::testing::Test {
 protected:
  JavaExpressionTest()
      : fake_jni_(&jvmti_),
        global_jvm_(&jvmti_, fake_jni_.jni()) {
  }

 protected:
  MockJvmtiEnv jvmti_;
  FakeJni fake_jni_;
  GlobalJvmEnv global_jvm_;
};


TEST(JavaExpressionTest, NumericLiterals) {
  ParserPositiveCommon({
    {
      "42",
      "<int>42"
    },
    {
      "42L",
      "<long>42L"
    },
    {
      "1.2f",
      "<float>1.2F",
    },
    {
      "3.4",
      "<double>3.4",
    },
    {
      ".7",
      "<double>0.7",
    },
    {
      ".8f",
      "<float>0.8F",
    },
    {
      "-.9f",
      "-<float>0.9F",
    },
    {
      "12f",
      "<float>12F",
    },
    {
      "0x100",
      "<int>256",
    },
    {
      "0100",
      "<int>64",
    },
  });
}


TEST(JavaExpressionTest, BinaryExpressionsLiterals) {
  ParserPositiveCommon({
    {
      "a + b * c - d",
      "(('a' + ('b' * 'c')) - 'd')"
    },
    {
      "a + b - 7",
      "(('a' + 'b') - <int>7)"
    },
    {
      "a * b / c",
      "(('a' * 'b') / 'c')"
    },
    {
      "a + b % c",
      "('a' + ('b' % 'c'))"
    },
    {
      "2147483648L + 2147483647",
      "(<long>2147483648L + <int>2147483647)"
    },
    {
      "a&&b",
      "('a' && 'b')"
    },
    {
      "a||b",
      "('a' || 'b')"
    },
    {
      "a&&b || c&&d",
      "(('a' && 'b') || ('c' && 'd'))"
    },
    {
      "a==b",
      "('a' == 'b')"
    },
    {
      "a!=b",
      "('a' != 'b')"
    },
    {
      "a<=b",
      "('a' <= 'b')"
    },
    {
      "a>=b",
      "('a' >= 'b')"
    },
    {
      "a<b",
      "('a' < 'b')"
    },
    {
      "a>b",
      "('a' > 'b')"
    },
    {
      "a&b",
      "('a' & 'b')"
    },
    {
      "a|b",
      "('a' | 'b')"
    },
    {
      "a^b",
      "('a' ^ 'b')"
    },
    {
      "a<<b",
      "('a' << 'b')"
    },
    {
      "a>>b",
      "('a' >> 'b')"
    },
    {
      "a>>> b",
      "('a' >>> 'b')"
    },
    {
      "true || false",
      "(true || false)"
    },
    {
      "a == null",
      "('a' == null)"
    },
  });
}

TEST(JavaExpressionTest, InstanceofBinaryExpressionsLiterals) {
  ParserPositiveCommon({
      {"a instanceof MyClass", "('a' instanceof MyClass)"},
      {"a instanceof com.util.Myclass", "('a' instanceof com.util.Myclass)"},
      {"a instanceof package1.Type1", "('a' instanceof package1.Type1)"},
      {"a.b.c instanceof MyClass", "('a'.b.c instanceof MyClass)"},
      {"a() instanceof MyClass", "(<call>( a() ) instanceof MyClass)"},
      {"a[1] instanceof MyClass", "('a'[<int>1] instanceof MyClass)"},
      {"a[1].b instanceof MyClass", "('a'[<int>1].b instanceof MyClass)"},
  });
}

TEST(JavaExpressionTest, UnaryExpressionsLiterals) {
  ParserPositiveCommon({
    {
      "-7",
      "-<int>7"
    },
    {
      "+7",
      "+<int>7"
    },
    {
      "~7",
      "~<int>7"
    },
    {
      "-5.6",
      "-<double>5.6",
    },
    {
      "!a",
      "!'a'"
    },
  });
}


TEST(JavaExpressionTest, Conditionals) {
  ParserPositiveCommon({
    {
      "a ? b : c",
      "('a' ? 'b' : 'c')"
    },
    {
      "1 ? 2 : 3 ? 4 : 5",
      "(<int>1 ? <int>2 : (<int>3 ? <int>4 : <int>5))"
    },
  });
}


TEST(JavaExpressionTest, Parenthesis) {
  ParserPositiveCommon({
    {
      "(7 + 8)",
      "(<int>7 + <int>8)"
    },
    {
      "(a + b) * c",
      "(('a' + 'b') * 'c')"
    },
  });
}


TEST(JavaExpressionTest, Char) {
  ParserPositiveCommon({
    {
      "'x'",
      "<char>'x'"
    },
    {
      "'\"'",
      "<char>'\\u0022'"
    },
    {
      "'\\uffff'",
      "<char>'\\uffff'"
    },
    {
      "'A' == 65",
      "(<char>'A' == <int>65)"
    },
  });
}


TEST(JavaExpressionTest, String) {
  ParserPositiveCommon({
    // Regular strings.
    {
      WrapDoubleQuotes("abcdefgh"),
      WrapDoubleQuotes("abcdefgh"),
    },
    {
      WrapDoubleQuotes(""),
      WrapDoubleQuotes(""),
    },

    // Escaped characters.
    {
      WrapDoubleQuotes("\\t"),
      WrapDoubleQuotes("\\u0009"),
    },
    {
      WrapDoubleQuotes("\\b"),
      WrapDoubleQuotes("\\u0008"),
    },
    {
      WrapDoubleQuotes("\\n"),
      WrapDoubleQuotes("\\u000a"),
    },
    {
      WrapDoubleQuotes("\\r"),
      WrapDoubleQuotes("\\u000d"),
    },
    {
      WrapDoubleQuotes("\\f"),
      WrapDoubleQuotes("\\u000c"),
    },
    {
      WrapDoubleQuotes("\\\\"),
      WrapDoubleQuotes("\\u005c"),
    },
    {
      WrapDoubleQuotes("\\\""),
      WrapDoubleQuotes("\\u0022"),
    },
    {
      WrapDoubleQuotes("\\'"),
      WrapDoubleQuotes("\\u0027"),
    },
    {
      WrapDoubleQuotes("\\nABC\\n\\n\\n\\nFE\\n\\nn\\\\rbb\\t"),
      WrapDoubleQuotes("\\u000aABC\\u000a\\u000a\\u000a\\u000aFE\\u000a"
                       "\\u000an\\u005crbb\\u0009"),
    },
    {
      WrapDoubleQuotes("123\\n4"),
      WrapDoubleQuotes("123\\u000a4"),
    },

    // Octal encoding.
    {
      WrapDoubleQuotes("\\7"),
      WrapDoubleQuotes("\\u0007"),
    },
    {
      WrapDoubleQuotes("\\78"),
      WrapDoubleQuotes("\\u00078"),
    },
    {
      WrapDoubleQuotes("\\64"),
      WrapDoubleQuotes("4"),
    },
    {
      WrapDoubleQuotes("\\64a"),
      WrapDoubleQuotes("4a"),
    },
    {
      WrapDoubleQuotes("\\101"),
      WrapDoubleQuotes("A"),
    },
    {
      WrapDoubleQuotes("A\\102C"),
      WrapDoubleQuotes("ABC"),
    },
    {
      WrapDoubleQuotes("\\444"),
      WrapDoubleQuotes("\\u005c444"),
    },

    // Unicode encoding.
    {
      WrapDoubleQuotes("\\u0041"),
      WrapDoubleQuotes("A"),
    },
    {
      WrapDoubleQuotes("A\\u0042C"),
      WrapDoubleQuotes("ABC"),
    },
  });
}


TEST(JavaExpressionTest, Variables) {
  ParserPositiveCommon({
    {
      "t",
      "'t'"
    },
    {
      "first.second.third+2",
      "('first'.second.third + <int>2)",
    },
    {
      "arr[7]*3",
      "('arr'[<int>7] * <int>3)",
    },
    {
      "arr[7][9].brr[13].crr[x << y] /17",
      "('arr'[<int>7][<int>9].brr[<int>13].crr[('x' << 'y')] / <int>17)",
    },
  });
}


TEST(JavaExpressionTest, ParserNegative) {
  const std::string test_cases[] = {
      std::string(),
      "1 ? 2 : 3 : 4",
      "1 ? 2 : 3 ? 4",
      RepeatString("2+", 10000) + "2",
      "1 + " + RepeatString("2*", 10000) + "2",
      "'ab'",
      "'\\612'",
      "'\\729'",
      WrapDoubleQuotes("\\"),
      WrapDoubleQuotes("\\u004"),
      WrapDoubleQuotes("\\u00"),
      WrapDoubleQuotes("\\u0"),
      WrapDoubleQuotes("\\u"),
      WrapDoubleQuotes("\\u111J"),
      WrapDoubleQuotes("\\u11J1"),
      WrapDoubleQuotes("\\u1J11"),
      WrapDoubleQuotes("\\uJ111"),
      "f(,)",
      "f(a,)",
      "f(",
      "()x",
      "(.)x",
      "(a.)x",
      "(List<String>)x",            // Generics not supported.
      "(java.util.List<String>)x",  // Generics not supported.
      "(type1())y",
      "myMethod(1111111111111111111111111111111111111111)",
      "a.b.c.myMethod(1111111111111111111111111111111111111111)",
      "1*myMethod(1111111111111111111111111111111111111111)",
      "(myMethod(1111111111111111111111111111111111111111))",
      "-myMethod(1111111111111111111111111111111111111111)",
      "3+(myMethod(1111111111111111111111111111111111111111))",
      "(verylonginteger)1111111111111111111111111111111111111111",
      "-(verylonginteger)1111111111111111111111111111111111111111",
      "(verylonginteger)1111111111111111111111111111111111111111 ? true : "
      "false",
      "a ? (verylonginteger)1111111111111111111111111111111111111111 : false",
      "a ? true : (verylonginteger)1111111111111111111111111111111111111111",
      "a[1111111111111111111111111111111111111111]",
      "a.b.c[1111111111111111111111111111111111111111]",
      "-a.b.c[1111111111111111111111111111111111111111]",
  };

  for (const std::string& test_case : test_cases) {
    std::istringstream input_stream(test_case);
    JavaExpressionLexer lexer(input_stream);
    JavaExpressionParser parser(lexer);
    parser.Init();

    parser.statement();
    /**
     * TODO: FIX
    if (parser.ActiveException()) {
      parser.reportError(parser.ActiveException()->getMessage());
    }
    */

    if (parser.num_errors() > 0) {
      continue;
    }

    JavaExpressionCompiler compiler;
    compiler.Init();

    std::unique_ptr<JavaExpression> expression(compiler.Walk(parser.getAST()));
    if (expression == nullptr) {
      continue;
    }

    std::ostringstream str_stream;
    expression->Print(&str_stream, false);

    FAIL() << "Input: " << test_case << " was parsed successfuly, "
                                        "but was supposed to fail" << std::endl
           << "AST: " << parser.getAST()->toStringTree() << std::endl
           << "Canonical form: " << str_stream.str();
  }
}


TEST(JavaExpressionTest, TryGetTypeNamePositive) {
  const struct {
    std::string input;
    std::string expected_signature;
  } test_cases[] = {
    { "MyClass", "MyClass" },
    { "com.MyClass", "com.MyClass" },
    { "com.myprod.MyClass", "com.myprod.MyClass" }
  };

  for (const auto& test_case : test_cases) {
    std::string ast;
    std::unique_ptr<JavaExpression> expression =
        ParseExpression(test_case.input, &ast);
    if (expression == nullptr) {
      continue;  // All failures are reported in "ParseExpression".
    }

    std::string actual_signature;
    EXPECT_TRUE(expression->TryGetTypeName(&actual_signature))
        << "Input: " << test_case.input << std::endl << "AST: " << ast;
    EXPECT_EQ(test_case.expected_signature, actual_signature)
        << "Input: " << test_case.input << std::endl << "AST: " << ast;
  }
}


TEST(JavaExpressionTest, TryGetTypeNameNegative) {
  const std::string test_cases[] = {"2",     "'c'",      "\"str\"",  "a+b",
                                    "a?b:c", "a[3].b.c", "(a?b:c).x"};

  for (const std::string& test_case : test_cases) {
    std::string ast;
    std::unique_ptr<JavaExpression> expression =
        ParseExpression(test_case, &ast);
    if (expression == nullptr) {
      continue;  // All failures are reported in "ParseExpression".
    }

    std::string signature;
    EXPECT_FALSE(expression->TryGetTypeName(&signature))
        << "Input: " << test_case << std::endl << "AST: " << ast;
  }
}


TEST(JavaExpressionTest, MethodCall) {
  ParserPositiveCommon({
    {
      "f()",
      "<call>( f() )"
    },
    {
      "f(1)",
      "<call>( f(<int>1) )",
    },
    {
      "f(1,2)",
      "<call>( f(<int>1, <int>2) )",
    },
    {
      "f(1,2,3)",
      "<call>( f(<int>1, <int>2, <int>3) )",
    },
    {
      "a.f()",
      "<call>( 'a'.f() )",
    },
    {
      "a.f(x)",
      "<call>( 'a'.f('x') )",
    },
    {
      "a.f(x, y)",
      "<call>( 'a'.f('x', 'y') )",
    },
    {
      "math.util.compute(sin(x), cos(x), 2 * tan (z.b))",
      "<call>( 'math'.util.compute("
          "<call>( sin('x') ), "
          "<call>( cos('x') ), "
          "(<int>2 * <call>( tan('z'.b) ))) )",
    },
    {
      "getA().getB(true).getC()",
      "<call>( <call>( <call>( getA() ).getB(true) ).getC() )",
    },
    {
      "a*f()",
      "('a' * <call>( f() ))",
    },
  });
}


TEST(JavaExpressionTest, TypeCast) {
  ParserPositiveCommon({
    {
      "(MyClass)x",
      "cast<MyClass>('x')"
    },
    {
      "(com.util.MyClass)a.b.c + 3",
      "(cast<com.util.MyClass>('a'.b.c) + <int>3)"
    },
    {
      "(package1.Type1)(package2.Type2)a",
      "cast<package1.Type1>(cast<package2.Type2>('a'))"
    },
    {
      "(MyClass)a[1]",
      "cast<MyClass>('a'[<int>1])"
    },
    {
      "(MyClass)a[1].b",
      "cast<MyClass>('a'[<int>1].b)"
    },
    {
      "(MyClass)f()",
      "cast<MyClass>(<call>( f() ))"
    },
  });
}

}  // namespace cdbg
}  // namespace devtools


