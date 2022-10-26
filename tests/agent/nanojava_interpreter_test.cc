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

#include "src/agent/nanojava_interpreter.h"

#include <cstdint>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "jni_proxy_classcastexception.h"
#include "jni_proxy_classfiletextifier.h"
#include "jni_proxy_exception.h"
#include "jni_proxy_jasmin_main.h"
#include "jni_proxy_object.h"
#include "jni_proxy_string.h"
#include "src/agent/jni_method_caller.h"
#include "src/agent/jvm_class_indexer.h"
#include "src/agent/model_util.h"
#include "src/agent/type_util.h"
#include "tests/agent/jasmin_utils.h"

using testing::MatchesRegex;

namespace {

// Provides absl::Substitue type functionality, but only accepts strings as
// arguments. Serves the needs of this test file without needing to depend on
// abseil, which we don't want to depend on at this time given we currently
// depend on glog and gflags and want to avoid any issues.
std::string Substitute(const std::string& fmt,
                       const std::vector<std::string>& args) {
  CHECK_LE(args.size(), 10);
  std::string output;
  int buf_pos = 0;
  int arg_pos;
  while ((arg_pos = fmt.find("$", buf_pos)) != std::string::npos) {
    CHECK_GE(fmt.size() - arg_pos, 2);

    output += fmt.substr(buf_pos, arg_pos - buf_pos);

    if (fmt[arg_pos + 1] == '$') {
      output += "$";
    } else {
      CHECK_GE(fmt[arg_pos + 1], '0');
      CHECK_LE(fmt[arg_pos + 1], '9');
      int arg_idx = fmt[arg_pos + 1] - '0';
      output += args[arg_idx];
    }

    buf_pos = arg_pos + 2;
  }

  output += fmt.substr(buf_pos, fmt.size() - buf_pos);
  return output;
}

std::string Substitute(const std::string& fmt, const std::string& arg1) {
  std::vector<std::string> args = {arg1};
  return Substitute(fmt, args);
}

std::string Substitute(const std::string& fmt, const std::string& arg1,
                       const std::string& arg2) {
  std::vector<std::string> args = {arg1, arg2};
  return Substitute(fmt, args);
}

std::string Substitute(const std::string& fmt, const std::string& arg1,
                       const std::string& arg2, const std::string& arg3) {
  std::vector<std::string> args = {arg1, arg2, arg3};
  return Substitute(fmt, args);
}

std::string Substitute(const std::string& fmt, const std::string& arg1,
                       const std::string& arg2, const std::string& arg3,
                       const std::string& arg4) {
  std::vector<std::string> args = {arg1, arg2, arg3, arg4};
  return Substitute(fmt, args);
}

std::string Substitute(const std::string& fmt, const std::string& arg1,
                       const std::string& arg2, const std::string& arg3,
                       const std::string& arg4, const std::string& arg5) {
  std::vector<std::string> args = {arg1, arg2, arg3, arg4, arg5};
  return Substitute(fmt, args);
}

std::string Substitute(const std::string& fmt, const std::string& arg1,
                       const std::string& arg2, const std::string& arg3,
                       const std::string& arg4, const std::string& arg5,
                       const std::string& arg6) {
  std::vector<std::string> args = {arg1, arg2, arg3, arg4, arg5, arg6};
  return Substitute(fmt, args);
}

}

namespace devtools {
namespace cdbg {
namespace nanojava {

class FakeSupervisor : public NanoJavaInterpreter::Supervisor {
 public:
  MethodCallResult InvokeNested(
      bool nonvirtual,
      const ConstantPool::MethodRef& method,
      jobject source,
      std::vector<JVariant> arguments) override {
    JniLocalRef cls;
    if (method.metadata.value().is_static() || nonvirtual) {
      cls = JniNewLocalRef(method.owner_cls.get());
    } else {
      cls = GetObjectClass(source);
    }
    EXPECT_NE(nullptr, cls);

    JniMethodCaller method_caller;
    method_caller.Bind(
        static_cast<jclass>(cls.get()),
        method.metadata.value());
    return method_caller.Call(nonvirtual, source, std::move(arguments));
  }

  std::unique_ptr<FormatMessageModel> IsNextInstructionAllowed() override {
    return nullptr;
  }

  void NewObjectAllocated(jobject obj) override {}

  std::unique_ptr<FormatMessageModel> IsNewArrayAllowed(
      int32_t count) override {
    return nullptr;
  }

  std::unique_ptr<FormatMessageModel> IsArrayModifyAllowed(
      jobject array) override {
    return nullptr;
  }

  std::unique_ptr<FormatMessageModel> IsFieldModifyAllowed(
      jobject target,
      const ConstantPool::FieldRef& field) override {
    return nullptr;
  }
};


class NanoJavaInterpreterTest : public testing::Test {
 protected:
  void SetUp() override {
    CHECK(BindSystemClasses());
    CHECK(jniproxy::BindClassFileTextifier());
    CHECK(jniproxy::jasmin::BindMain());

    // Make sure that classes that we need are loaded.
    const std::string test_lib_base =
        "com/google/devtools/cdbg/debuglets/java/";
    const std::string preload_classes[] = {
        test_lib_base + "NanoJavaInterpreterTestLib",
        test_lib_base + "NanoJavaInterpreterTestLib$InstanceFields",
        test_lib_base + "NanoJavaInterpreterTestLib$StaticFields",
        "java/util/NoSuchElementException",
        "java/net/URISyntaxException",
        "java/lang/ReflectiveOperationException"};
    for (const std::string& internal_name : preload_classes) {
      JavaClass cls;
      EXPECT_TRUE(cls.FindWithJNI(internal_name.c_str()));
    }

    class_indexer_.Initialize();
  }

  void TearDown() override {
    jniproxy::CleanupClassFileTextifier();
    jniproxy::jasmin::CleanupMain();
    CleanupSystemClasses();
  }

  // Executes the method expecting it to succeed. Returns method return value
  // formatted as a string.
  std::string ExecuteExpectSuccess(const std::string& blob,
                                   const std::vector<JVariant>& arguments) {
    std::unique_ptr<ClassFile> class_file =
        ClassFile::LoadFromBlob(&class_indexer_, blob);
    EXPECT_NE(nullptr, class_file);

    EXPECT_EQ(1, class_file->GetMethodsCount());

    FakeSupervisor supervisor;
    NanoJavaInterpreter interpreter(
        &supervisor,
        class_file->GetMethod(0),
        nullptr,
        nullptr,
        arguments);

    MethodCallResult result = interpreter.Execute();
    switch (result.result_type()) {
      case MethodCallResult::Type::Error:
        ADD_FAILURE() << "Method execution failed: " << result.error();
        return "";

      case MethodCallResult::Type::JavaException:
        ADD_FAILURE()
            << "Unexpected Java exception: "
            << jniproxy::Object()->toString(result.exception()).GetData();
        return "";

      case MethodCallResult::Type::Success: {
        const JVariant& return_value = result.return_value();

        if (return_value.has_non_null_object()) {
          jobject obj = nullptr;
          return_value.get<jobject>(&obj);

          std::string string_value;
          if (GetObjectClassSignature(obj).front() == '[') {
            string_value = ArrayToString(obj);
          } else {
            string_value = jniproxy::Object()->toString(obj).GetData();
          }

          return "<" +
                 TypeNameFromSignature(
                     { JType::Object, GetObjectClassSignature(obj) }) +
                 ">" +
                 string_value;
        }

        return return_value.ToString(false);
      }
    }
  }

  // Executes the method expecting it to throw an exception.
  std::string ExecuteExpectException(const std::string& blob,
                                     const std::vector<JVariant>& arguments) {
    std::unique_ptr<ClassFile> class_file =
        ClassFile::LoadFromBlob(&class_indexer_, blob);
    EXPECT_NE(nullptr, class_file);

    EXPECT_EQ(1, class_file->GetMethodsCount());

    FakeSupervisor supervisor;
    NanoJavaInterpreter interpreter(
        &supervisor,
        class_file->GetMethod(0),
        nullptr,
        nullptr,
        arguments);

    MethodCallResult result = interpreter.Execute();
    switch (result.result_type()) {
      case MethodCallResult::Type::Error:
        ADD_FAILURE() << "Method execution failed: " << result.error();
        return "";

      case MethodCallResult::Type::JavaException:
        return jniproxy::Object()->toString(result.exception()).GetData();

      case MethodCallResult::Type::Success: {
        ADD_FAILURE()
            << "Unexpected method success: "
            << result.return_value().ToString(false);
        return "";
      }
    }
  }

  // Executes the method expecting it to fail.
  FormatMessageModel ExecuteExpectError(
      const std::string& blob, const std::vector<JVariant>& arguments) {
    std::unique_ptr<ClassFile> class_file =
        ClassFile::LoadFromBlob(&class_indexer_, blob);
    EXPECT_NE(nullptr, class_file);

    EXPECT_EQ(1, class_file->GetMethodsCount());

    FakeSupervisor supervisor;
    NanoJavaInterpreter interpreter(
        &supervisor,
        class_file->GetMethod(0),
        nullptr,
        nullptr,
        arguments);

    MethodCallResult result = interpreter.Execute();
    EXPECT_EQ(MethodCallResult::Type::Error, result.result_type());
    LOG(INFO) << result.error();
    return result.error();
  }

  static std::string ArrayToString(jobject array) {
    std::string signature = GetObjectClassSignature(array);
    if (signature.size() > 2) {
      signature = "[Ljava/lang/Object;";
    }

    JavaClass arrays_cls;
    arrays_cls.FindWithJNI("java/util/Arrays");
    jmethodID method_id = arrays_cls.GetStaticMethod(
        "toString",
        "(" + signature + ")Ljava/lang/String;");

    JniLocalRef str(jni()->CallObjectMethod(
        arrays_cls.get(),
        method_id,
        array));
    if (!JniCheckNoException("Arrays.toString")) {
      ADD_FAILURE() << "Arrays.toString() threw an exception";
      return std::string();
    }

    return JniToNativeString(str.get());
  }

 protected:
  JvmClassIndexer class_indexer_;
};


TEST_F(NanoJavaInterpreterTest, Nop) {
  const std::string blob = AssembleMethod("I",
                                          R"(.limit stack 1
         nop
         ldc 487
         ireturn)");

  ASSERT_EQ("<int>487", ExecuteExpectSuccess(blob, {}));
}


// Test binary operation (e.g. type = float, op = * ):
// type run(type n1, type n2) {
//   return n1 op n2;
// }
#define BINARY_ARITHMETIC_OPERATION_TEST(optype, opcode, operation, n1, n2)    \
  TEST_F(NanoJavaInterpreterTest, Opcode_##optype##opcode) {                   \
    const int primitive_size = sizeof(n1) / 4;                                 \
                                                                               \
    LOG(INFO) << "Testing " << JVariant::Primitive(n1).ToString(false) << " "  \
              << STRINGIFY(operation) << " "                                   \
              << JVariant::Primitive(n2).ToString(false)                       \
              << " (size: " << primitive_size << ")";                          \
                                                                               \
    std::shared_ptr<ClassIndexer::Type> return_type =                          \
        class_indexer_.GetPrimitiveType(JVariant::Primitive(n1).type());       \
                                                                               \
    const std::string blob =                                                   \
        AssembleMethod(return_type->GetSignature(),                            \
                             Substitute(".limit stack $0\n"                    \
                                        ".limit locals $1\n"                   \
                                        "$2load 0\n"                           \
                                        "$2load $3\n"                          \
                                        "$2$4\n"                               \
                                        "$2return\n",                          \
                                        std::to_string(primitive_size * 2),    \
                                        std::to_string(primitive_size * 2),    \
                                        STRINGIFY(optype),                     \
                                        std::to_string(primitive_size),        \
                                        STRINGIFY(opcode)));                   \
                                                                               \
    ASSERT_EQ(JVariant::Primitive(n1 operation n2).ToString(false),            \
              ExecuteExpectSuccess(                                            \
                  blob, {JVariant::Primitive(n1), JVariant::Primitive(n2)}));  \
  }

BINARY_ARITHMETIC_OPERATION_TEST(i, add, +, 345345, 234234234)
BINARY_ARITHMETIC_OPERATION_TEST(i, sub, -, -234897234, 891286123)
BINARY_ARITHMETIC_OPERATION_TEST(i, mul, *, 232345, 1283487234)
BINARY_ARITHMETIC_OPERATION_TEST(i, div, /, 71778742, 4323)
BINARY_ARITHMETIC_OPERATION_TEST(i, rem, %, 8, 3)
BINARY_ARITHMETIC_OPERATION_TEST(i, shl, <<, 23, 5)
BINARY_ARITHMETIC_OPERATION_TEST(i, shr, >>, 28742567, 7)
BINARY_ARITHMETIC_OPERATION_TEST(i, and, &, 348953897, 2374526)
BINARY_ARITHMETIC_OPERATION_TEST(i, or, |, 348953897, 2374526)
BINARY_ARITHMETIC_OPERATION_TEST(i, xor, ^, 348953897, 2374526)
BINARY_ARITHMETIC_OPERATION_TEST(f, add, +, 345.345f, 234.234234f)
BINARY_ARITHMETIC_OPERATION_TEST(f, sub, -, -23489723.4f, 891286.123f)
BINARY_ARITHMETIC_OPERATION_TEST(f, mul, *, 2323.45f, 128.3487234f)
BINARY_ARITHMETIC_OPERATION_TEST(f, div, /, 7177.8742f, 4.323f)
BINARY_ARITHMETIC_OPERATION_TEST(l, add, +, 345345234L, 234234234234L)
BINARY_ARITHMETIC_OPERATION_TEST(l, sub, -, -23489723445546L, 891286123645L)
BINARY_ARITHMETIC_OPERATION_TEST(l, mul, *, 232345345L, 128348723544564L)
BINARY_ARITHMETIC_OPERATION_TEST(l, div, /, 7177874234L, 43232L)
BINARY_ARITHMETIC_OPERATION_TEST(l, rem, %, 436727828L, 23735L)
BINARY_ARITHMETIC_OPERATION_TEST(l, and, &, 348953897234L, 23745264563345L)
BINARY_ARITHMETIC_OPERATION_TEST(l, or, |, 348953897234L, 23745264563345L)
BINARY_ARITHMETIC_OPERATION_TEST(l, xor, ^, 348953897234L, 23745264563345L)
BINARY_ARITHMETIC_OPERATION_TEST(d, add, +, 345.342345, 223434.234234)
BINARY_ARITHMETIC_OPERATION_TEST(d, sub, -, -23482349723.554, 891282346.123)
BINARY_ARITHMETIC_OPERATION_TEST(d, mul, *, 2323.45, 128.3487232344)
BINARY_ARITHMETIC_OPERATION_TEST(d, div, /, 7177.8743452, 4.3221343)


TEST_F(NanoJavaInterpreterTest, IntegerDivisionOverflow) {
  const std::string test_cases[] = {"idiv", "irem"};
  for (const std::string& test_case : test_cases) {
    const std::string blob = AssembleMethod("I", Substitute(
                                                     R"(.limit stack 2
               ldc -2147483648
               ldc -1
               $0
               ireturn)",
                                                     test_case));

    ASSERT_EQ("<int>-2147483648", ExecuteExpectSuccess(blob, {}));
  }
}


TEST_F(NanoJavaInterpreterTest, LongDivisionOverflow) {
  const std::string test_cases[] = {"ldiv", "lrem"};
  for (const std::string& test_case : test_cases) {
    const std::string blob = AssembleMethod("J", Substitute(
                                                     R"(.limit stack 4
               ldc2_w -9223372036854775808
               ldc2_w -1
               $0
               lreturn)",
                                                     test_case));

    ASSERT_EQ("<long>-9223372036854775808", ExecuteExpectSuccess(blob, {}));
  }
}


TEST_F(NanoJavaInterpreterTest, IntegerDivisionByZero) {
  const std::string test_cases[] = {"idiv", "irem"};
  for (const std::string& test_case : test_cases) {
    const std::string blob = AssembleMethod("I", Substitute(
                                                     R"(.limit stack 2
               ldc 47
               ldc 0
               $0
               ireturn)",
                                                     test_case));

    ASSERT_EQ(
        "java.lang.ArithmeticException",
        ExecuteExpectException(blob, {}));
  }
}


TEST_F(NanoJavaInterpreterTest, LongDivisionByZero) {
  const std::string test_cases[] = {"ldiv", "lrem"};
  for (const std::string& test_case : test_cases) {
    const std::string blob = AssembleMethod("J", Substitute(
                                                     R"(.limit stack 4
               ldc2_w 472345723423432
               ldc2_w 0
               $0
               lreturn)",
                                                     test_case));

    ASSERT_EQ(
        "java.lang.ArithmeticException",
        ExecuteExpectException(blob, {}));
  }
}


// static int test(int x) {
//   return -x;
// }
TEST_F(NanoJavaInterpreterTest, Opcode_ineg) {
  ASSERT_EQ(
      "<int>-212989",
      ExecuteExpectSuccess(
          AssembleMethod(
              "I",
              R"(iload 0
                 ineg
                 ireturn)"),
          { JVariant::Int(212989) }));
}


// static float test(float x) {
//   return -x;
// }
TEST_F(NanoJavaInterpreterTest, Opcode_fneg) {
  ASSERT_EQ(
      "<float>-3.14",
      ExecuteExpectSuccess(
          AssembleMethod(
              "F",
              R"(fload 0
                 fneg
                 freturn)"),
          { JVariant::Float(3.14f) }));
}


// static long test(long x) {
//   return -x;
// }
TEST_F(NanoJavaInterpreterTest, Opcode_lneg) {
  ASSERT_EQ(
      "<long>90347593874",
      ExecuteExpectSuccess(
          AssembleMethod(
              "J",
              R"(.limit stack 2
                 .limit locals 2
                 lload 0
                 lneg
                 lreturn)"),
          { JVariant::Long(-90347593874L) }));
}


// static double test(double x) {
//   return -x;
// }
TEST_F(NanoJavaInterpreterTest, Opcode_dneg) {
  ASSERT_EQ(
      "<double>3.1415",
      ExecuteExpectSuccess(
          AssembleMethod(
              "D",
              R"(.limit stack 2
                 .limit locals 2
                 dload 0
                 dneg
                 dreturn)"),
          { JVariant::Double(-3.1415) }));
}


// static long test() {
//   return 23871L << 11;
// }
TEST_F(NanoJavaInterpreterTest, Opcode_lshl) {
  ASSERT_EQ(
      "<long>" + std::to_string(23871L << 11),
      ExecuteExpectSuccess(
          AssembleMethod(
              "J",
              R"(.limit stack 3
                 ldc2_w 23871
                 ldc 11
                 lshl
                 lreturn)"),
          {}));
}


// static long test() {
//   return 834789537486534L >> 5;
// }
TEST_F(NanoJavaInterpreterTest, Opcode_lshr) {
  ASSERT_EQ(
      "<long>" + std::to_string(834789537486534L >> 5),
      ExecuteExpectSuccess(
          AssembleMethod(
              "J",
              R"(.limit stack 3
                 ldc2_w 834789537486534
                 ldc 5
                 lshr
                 lreturn)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_iushr) {
  ASSERT_EQ(
      "<int>" + std::to_string(0x0FFFFFFF),
      ExecuteExpectSuccess(
          AssembleMethod(
              "I",
              R"(.limit stack 2
                 ldc 0xFFFFFFFF
                 ldc 4
                 iushr
                 ireturn)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_lushr) {
  ASSERT_EQ(
      "<long>" + std::to_string(0x0FFFFFFFFFFFFFFFL),
      ExecuteExpectSuccess(
          AssembleMethod(
              "J",
              R"(.limit stack 3
                 ldc2_w -1
                 ldc 4
                 lushr
                 lreturn)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_lcmp) {
  const std::string blob = AssembleMethod("I",
                                          R"(.limit stack 4
           .limit locals 4
           lload 0
           lload 2
           lcmp
           ireturn)");

  ASSERT_EQ(
      "<int>1",
      ExecuteExpectSuccess(blob, { JVariant::Long(5), JVariant::Long(3) }));

  ASSERT_EQ(
      "<int>-1",
      ExecuteExpectSuccess(blob, { JVariant::Long(5), JVariant::Long(8) }));

  ASSERT_EQ(
      "<int>0",
      ExecuteExpectSuccess(blob, { JVariant::Long(4), JVariant::Long(4) }));
}


TEST_F(NanoJavaInterpreterTest, Opcode_fcmp) {
  const std::string source_code =
      R"(.limit stack 2
         .limit locals 2
         fload 0
         fload 1
         $0
         ireturn)";

  const std::string blobs[] = {
      AssembleMethod("I", Substitute(source_code, "fcmpl")),
      AssembleMethod("I", Substitute(source_code, "fcmpg"))};

  for (const std::string& blob : blobs) {
    ASSERT_EQ(
        "<int>1",
        ExecuteExpectSuccess(
            blob,
            { JVariant::Float(3.0f), JVariant::Float(2.0f) }));

    ASSERT_EQ(
        "<int>-1",
        ExecuteExpectSuccess(
            blob,
            { JVariant::Float(1.0f), JVariant::Float(2.0f) }));

    ASSERT_EQ(
        "<int>0",
        ExecuteExpectSuccess(
            blob,
            { JVariant::Float(1.1f), JVariant::Float(1.1f) }));
  }

  ASSERT_EQ(
      "<int>-1",
      ExecuteExpectSuccess(
          blobs[0],
          { JVariant::Float(nanf("")), JVariant::Float(1.1f) }));

  ASSERT_EQ(
      "<int>-1",
      ExecuteExpectSuccess(
          blobs[0],
          { JVariant::Float(1.1f), JVariant::Float(nanf("")) }));

  ASSERT_EQ(
      "<int>-1",
      ExecuteExpectSuccess(
          blobs[0],
          { JVariant::Float(nanf("")), JVariant::Float(nanf("")) }));

  ASSERT_EQ(
      "<int>1",
      ExecuteExpectSuccess(
          blobs[1],
          { JVariant::Float(nanf("")), JVariant::Float(1.1f) }));

  ASSERT_EQ(
      "<int>1",
      ExecuteExpectSuccess(
          blobs[1],
          { JVariant::Float(1.1f), JVariant::Float(nanf("")) }));

  ASSERT_EQ(
      "<int>1",
      ExecuteExpectSuccess(
          blobs[1],
          { JVariant::Float(nanf("")), JVariant::Float(nanf("")) }));
}


TEST_F(NanoJavaInterpreterTest, Opcode_dcmp) {
  const std::string source_code =
      R"(.limit stack 4
         .limit locals 4
         dload 0
         dload 2
         $0
         ireturn)";

  const std::string blobs[] = {
      AssembleMethod("I", Substitute(source_code, "dcmpl")),
      AssembleMethod("I", Substitute(source_code, "dcmpg"))};

  for (const std::string& blob : blobs) {
    ASSERT_EQ(
        "<int>1",
        ExecuteExpectSuccess(
            blob,
            { JVariant::Double(3.0), JVariant::Double(2.0) }));

    ASSERT_EQ(
        "<int>-1",
        ExecuteExpectSuccess(
            blob,
            { JVariant::Double(1.0), JVariant::Double(2.0) }));

    ASSERT_EQ(
        "<int>0",
        ExecuteExpectSuccess(
            blob,
            { JVariant::Double(1.1), JVariant::Double(1.1) }));
  }

  ASSERT_EQ(
      "<int>-1",
      ExecuteExpectSuccess(
          blobs[0],
          { JVariant::Double(nan("")), JVariant::Double(1.1) }));

  ASSERT_EQ(
      "<int>-1",
      ExecuteExpectSuccess(
          blobs[0],
          { JVariant::Double(1.1), JVariant::Double(nan("")) }));

  ASSERT_EQ(
      "<int>-1",
      ExecuteExpectSuccess(
          blobs[0],
          { JVariant::Double(nan("")), JVariant::Double(nan("")) }));

  ASSERT_EQ(
      "<int>1",
      ExecuteExpectSuccess(
          blobs[1],
          { JVariant::Double(nan("")), JVariant::Double(1.1) }));

  ASSERT_EQ(
      "<int>1",
      ExecuteExpectSuccess(
          blobs[1],
          { JVariant::Double(1.1), JVariant::Double(nan("")) }));

  ASSERT_EQ(
      "<int>1",
      ExecuteExpectSuccess(
          blobs[1],
          { JVariant::Double(nan("")), JVariant::Double(nan("")) }));
}


TEST_F(NanoJavaInterpreterTest, Opcode_if) {
  const struct {
    std::string opcode;
    int32_t argument;
    std::string expected_return_value;
  } test_cases[] {
    { "ifeq", 0, "branched" },
    { "ifeq", 1, "not branched" },
    { "ifeq", -1, "not branched" },
    { "ifne", 0, "not branched" },
    { "ifne", 1, "branched" },
    { "ifne", -1, "branched" },
    { "iflt", -1, "branched" },
    { "iflt", 0, "not branched" },
    { "iflt", 1, "not branched" },
    { "ifle", -1, "branched" },
    { "ifle", 0, "branched" },
    { "ifle", 1, "not branched" },
    { "ifgt", 1, "branched" },
    { "ifgt", 0, "not branched" },
    { "ifgt", -1, "not branched" },
    { "ifge", 1, "branched" },
    { "ifge", 0, "branched" },
    { "ifge", -1, "not branched" }
  };

  for (const auto& test_case : test_cases) {
    const std::string blob =
        AssembleMethod("Ljava/lang/String;", Substitute(
                                                 R"(.limit locals 1
               iload 0
               $0 L
               ldc "not branched"
               areturn
               L:
               ldc "branched"
               areturn)",
                                                 test_case.opcode));

    EXPECT_EQ(
        std::string("<java.lang.String>") + test_case.expected_return_value,
        ExecuteExpectSuccess(blob, {JVariant::Int(test_case.argument)}));
  }
}


TEST_F(NanoJavaInterpreterTest, Opcode_if_icmp) {
  const struct {
    std::string opcode;
    int32_t argument1;
    int32_t argument2;
    std::string expected_return_value;
  } test_cases[] {
    { "if_icmpeq", 1, 2, "not branched" },
    { "if_icmpeq", 2, 2, "branched" },
    { "if_icmpeq", 2, 3, "not branched" },
    { "if_icmpne", 1, 2, "branched" },
    { "if_icmpne", 2, 2, "not branched" },
    { "if_icmpne", 2, 3, "branched" },
    { "if_icmplt", 1, 2, "branched" },
    { "if_icmplt", 2, 2, "not branched" },
    { "if_icmplt", 3, 2, "not branched" },
    { "if_icmple", 1, 2, "branched" },
    { "if_icmple", 2, 2, "branched" },
    { "if_icmple", 3, 2, "not branched" },
    { "if_icmpgt", 1, 2, "not branched" },
    { "if_icmpgt", 2, 2, "not branched" },
    { "if_icmpgt", 3, 2, "branched" },
    { "if_icmpge", 1, 2, "not branched" },
    { "if_icmpge", 2, 2, "branched" },
    { "if_icmpge", 3, 2, "branched" }
  };

  for (const auto& test_case : test_cases) {
    const std::string blob =
        AssembleMethod("Ljava/lang/String;", Substitute(
                                                 R"(.limit stack 2
               .limit locals 2
               iload 0
               iload 1
               $0 L
               ldc "not branched"
               areturn
               L:
               ldc "branched"
               areturn)",
                                                 test_case.opcode));

    EXPECT_EQ(
        std::string("<java.lang.String>") + test_case.expected_return_value,
        ExecuteExpectSuccess(blob, {JVariant::Int(test_case.argument1),
                                    JVariant::Int(test_case.argument2)}))
        << "opcode: " << test_case.opcode
        << ", arguments: " << test_case.argument1 << ", "
        << test_case.argument2;
  }
}


TEST_F(NanoJavaInterpreterTest, Opcode_if_iacmp) {
  JniLocalRef obj1 = jniproxy::Object()->NewObject()
      .Release(ExceptionAction::LOG_AND_IGNORE);
  JniLocalRef obj2 = jniproxy::Object()->NewObject()
      .Release(ExceptionAction::LOG_AND_IGNORE);

  const struct {
    std::string opcode;
    jobject argument1;
    jobject argument2;
    std::string expected_return_value;
  } test_cases[] {
    { "if_acmpeq", nullptr, nullptr, "branched" },
    { "if_acmpeq", obj1.get(), nullptr, "not branched" },
    { "if_acmpeq", nullptr, obj1.get(), "not branched" },
    { "if_acmpeq", obj1.get(), obj1.get(), "branched" },
    { "if_acmpeq", obj1.get(), obj2.get(), "not branched" },
    { "if_acmpne", nullptr, nullptr, "not branched" },
    { "if_acmpne", obj1.get(), nullptr, "branched" },
    { "if_acmpne", nullptr, obj1.get(), "branched" },
    { "if_acmpne", obj1.get(), obj1.get(), "not branched" },
    { "if_acmpne", obj1.get(), obj2.get(), "branched" }
  };

  for (const auto& test_case : test_cases) {
    const std::string blob =
        AssembleMethod("Ljava/lang/String;", Substitute(
                                                 R"(.limit stack 2
               .limit locals 2
               aload 0
               aload 1
               $0 L
               ldc "not branched"
               areturn
               L:
               ldc "branched"
               areturn)",
                                                 test_case.opcode));

    EXPECT_EQ(
        std::string("<java.lang.String>") + test_case.expected_return_value,
        ExecuteExpectSuccess(blob,
                             {
                                 JVariant::BorrowedRef(test_case.argument1),
                                 JVariant::BorrowedRef(test_case.argument2),
                             }));
  }
}


TEST_F(NanoJavaInterpreterTest, Opcode_if_null_nonnull) {
  JniLocalRef obj = jniproxy::Object()->NewObject()
      .Release(ExceptionAction::LOG_AND_IGNORE);

  const struct {
    std::string opcode;
    jobject argument;
    std::string expected_return_value;
  } test_cases[] {
    { "ifnull", nullptr, "branched" },
    { "ifnull", obj.get(), "not branched" },
    { "ifnonnull", nullptr, "not branched" },
    { "ifnonnull", obj.get(), "branched" }
  };

  for (const auto& test_case : test_cases) {
    const std::string blob =
        AssembleMethod("Ljava/lang/String;", Substitute(
                                                 R"(.limit locals 1
               aload 0
               $0 L
               ldc "not branched"
               areturn
               L:
               ldc "branched"
               areturn)",
                                                 test_case.opcode));

    EXPECT_EQ(
        std::string("<java.lang.String>") + test_case.expected_return_value,
        ExecuteExpectSuccess(blob,
                             {
                                 JVariant::BorrowedRef(test_case.argument),
                             }));
  }
}


TEST_F(NanoJavaInterpreterTest, Opcode_goto) {
  ASSERT_EQ(
      "<boolean>true",
      ExecuteExpectSuccess(
          AssembleMethod(
              "Z",
              R"(goto L
                 iconst_0
                 ireturn
                 L:
                 iconst_1
                 ireturn)"),
          {}));
}


// static Object test() {
//   return null;
// }
TEST_F(NanoJavaInterpreterTest, Opcode_aconst_null) {
  ASSERT_EQ(
      "null",
      ExecuteExpectSuccess(
          AssembleMethod(
              "Ljava/lang/Object;",
              R"(aconst_null
                 areturn)"),
          {}));
}


// static int test() {
//   return 358;
// }
TEST_F(NanoJavaInterpreterTest, Opcode_sipush) {
  ASSERT_EQ(
      "<int>358",
      ExecuteExpectSuccess(
          AssembleMethod(
              "I",
              R"(sipush 358
                 ireturn)"),
          {}));
}


// static int test() {
//   return -23;
// }
TEST_F(NanoJavaInterpreterTest, Opcode_bipush) {
  ASSERT_EQ(
      "<int>-23",
      ExecuteExpectSuccess(
          AssembleMethod(
              "I",
              R"(bipush -23
                 ireturn)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_istore) {
  ASSERT_EQ(
      "<int>47",
      ExecuteExpectSuccess(
          AssembleMethod(
              "I",
              R"(.limit locals 1
                 bipush 47
                 istore 0
                 iload 0
                 ireturn)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_fstore) {
  ASSERT_EQ(
      "<float>47",
      ExecuteExpectSuccess(
          AssembleMethod(
              "F",
              R"(.limit locals 1
                 bipush 47
                 i2f
                 fstore 0
                 fload 0
                 freturn)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_lstore) {
  ASSERT_EQ(
      "<long>47",
      ExecuteExpectSuccess(
          AssembleMethod(
              "J",
              R"(.limit stack 2
                 .limit locals 2
                 bipush 47
                 i2l
                 lstore 0
                 lload 0
                 lreturn)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_dstore) {
  ASSERT_EQ(
      "<double>47",
      ExecuteExpectSuccess(
          AssembleMethod(
              "D",
              R"(.limit stack 2
                 .limit locals 2
                 bipush 47
                 i2d
                 dstore 0
                 dload 0
                 dreturn)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_astore) {
  ASSERT_EQ(
      "<java.lang.String>hello",
      ExecuteExpectSuccess(
          AssembleMethod(
              "Ljava/lang/String;",
              R"(.limit locals 2
                 ldc "hello"
                 astore 0
                 aload 0
                 areturn)"),
          {}));
}


// static int test() {
//   return 123456;
// }
TEST_F(NanoJavaInterpreterTest, Opcode_ldc_Int) {
  ASSERT_EQ(
      "<int>123456",
      ExecuteExpectSuccess(
          AssembleMethod(
              "I",
              R"(ldc 123456
                 ireturn)"),
          {}));
}


// static float test() {
//   return 123.456f;
// }
TEST_F(NanoJavaInterpreterTest, Opcode_ldc_Float) {
  ASSERT_EQ(
      "<float>123.456",
      ExecuteExpectSuccess(
          AssembleMethod(
              "F",
              R"(ldc 123.456
                 freturn)"),
          {}));
}


// static long test() {
//   return 1234567890l;
// }
TEST_F(NanoJavaInterpreterTest, Opcode_ldc_Long) {
  ASSERT_EQ(
      "<long>1234567890",
      ExecuteExpectSuccess(
          AssembleMethod(
              "J",
              R"(.limit stack 2
                 ldc2_w 1234567890
                 lreturn)"),
          {}));
}


// static double test() {
//   return 123456.789;
// }
TEST_F(NanoJavaInterpreterTest, Opcode_ldc_Double) {
  ASSERT_EQ(
      "<double>123456.789",
      ExecuteExpectSuccess(
          AssembleMethod(
              "D",
              R"(.limit stack 2
                 ldc2_w 123456.789
                 dreturn)"),
          {}));
}


// static String test() {
//   return "hello";
// }
TEST_F(NanoJavaInterpreterTest, Opcode_ldc_String) {
  ASSERT_EQ(
      "<java.lang.String>hello",
      ExecuteExpectSuccess(
          AssembleMethod(
              "Ljava/lang/String;",
              R"(.limit stack 1
                 ldc "hello"
                 areturn)"),
          {}));
}


// static Class<?> test() {
//   return Exception.class;
// }
TEST_F(NanoJavaInterpreterTest, Opcode_ldc_ObjectClass) {
  ASSERT_EQ(
      "<java.lang.Class>class java.lang.Exception",
      ExecuteExpectSuccess(
          AssembleMethod(
              "Ljava/lang/Class;",
              R"(ldc java/lang/Exception
                 areturn)"),
          {}));
}


// public static int test(int x) {
//   x += 57;
//   return x;
// }
TEST_F(NanoJavaInterpreterTest, Opcode_iinc) {
  ASSERT_EQ(
      "<int>99",
      ExecuteExpectSuccess(
          AssembleMethod(
              "I",
              R"(iinc 0 57
                 iload 0
                 ireturn)"),
          { JVariant::Int(42) }));
}


// static int test(int x) {
//   return x * x;
// }
TEST_F(NanoJavaInterpreterTest, Opcode_dup) {
  ASSERT_EQ(
      "<int>64",
      ExecuteExpectSuccess(
          AssembleMethod(
              "I",
              R"(.limit stack 2
                 iload 0
                 dup
                 imul
                 ireturn)"),
          { JVariant::Int(8) }));
}


TEST_F(NanoJavaInterpreterTest, Opcode_dup_x1) {
  ASSERT_EQ(
      "<int>12",
      ExecuteExpectSuccess(
          AssembleMethod(
              "I",
              R"(.limit stack 3
                 .limit locals 2
                 iload 0
                 iload 1
                 dup_x1
                 isub
                 isub
                 ireturn)"),
          { JVariant::Int(4), JVariant::Int(8) }));
}


TEST_F(NanoJavaInterpreterTest, Opcode_dup_x2) {
  ASSERT_EQ(
      "<int>-12",
      ExecuteExpectSuccess(
          AssembleMethod(
              "I",
              R"(.limit stack 4
                 .limit locals 3
                 iload 0
                 iload 1
                 iload 2
                 dup_x2
                 iadd
                 iadd
                 isub
                 ireturn)"),
          { JVariant::Int(4), JVariant::Int(8), JVariant::Int(100) }));
}


// static double test(double x) {
//   return x * x;
// }
TEST_F(NanoJavaInterpreterTest, Opcode_dup2_Double) {
  ASSERT_EQ(
      "<double>2085.7489",
      ExecuteExpectSuccess(
          AssembleMethod(
              "D",
              R"(.limit stack 4
                 .limit locals 2
                 dload 0
                 dup2
                 dmul
                 dreturn)"),
          { JVariant::Double(45.67) }));
}


// static double test(String s) {
//   return s.concat(s).concat(s).concat(s);
// }
TEST_F(NanoJavaInterpreterTest, Opcode_dup2_Object) {
  ASSERT_EQ(
      "<java.lang.String>abcabcabcabc",
      ExecuteExpectSuccess(
          AssembleMethod(
              "Ljava/lang/Object;",
              R"(.limit stack 4
                 aload 0
                 aload 0
                 dup2
                 invokevirtual java/lang/String/concat(Ljava/lang/String;)Ljava/lang/String;
                 invokevirtual java/lang/String/concat(Ljava/lang/String;)Ljava/lang/String;
                 invokevirtual java/lang/String/concat(Ljava/lang/String;)Ljava/lang/String;
                 areturn)"),
          { JVariant::LocalRef(JniToJavaString("abc")) }));
}


TEST_F(NanoJavaInterpreterTest, Opcode_dup2_x1) {
  ASSERT_EQ(
      "<int>-7",
      ExecuteExpectSuccess(
          AssembleMethod(
              "I",
              R"(.limit stack 5
                 iconst_1
                 iconst_2
                 iconst_4
                 dup2_x1
                 iadd
                 isub
                 isub
                 isub
                 ireturn)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_dup2_x2) {
  constexpr int64_t n1 = 347856378464L;
  constexpr int64_t n2 = 89435862334L;
  ASSERT_EQ(
      "<long>" + std::to_string(n2 - (n1 - n2)),
      ExecuteExpectSuccess(
          AssembleMethod(
              "J",
              R"(.limit stack 6
                 .limit locals 4
                 lload 0
                 lload 2
                 dup2_x2
                 lsub
                 lsub
                 lreturn)"),
          { JVariant::Long(n1), JVariant::Long(n2) }));
}


TEST_F(NanoJavaInterpreterTest, Opcode_pop) {
  ASSERT_EQ(
      "<int>1",
      ExecuteExpectSuccess(
          AssembleMethod(
              "I",
              R"(.limit stack 2
                 iconst_1
                 iconst_2
                 pop
                 ireturn)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_pop2) {
  ASSERT_EQ(
      "<int>1",
      ExecuteExpectSuccess(
          AssembleMethod(
              "I",
              R"(.limit stack 3
                 iconst_1
                 iconst_2
                 iconst_3
                 pop2
                 ireturn)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_swap) {
  ASSERT_EQ(
      "<int>1",
      ExecuteExpectSuccess(
          AssembleMethod(
              "I",
              R"(.limit stack 2
                 iconst_1
                 iconst_2
                 swap
                 isub
                 ireturn)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_i2l) {
  ASSERT_EQ(
      "<long>1",
      ExecuteExpectSuccess(
          AssembleMethod(
              "J",
              R"(.limit stack 2
                 iconst_1
                 i2l
                 lreturn)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_i2f) {
  ASSERT_EQ(
      "<float>1",
      ExecuteExpectSuccess(
          AssembleMethod(
              "F",
              R"(iconst_1
                 i2f
                 freturn)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_i2d) {
  ASSERT_EQ(
      "<double>1",
      ExecuteExpectSuccess(
          AssembleMethod(
              "D",
              R"(.limit stack 2
                 iconst_1
                 i2d
                 dreturn)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_l2i) {
  ASSERT_EQ(
      "<int>12345",
      ExecuteExpectSuccess(
          AssembleMethod(
              "I",
              R"(.limit stack 2
                 .limit locals 2
                 lload 0
                 l2i
                 ireturn)"),
          { JVariant::Long(12345) }));
}


TEST_F(NanoJavaInterpreterTest, Opcode_l2f) {
  ASSERT_EQ(
      "<float>12345",
      ExecuteExpectSuccess(
          AssembleMethod(
              "F",
              R"(.limit stack 2
                 .limit locals 2
                 lload 0
                 l2f
                 freturn)"),
          { JVariant::Long(12345) }));
}


TEST_F(NanoJavaInterpreterTest, Opcode_l2d) {
  ASSERT_EQ(
      "<double>12345",
      ExecuteExpectSuccess(
          AssembleMethod(
              "D",
              R"(.limit stack 2
                 .limit locals 2
                 lload 0
                 l2d
                 dreturn)"),
          { JVariant::Long(12345) }));
}


TEST_F(NanoJavaInterpreterTest, Opcode_f2i) {
  ASSERT_EQ(
      "<int>3",
      ExecuteExpectSuccess(
          AssembleMethod(
              "I",
              R"(fload 0
                 f2i
                 ireturn)"),
          { JVariant::Float(3.14f) }));
}


TEST_F(NanoJavaInterpreterTest, Opcode_f2l) {
  ASSERT_EQ(
      "<long>3",
      ExecuteExpectSuccess(
          AssembleMethod(
              "J",
              R"(.limit stack 2
                 fload 0
                 f2l
                 lreturn)"),
          { JVariant::Float(3.14f) }));
}


TEST_F(NanoJavaInterpreterTest, Opcode_f2d) {
  // Conversion from float to double introduces noise like "3.140000105".
  EXPECT_THAT(
      ExecuteExpectSuccess(
          AssembleMethod(
              "D",
              R"(.limit stack 2
                 fload 0
                 f2d
                 dreturn)"),
          { JVariant::Float(3.14f) }),
      MatchesRegex("^<double>3\\.14[0-9]+"));
}


TEST_F(NanoJavaInterpreterTest, Opcode_d2i) {
  ASSERT_EQ(
      "<int>3",
      ExecuteExpectSuccess(
          AssembleMethod(
              "I",
              R"(.limit stack 2
                 .limit locals 2
                 dload 0
                 d2i
                 ireturn)"),
          { JVariant::Double(3.14) }));
}


TEST_F(NanoJavaInterpreterTest, Opcode_d2l) {
  ASSERT_EQ(
      "<long>3",
      ExecuteExpectSuccess(
          AssembleMethod(
              "J",
              R"(.limit stack 2
                 .limit locals 2
                 dload 0
                 d2l
                 lreturn)"),
          { JVariant::Double(3.14) }));
}


TEST_F(NanoJavaInterpreterTest, Opcode_d2f) {
  ASSERT_EQ(
      "<float>3.14",
      ExecuteExpectSuccess(
          AssembleMethod(
              "F",
              R"(.limit stack 2
                 .limit locals 2
                 dload 0
                 d2f
                 freturn)"),
          { JVariant::Double(3.14) }));
}


TEST_F(NanoJavaInterpreterTest, Opcode_i2b) {
  ASSERT_EQ(
      "<byte>23",
      ExecuteExpectSuccess(
          AssembleMethod(
              "B",
              R"(iload 0
                 i2b
                 ireturn)"),
          { JVariant::Int(0xFFFFF00 + 23) }));
}


TEST_F(NanoJavaInterpreterTest, Opcode_i2c) {
  ASSERT_EQ(
      "<char>2345",
      ExecuteExpectSuccess(
          AssembleMethod(
              "C",
              R"(iload 0
                 i2c
                 ireturn)"),
          { JVariant::Int(0xFFF0000 + 2345) }));
}


TEST_F(NanoJavaInterpreterTest, Opcode_i2s) {
  ASSERT_EQ(
      "<short>-42",
      ExecuteExpectSuccess(
          AssembleMethod(
              "S",
              R"(iload 0
                 i2s
                 ireturn)"),
          { JVariant::Int(-42) }));
}


// public static String test(String what) {
//   return "hello ".concat(what);
// }
TEST_F(NanoJavaInterpreterTest, Opcode_ldc_invokevirtual_jni) {
  ASSERT_EQ(
      "<java.lang.String>hello world",
      ExecuteExpectSuccess(
          AssembleMethod(
              "Ljava/lang/String;",
              R"(.limit stack 2
                 ldc "hello "
                 aload 0
                 invokevirtual java/lang/String/concat(Ljava/lang/String;)Ljava/lang/String;
                 areturn)"),
          { JVariant::LocalRef(JniToJavaString("world")) }));
}


// Same as "Opcode_ldc_invokevirtual_jni", but uses INVOKEINTERFACE opcode.
TEST_F(NanoJavaInterpreterTest, Opcode_ldc_invokeinterface_jni) {
  ASSERT_EQ(
      "<java.lang.String>hello world",
      ExecuteExpectSuccess(
          AssembleMethod(
              "Ljava/lang/String;",
              R"(.limit stack 2
                 ldc "hello "
                 aload 0
                 invokeinterface java/lang/String/concat(Ljava/lang/String;)Ljava/lang/String; 1
                 areturn)"),
          { JVariant::LocalRef(JniToJavaString("world")) }));
}


// public static String test() {
//   return String.valueOf(73);
// }
TEST_F(NanoJavaInterpreterTest, Opcode_ldc_invokestatic_jni) {
  ASSERT_EQ(
      "<java.lang.String>73",
      ExecuteExpectSuccess(
          AssembleMethod(
              "Ljava/lang/String;",
              R"(ldc 73
                 invokestatic java/lang/String/valueOf(I)Ljava/lang/String;
                 areturn)"),
          { JVariant::LocalRef(JniToJavaString("world")) }));
}


TEST_F(NanoJavaInterpreterTest, Opcode_ldc_invokespecial_jni) {
  JniLocalRef obj = jniproxy::Exception()->NewObject("message")
      .Release(ExceptionAction::LOG_AND_IGNORE);

  EXPECT_THAT(
      ExecuteExpectSuccess(
          AssembleMethod(
              "Ljava/lang/Object;",
              R"(aload 0
                 invokespecial java/lang/Object/toString()Ljava/lang/String;
                 areturn)"),
          { JVariant::LocalRef(std::move(obj)) }),
      MatchesRegex("^<java\\.lang\\.String>java\\.lang\\.Exception@[0-9a-f]+"));
}


// public static Object test(String message) {
//   return new Exception(message);
// }
TEST_F(NanoJavaInterpreterTest, Opcode_new_Object) {
  ASSERT_EQ(
      "<java.lang.Exception>java.lang.Exception: this-is-me",
      ExecuteExpectSuccess(
          AssembleMethod(
              "Ljava/lang/Throwable;",
              R"(.limit stack 3
                 new java/lang/Exception
                 dup
                 aload 0
                 invokespecial java/lang/Exception/<init>(Ljava/lang/String;)V
                 areturn)"),
          { JVariant::LocalRef(JniToJavaString("this-is-me")) }));
}


// public static boolean test() {
//   return "hello" instanceof Object;
// }
TEST_F(NanoJavaInterpreterTest, Opcode_instanceof) {
  ASSERT_EQ(
      "<boolean>true",
      ExecuteExpectSuccess(
          AssembleMethod(
              "Z",
              R"(.limit locals 0
                 ldc "hello"
                 instanceof java/lang/Object
                 ireturn)"),
          {}));
}


// public static Object test() {
//   return (Object)"hello";
// }
TEST_F(NanoJavaInterpreterTest, Opcode_checkcast_Success) {
  ASSERT_EQ(
      "<java.lang.String>hello",
      ExecuteExpectSuccess(
          AssembleMethod(
              "Ljava/lang/Object;",
              R"(.limit locals 0
                 ldc "hello"
                 checkcast java/lang/Object
                 areturn)"),
          {}));
}


// public static Class test() {
//   return (Class)"hello";
// }
TEST_F(NanoJavaInterpreterTest, Opcode_checkcast_Exception) {
  ASSERT_EQ(
      "java.lang.ClassCastException",
      ExecuteExpectException(
          AssembleMethod(
              "Ljava/lang/Class;",
              R"(.limit locals 0
                 ldc "hello"
                 checkcast java/lang/Class
                 areturn)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_getstatic_Boolean) {
  ASSERT_EQ(
      "<boolean>true",
      ExecuteExpectSuccess(
          AssembleMethod(
              "Z",
              R"(getstatic com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$StaticFields/booleanStaticField Z
                 ireturn)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_getstatic_Byte) {
  ASSERT_EQ(
      "<byte>123",
      ExecuteExpectSuccess(
          AssembleMethod(
              "B",
              R"(getstatic com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$StaticFields/byteStaticField B
                 ireturn)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_getstatic_Char) {
  ASSERT_EQ(
      "<char>1234",
      ExecuteExpectSuccess(
          AssembleMethod(
              "C",
              R"(getstatic com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$StaticFields/charStaticField C
                 ireturn)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_getstatic_Short) {
  ASSERT_EQ(
      "<short>-12345",
      ExecuteExpectSuccess(
          AssembleMethod(
              "S",
              R"(getstatic com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$StaticFields/shortStaticField S
                 ireturn)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_getstatic_Int) {
  ASSERT_EQ(
      "<int>1234567",
      ExecuteExpectSuccess(
          AssembleMethod(
              "I",
              R"(getstatic com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$StaticFields/intStaticField I
                 ireturn)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_getstatic_Long) {
  ASSERT_EQ(
      "<long>1234567890",
      ExecuteExpectSuccess(
          AssembleMethod(
              "J",
              R"(.limit stack 2
                 getstatic com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$StaticFields/longStaticField J
                 lreturn)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_getstatic_Float) {
  ASSERT_EQ(
      "<float>3.14",
      ExecuteExpectSuccess(
          AssembleMethod(
              "F",
              R"(getstatic com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$StaticFields/floatStaticField F
                 freturn)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_getstatic_Double) {
  ASSERT_EQ(
      "<double>3.1415",
      ExecuteExpectSuccess(
          AssembleMethod(
              "D",
              R"(.limit stack 2
                 getstatic com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$StaticFields/doubleStaticField D
                 dreturn)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_getstatic_String) {
  ASSERT_EQ(
      "<java.lang.String>hello static",
      ExecuteExpectSuccess(
          AssembleMethod(
              "Ljava/lang/String;",
              R"(getstatic com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$StaticFields/stringStaticField Ljava/lang/String;
                 areturn)"),
          {}));
}

static std::string BuildGetPutFieldTestClass(const std::string& field_name,
                                             const JSignature& field_signature,
                                             const std::string& value) {
  const bool is_double_slot = ((field_signature.type == JType::Long) ||
                               (field_signature.type == JType::Double));

  std::string return_opcode;
  switch (field_signature.type) {
    case JType::Float:
      return_opcode = "freturn";
      break;

    case JType::Long:
      return_opcode = "lreturn";
      break;

    case JType::Double:
      return_opcode = "dreturn";
      break;

    case JType::Object:
      return_opcode = "areturn";
      break;

    default:
      return_opcode = "ireturn";
      break;
  }

  return AssembleMethod(
      SignatureFromJSignature(field_signature),
      Substitute(
          R"(.limit stack $0
             new com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$$InstanceFields
             dup
             dup
             invokespecial com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$$InstanceFields/<init>()V
             $1 $2
             putfield com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$$InstanceFields/$3 $4
             getfield com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$$InstanceFields/$3 $4
             $5)",
          std::to_string(is_double_slot ? 4 : 3), is_double_slot ? "ldc2_w" : "ldc", value,
          field_name, SignatureFromJSignature(field_signature), return_opcode));
}

TEST_F(NanoJavaInterpreterTest, Opcode_getfield_putfield_Boolean) {
  ASSERT_EQ(
      "<boolean>true",
      ExecuteExpectSuccess(
          BuildGetPutFieldTestClass(
              "booleanInstanceField",
              { JType::Boolean },
              "1"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_getfield_putfield_Byte) {
  ASSERT_EQ(
      "<byte>-78",
      ExecuteExpectSuccess(
          BuildGetPutFieldTestClass(
              "byteInstanceField",
              { JType::Byte },
              "-78"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_getfield_putfield_Char) {
  ASSERT_EQ(
      "<char>45678",
      ExecuteExpectSuccess(
          BuildGetPutFieldTestClass(
              "charInstanceField",
              { JType::Char },
              "45678"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_getfield_putfield_Short) {
  ASSERT_EQ(
      "<short>12345",
      ExecuteExpectSuccess(
          BuildGetPutFieldTestClass(
              "shortInstanceField",
              { JType::Short },
              "12345"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_getfield_putfield_Int) {
  ASSERT_EQ(
      "<int>74865347",
      ExecuteExpectSuccess(
          BuildGetPutFieldTestClass(
              "intInstanceField",
              { JType::Int },
              "74865347"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_getfield_putfield_Long) {
  ASSERT_EQ(
      "<long>748653474354343",
      ExecuteExpectSuccess(
          BuildGetPutFieldTestClass(
              "longInstanceField",
              { JType::Long },
              "748653474354343"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_getfield_putfield_Float) {
  ASSERT_EQ(
      "<float>3.14",
      ExecuteExpectSuccess(
          BuildGetPutFieldTestClass(
              "floatInstanceField",
              { JType::Float },
              "3.14"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_getfield_putfield_Double) {
  ASSERT_EQ(
      "<double>2.86",
      ExecuteExpectSuccess(
          BuildGetPutFieldTestClass(
              "doubleInstanceField",
              { JType::Double },
              "2.86"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, Opcode_getfield_putfield_String) {
  ASSERT_EQ(
      "<java.lang.String>octopus",
      ExecuteExpectSuccess(
          BuildGetPutFieldTestClass(
              "stringInstanceField",
              { JType::Object, "Ljava/lang/String;" },
              "\"octopus\""),
          {}));
}


// public static boolean[] test() {
//   return new boolean[] { true, false };
// }
TEST_F(NanoJavaInterpreterTest, Opcode_newarray_bastore_Boolean) {
  ASSERT_EQ(
      "<boolean[]>[true, false]",
      ExecuteExpectSuccess(
          AssembleMethod(
              "[Z",
              R"(.limit stack 4
                 iconst_2
                 newarray boolean
                 dup
                 iconst_0
                 iconst_1
                 bastore
                 dup
                 iconst_1
                 iconst_0
                 bastore
                 areturn)"),
          {}));
}


// public static byte[] test() {
//   return new byte[] { -78, 123 };
// }
TEST_F(NanoJavaInterpreterTest, Opcode_newarray_bastore_Byte) {
  ASSERT_EQ(
      "<byte[]>[-78, 123]",
      ExecuteExpectSuccess(
          AssembleMethod(
              "[B",
              R"(.limit stack 4
                 iconst_2
                 newarray byte
                 dup
                 iconst_0
                 ldc -78
                 bastore
                 dup
                 iconst_1
                 ldc 123
                 bastore
                 areturn)"),
          {}));
}


// public static char[] test() {
//   return new char[] { 'A', 'B' };
// }
TEST_F(NanoJavaInterpreterTest, Opcode_newarray_castore_Char) {
  ASSERT_EQ(
      "<char[]>[A, B]",
      ExecuteExpectSuccess(
          AssembleMethod(
              "[C",
              R"(.limit stack 4
                 iconst_2
                 newarray char
                 dup
                 iconst_0
                 ldc 65
                 castore
                 dup
                 iconst_1
                 ldc 66
                 castore
                 areturn)"),
          {}));
}


// public static short[] test() {
//   return new short[] { -12345, 23456 };
// }
TEST_F(NanoJavaInterpreterTest, Opcode_newarray_sastore_Short) {
  ASSERT_EQ(
      "<short[]>[-12345, 23456]",
      ExecuteExpectSuccess(
          AssembleMethod(
              "[S",
              R"(.limit stack 4
                 iconst_2
                 newarray short
                 dup
                 iconst_0
                 ldc -12345
                 sastore
                 dup
                 iconst_1
                 ldc 23456
                 sastore
                 areturn)"),
          {}));
}


// public static short[] test() {
//   return new short[] { -1234567, 2345678 };
// }
TEST_F(NanoJavaInterpreterTest, Opcode_newarray_iastore_Int) {
  ASSERT_EQ(
      "<int[]>[-1234567, 2345678]",
      ExecuteExpectSuccess(
          AssembleMethod(
              "[I",
              R"(.limit stack 4
                 iconst_2
                 newarray int
                 dup
                 iconst_0
                 ldc -1234567
                 iastore
                 dup
                 iconst_1
                 ldc 2345678
                 iastore
                 areturn)"),
          {}));
}


// public static long[] test() {
//   return new long[] { 34788734543233L, -893458578345L };
// }
TEST_F(NanoJavaInterpreterTest, Opcode_newarray_lastore_Long) {
  ASSERT_EQ(
      "<long[]>[34788734543233, -893458578345]",
      ExecuteExpectSuccess(
          AssembleMethod(
              "[J",
              R"(.limit stack 5
                 iconst_2
                 newarray long
                 dup
                 iconst_0
                 ldc2_w 34788734543233
                 lastore
                 dup
                 iconst_1
                 ldc2_w -893458578345
                 lastore
                 areturn)"),
          {}));
}


// public static float[] test() {
//   return new float[] { 1.1f, 2.2f };
// }
TEST_F(NanoJavaInterpreterTest, Opcode_newarray_fastore_Float) {
  ASSERT_EQ(
      "<float[]>[1.1, 2.2]",
      ExecuteExpectSuccess(
          AssembleMethod(
              "[F",
              R"(.limit stack 4
                 iconst_2
                 newarray float
                 dup
                 iconst_0
                 ldc 1.1
                 fastore
                 dup
                 iconst_1
                 ldc 2.2
                 fastore
                 areturn)"),
          {}));
}


// public static double[] test() {
//   return new double[] { 1.11, 2.22 };
// }
TEST_F(NanoJavaInterpreterTest, Opcode_newarray_fastore_Double) {
  ASSERT_EQ(
      "<double[]>[1.11, 2.22]",
      ExecuteExpectSuccess(
          AssembleMethod(
              "[D",
              R"(.limit stack 5
                 iconst_2
                 newarray double
                 dup
                 iconst_0
                 ldc2_w 1.11
                 dastore
                 dup
                 iconst_1
                 ldc2_w 2.22
                 dastore
                 areturn)"),
          {}));
}


// public static String[] test() {
//   return new String[] { "first", "second" };
// }
TEST_F(NanoJavaInterpreterTest, Opcode_newarray_aastore_String) {
  ASSERT_EQ(
      "<java.lang.String[]>[first, second]",
      ExecuteExpectSuccess(
          AssembleMethod(
              "[Ljava/lang/String;",
              R"(.limit stack 4
                 iconst_2
                 anewarray java/lang/String
                 dup
                 iconst_0
                 ldc "first"
                 aastore
                 dup
                 iconst_1
                 ldc "second"
                 aastore
                 areturn)"),
          {}));
}


// public static boolean test(boolean[] array) {
//   return array[1];
// }
TEST_F(NanoJavaInterpreterTest, Opcode_baload_Boolean) {
  const jboolean content[] = { false, true };
  JniLocalRef array(jni()->NewBooleanArray(arraysize(content)));
  jni()->SetBooleanArrayRegion(
      static_cast<jbooleanArray>(array.get()),
      0,
      arraysize(content),
      content);

  ASSERT_EQ(
      "<boolean>true",
      ExecuteExpectSuccess(
          AssembleMethod(
              "Z",
              R"(.limit stack 2
                 aload 0
                 iconst_1
                 baload
                 ireturn)"),
          { JVariant::BorrowedRef(array.get()) }));
}


// public static byte test(byte[] array) {
//   return array[1];
// }
TEST_F(NanoJavaInterpreterTest, Opcode_baload_Byte) {
  const jbyte content[] = { -123, 87 };
  JniLocalRef array(jni()->NewByteArray(arraysize(content)));
  jni()->SetByteArrayRegion(
      static_cast<jbyteArray>(array.get()),
      0,
      arraysize(content),
      content);

  ASSERT_EQ(
      "<byte>87",
      ExecuteExpectSuccess(
          AssembleMethod(
              "B",
              R"(.limit stack 2
                 aload 0
                 iconst_1
                 baload
                 ireturn)"),
          { JVariant::BorrowedRef(array.get()) }));
}


// public static char test(char[] array) {
//   return array[1];
// }
TEST_F(NanoJavaInterpreterTest, Opcode_caload_Char) {
  const jchar content[] = { 1, 12345 };
  JniLocalRef array(jni()->NewCharArray(arraysize(content)));
  jni()->SetCharArrayRegion(
      static_cast<jcharArray>(array.get()),
      0,
      arraysize(content),
      content);

  ASSERT_EQ(
      "<char>12345",
      ExecuteExpectSuccess(
          AssembleMethod(
              "C",
              R"(.limit stack 2
                 aload 0
                 iconst_1
                 caload
                 ireturn)"),
          { JVariant::BorrowedRef(array.get()) }));
}


// public static short test(short[] array) {
//   return array[1];
// }
TEST_F(NanoJavaInterpreterTest, Opcode_saload_Short) {
  const jshort content[] = { 1, -12345 };
  JniLocalRef array(jni()->NewShortArray(arraysize(content)));
  jni()->SetShortArrayRegion(
      static_cast<jshortArray>(array.get()),
      0,
      arraysize(content),
      content);

  ASSERT_EQ(
      "<short>-12345",
      ExecuteExpectSuccess(
          AssembleMethod(
              "S",
              R"(.limit stack 2
                 aload 0
                 iconst_1
                 saload
                 ireturn)"),
          { JVariant::BorrowedRef(array.get()) }));
}


// public static int test(int[] array) {
//   return array[1];
// }
TEST_F(NanoJavaInterpreterTest, Opcode_iaload_Int) {
  const jint content[] = { 1, 34785633 };
  JniLocalRef array(jni()->NewIntArray(arraysize(content)));
  jni()->SetIntArrayRegion(
      static_cast<jintArray>(array.get()),
      0,
      arraysize(content),
      content);

  ASSERT_EQ(
      "<int>34785633",
      ExecuteExpectSuccess(
          AssembleMethod(
              "I",
              R"(.limit stack 2
                 aload 0
                 iconst_1
                 iaload
                 ireturn)"),
          { JVariant::BorrowedRef(array.get()) }));
}


// public static long test(long[] array) {
//   return array[1];
// }
TEST_F(NanoJavaInterpreterTest, Opcode_laload_Long) {
  const jlong content[] = { 1, 34785633345345L };
  JniLocalRef array(jni()->NewLongArray(arraysize(content)));
  jni()->SetLongArrayRegion(
      static_cast<jlongArray>(array.get()),
      0,
      arraysize(content),
      content);

  ASSERT_EQ(
      "<long>34785633345345",
      ExecuteExpectSuccess(
          AssembleMethod(
              "J",
              R"(.limit stack 3
                 aload 0
                 iconst_1
                 laload
                 lreturn)"),
          { JVariant::BorrowedRef(array.get()) }));
}


// public static float test(float[] array) {
//   return array[1];
// }
TEST_F(NanoJavaInterpreterTest, Opcode_faload_Float) {
  const jfloat content[] = { 1, 1.1f };
  JniLocalRef array(jni()->NewFloatArray(arraysize(content)));
  jni()->SetFloatArrayRegion(
      static_cast<jfloatArray>(array.get()),
      0,
      arraysize(content),
      content);

  ASSERT_EQ(
      "<float>1.1",
      ExecuteExpectSuccess(
          AssembleMethod(
              "F",
              R"(.limit stack 2
                 aload 0
                 iconst_1
                 faload
                 freturn)"),
          { JVariant::BorrowedRef(array.get()) }));
}


// public static double test(double[] array) {
//   return array[1];
// }
TEST_F(NanoJavaInterpreterTest, Opcode_daload_Double) {
  const jdouble content[] = { 1, 2.22 };
  JniLocalRef array(jni()->NewDoubleArray(arraysize(content)));
  jni()->SetDoubleArrayRegion(
      static_cast<jdoubleArray>(array.get()),
      0,
      arraysize(content),
      content);

  ASSERT_EQ(
      "<double>2.22",
      ExecuteExpectSuccess(
          AssembleMethod(
              "D",
              R"(.limit stack 3
                 aload 0
                 iconst_1
                 daload
                 dreturn)"),
          { JVariant::BorrowedRef(array.get()) }));
}


// public static Object test(Object[] array) {
//   return array[1];
// }
TEST_F(NanoJavaInterpreterTest, Opcode_aaload_String) {
  JniLocalRef array(
      jni()->NewObjectArray(2, jniproxy::String()->GetClass(), nullptr));
  jni()->SetObjectArrayElement(
      static_cast<jobjectArray>(array.get()),
      1,
      JniToJavaString("hello").get());

  ASSERT_EQ(
      "<java.lang.String>hello",
      ExecuteExpectSuccess(
          AssembleMethod(
              "Ljava/lang/String;",
              R"(.limit stack 2
                 aload 0
                 iconst_1
                 aaload
                 areturn)"),
          { JVariant::BorrowedRef(array.get()) }));
}


// public static int test() {
//   return new double[87].length;
// }
TEST_F(NanoJavaInterpreterTest, Opcode_arraylength) {
  ASSERT_EQ(
      "<int>87",
      ExecuteExpectSuccess(
          AssembleMethod(
              "I",
              R"(ldc 87
                 newarray double
                 arraylength
                 ireturn)"),
          {}));
}


// public static int test() {
//   throw new NoSuchElementException("not real one");
// }
TEST_F(NanoJavaInterpreterTest, Opcode_athrow) {
  ASSERT_EQ(
      "java.util.NoSuchElementException: not real one",
      ExecuteExpectException(
          AssembleMethod(
              "V",
              R"(.limit stack 3
                 new java/util/NoSuchElementException
                 dup
                 ldc "not real one"
                 invokespecial java/util/NoSuchElementException/<init>(Ljava/lang/String;)V
                 athrow
                 return)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, CatchException) {
  const struct {
    std::string type;
    bool expect_catch;
  } test_cases[] = {
      { "all", true },
      { "java/util/NoSuchElementException", true },
      { "java/lang/RuntimeException", true },
      { "java/lang/Exception", true },
      { "java/lang/Throwable", true },
      { "java/net/URISyntaxException", false },
      { "java/lang/ReflectiveOperationException", false }
  };

  for (const auto& test_case : test_cases) {
    const std::string blob = AssembleMethod("I", Substitute(
                                                     R"(.limit stack 3
               .catch $0 from TRY to CATCH using CATCH
               new java/util/NoSuchElementException
               dup
               ldc "not real one"
               invokespecial java/util/NoSuchElementException/<init>(Ljava/lang/String;)V
               TRY:
               athrow
               CATCH:
               iconst_3
               ireturn)",
                                                     test_case.type));

    if (test_case.expect_catch) {
      ASSERT_EQ("<int>3", ExecuteExpectSuccess(blob, {}))
          << "Type: " << test_case.type << " (expect to catch an exception)";
    } else {
      ASSERT_EQ("java.util.NoSuchElementException: not real one",
                ExecuteExpectException(blob, {}))
          << "Type: " << test_case.type << " (should not catch an exception)";
    }
  }
}


// public static int test(int x) {
//   switch (x) {
//     case 55:
//       return 1;
//
//     case 56:
//       return 2;
//
//     case 57:
//       return 3;
//   }
//
//   return -1;
// }
TEST_F(NanoJavaInterpreterTest, Opcode_tableswitch) {
  const std::string blob = AssembleMethod("I",
                                          R"(iload_0
         tableswitch 55
           L55
           L56
           L57
           default: LDEFAULT
         L55:
         iconst_1
         ireturn
         L56:
         iconst_2
         ireturn
         L57:
         iconst_3
         ireturn
         LDEFAULT:
         iconst_m1
         ireturn)");

  EXPECT_EQ("<int>-1", ExecuteExpectSuccess(blob, { JVariant::Int(3) }));
  EXPECT_EQ("<int>-1", ExecuteExpectSuccess(blob, { JVariant::Int(54) }));
  EXPECT_EQ("<int>1", ExecuteExpectSuccess(blob, { JVariant::Int(55) }));
  EXPECT_EQ("<int>2", ExecuteExpectSuccess(blob, { JVariant::Int(56) }));
  EXPECT_EQ("<int>3", ExecuteExpectSuccess(blob, { JVariant::Int(57) }));
  EXPECT_EQ("<int>-1", ExecuteExpectSuccess(blob, { JVariant::Int(58) }));
}


// public static int test(int x) {
//   switch (x) {
//     case 3:
//       return 1;
//
//     case 18:
//       return 2;
//
//     case 427:
//       return 3;
//   }
//
//   return -1;
// }
TEST_F(NanoJavaInterpreterTest, Opcode_lookupswitch) {
  const std::string blob = AssembleMethod("I",
                                          R"(iload_0
         lookupswitch
           3: L3
           18: L18
           427: L427
           default: LDEFAULT
         L3:
         iconst_1
         ireturn
         L18:
         iconst_2
         ireturn
         L427:
         iconst_3
         ireturn
         LDEFAULT:
         iconst_m1
         ireturn)");

  EXPECT_EQ("<int>-1", ExecuteExpectSuccess(blob, { JVariant::Int(1) }));
  EXPECT_EQ("<int>-1", ExecuteExpectSuccess(blob, { JVariant::Int(4) }));
  EXPECT_EQ("<int>1", ExecuteExpectSuccess(blob, { JVariant::Int(3) }));
  EXPECT_EQ("<int>2", ExecuteExpectSuccess(blob, { JVariant::Int(18) }));
  EXPECT_EQ("<int>3", ExecuteExpectSuccess(blob, { JVariant::Int(427) }));
  EXPECT_EQ("<int>-1", ExecuteExpectSuccess(blob, { JVariant::Int(428) }));
}


TEST_F(NanoJavaInterpreterTest, PutFieldBadValueType) {
  ExecuteExpectError(
      AssembleMethod(
          "V",
          R"(.limit stack 2
             ldc "instance"
             ldc "value of a bad type"
             putfield java/lang/String/value [C
             return)"),
      {});
}


TEST_F(NanoJavaInterpreterTest, PutBooleanFieldBadInstanceType) {
  ExecuteExpectError(
      AssembleMethod(
          "V",
          R"(.limit stack 2
             ldc "instance"
             iconst_0
             putfield com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$InstanceFields/booleanInstanceField Z
             return)"),
      {});
}


TEST_F(NanoJavaInterpreterTest, PutByteFieldBadInstanceType) {
  ExecuteExpectError(
      AssembleMethod(
          "V",
          R"(.limit stack 2
             ldc "instance"
             iconst_0
             putfield com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$InstanceFields/byteInstanceField B
             return)"),
      {});
}


TEST_F(NanoJavaInterpreterTest, PutCharFieldBadInstanceType) {
  ExecuteExpectError(
      AssembleMethod(
          "V",
          R"(.limit stack 2
             ldc "instance"
             iconst_0
             putfield com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$InstanceFields/charInstanceField C
             return)"),
      {});
}


TEST_F(NanoJavaInterpreterTest, PutShortFieldBadInstanceType) {
  ExecuteExpectError(
      AssembleMethod(
          "V",
          R"(.limit stack 2
             ldc "instance"
             iconst_0
             putfield com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$InstanceFields/shortInstanceField S
             return)"),
      {});
}


TEST_F(NanoJavaInterpreterTest, PutIntFieldBadInstanceType) {
  ExecuteExpectError(
      AssembleMethod(
          "V",
          R"(.limit stack 2
             ldc "instance"
             iconst_0
             putfield com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$InstanceFields/intInstanceField I
             return)"),
      {});
}


TEST_F(NanoJavaInterpreterTest, PutFloatFieldBadInstanceType) {
  ExecuteExpectError(
      AssembleMethod(
          "V",
          R"(.limit stack 2
             ldc "instance"
             iconst_0
             putfield com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$InstanceFields/floatInstanceField F
             return)"),
      {});
}


TEST_F(NanoJavaInterpreterTest, PutLongFieldBadInstanceType) {
  ExecuteExpectError(
      AssembleMethod(
          "V",
          R"(.limit stack 3
             ldc "instance"
             iconst_0
             i2l
             putfield com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$InstanceFields/longInstanceField J
             return)"),
      {});
}


TEST_F(NanoJavaInterpreterTest, PutDoubleFieldBadInstanceType) {
  ExecuteExpectError(
      AssembleMethod(
          "V",
          R"(.limit stack 3
             ldc "instance"
             fconst_0
             f2d
             putfield com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$InstanceFields/doubleInstanceField D
             return)"),
      {});
}


TEST_F(NanoJavaInterpreterTest, PutObjectFieldBadInstanceType) {
  ExecuteExpectError(
      AssembleMethod(
          "V",
          R"(.limit stack 2
             ldc "instance"
             ldc "value"
             putfield com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$InstanceFields/stringInstanceField Ljava/lang/String;
             return)"),
      {});
}


TEST_F(NanoJavaInterpreterTest, GetBooleanFieldBadInstanceType) {
  ExecuteExpectError(
      AssembleMethod(
          "V",
          R"(.limit locals 0
             ldc "instance"
             getfield com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$InstanceFields/booleanInstanceField Z
             return)"),
      {});
}


TEST_F(NanoJavaInterpreterTest, GetByteFieldBadInstanceType) {
  ExecuteExpectError(
      AssembleMethod(
          "V",
          R"(.limit locals 0
             ldc "instance"
             getfield com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$InstanceFields/byteInstanceField B
             return)"),
      {});
}


TEST_F(NanoJavaInterpreterTest, GetCharFieldBadInstanceType) {
  ExecuteExpectError(
      AssembleMethod(
          "V",
          R"(.limit locals 0
             ldc "instance"
             getfield com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$InstanceFields/charInstanceField C
             return)"),
      {});
}


TEST_F(NanoJavaInterpreterTest, GetShortFieldBadInstanceType) {
  ExecuteExpectError(
      AssembleMethod(
          "V",
          R"(.limit locals 0
             ldc "instance"
             getfield com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$InstanceFields/shortInstanceField S
             return)"),
      {});
}


TEST_F(NanoJavaInterpreterTest, GetIntFieldBadInstanceType) {
  ExecuteExpectError(
      AssembleMethod(
          "V",
          R"(.limit locals 0
             ldc "instance"
             getfield com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$InstanceFields/intInstanceField I
             return)"),
      {});
}


TEST_F(NanoJavaInterpreterTest, GetFloatFieldBadInstanceType) {
  ExecuteExpectError(
      AssembleMethod(
          "V",
          R"(.limit locals 0
             ldc "instance"
             getfield com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$InstanceFields/floatInstanceField F
             return)"),
      {});
}


TEST_F(NanoJavaInterpreterTest, GetLongFieldBadInstanceType) {
  ExecuteExpectError(
      AssembleMethod(
          "V",
          R"(.limit stack 2
             ldc "instance"
             getfield com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$InstanceFields/longInstanceField J
             return)"),
      {});
}


TEST_F(NanoJavaInterpreterTest, GetDoubleFieldBadInstanceType) {
  ExecuteExpectError(
      AssembleMethod(
          "V",
          R"(.limit stack 2
             ldc "instance"
             getfield com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$InstanceFields/doubleInstanceField D
             return)"),
      {});
}


TEST_F(NanoJavaInterpreterTest, GetObjectFieldBadInstanceType) {
  ExecuteExpectError(
      AssembleMethod(
          "V",
          R"(.limit stack 1
             ldc "instance"
             getfield com/google/devtools/cdbg/debuglets/java/NanoJavaInterpreterTestLib$InstanceFields/stringInstanceField Ljava/lang/String;
             return)"),
      {});
}


TEST_F(NanoJavaInterpreterTest, ReturnBadObjectClass) {
  ExecuteExpectError(
      AssembleMethod(
          "Ljava/lang/Exception;",
          R"(.limit stack 1
             ldc "string"
             areturn)"),
      {});
}


TEST_F(NanoJavaInterpreterTest, ReturnNotAnObject) {
  ExecuteExpectError(
      AssembleMethod(
          "Ljava/lang/Object;",
          R"(iconst_1
             areturn)"),
      {});
}


TEST_F(NanoJavaInterpreterTest, PrimitiveReturnObjectAsObject) {
  ExecuteExpectError(
      AssembleMethod(
          "I",
          R"(.limit stack 1
             ldc "string"
             areturn)"),
      {});
}


TEST_F(NanoJavaInterpreterTest, PrimitiveReturnObjectAsInt) {
  ExecuteExpectError(
      AssembleMethod(
          "I",
          R"(.limit stack 1
             ldc "string"
             ireturn)"),
      {});
}


TEST_F(NanoJavaInterpreterTest, InvokeStaticUnavailableClass) {
  EXPECT_EQ(
      FormatMessageModel(
          {ClassNotLoaded, {"com.my.UnknownClass", "Lcom/my/UnknownClass;"}}),
      ExecuteExpectError(
          AssembleMethod(
              "V",
              R"(.limit stack 0
                 invokestatic com/my/UnknownClass/someMethod()V)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, InvokeInstanceUnavailableClass) {
  const std::string test_cases[] = {"invokevirtual", "invokespecial"};
  for (const std::string& test_case : test_cases) {
    EXPECT_EQ(
        FormatMessageModel(
            {ClassNotLoaded, {"com.my.UnknownClass", "Lcom/my/UnknownClass;"}}),
              ExecuteExpectError(AssembleMethod("V", Substitute(
                                                         R"(.limit stack 1
                       new java/lang/Object
                       $0 com/my/UnknownClass/someMethod()V)",
                                                         test_case)),
                                 {}));
  }
}


TEST_F(NanoJavaInterpreterTest, InvokeInterfaceUnavailableClass) {
  EXPECT_EQ(
      FormatMessageModel(
          {ClassNotLoaded, {"com.my.UnknownClass", "Lcom/my/UnknownClass;"}}),
      ExecuteExpectError(
          AssembleMethod(
              "V",
              R"(.limit stack 1
                 new java/lang/Object
                 invokeinterface com/my/UnknownClass/someMethod()V 0)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, GetStaticFieldUnavailableClass) {
  EXPECT_EQ(
      FormatMessageModel(
          {ClassNotLoaded, {"com.my.UnknownClass", "Lcom/my/UnknownClass;"}}),
      ExecuteExpectError(
          AssembleMethod(
              "V",
              R"(.limit stack 1
                 getstatic com/my/UnknownClass/someField Z)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, PutStaticFieldUnavailableClass) {
  EXPECT_EQ(
      FormatMessageModel(
          {ClassNotLoaded, {"com.my.UnknownClass", "Lcom/my/UnknownClass;"}}),
      ExecuteExpectError(
          AssembleMethod(
              "V",
              R"(.limit stack 1
                 iconst_1
                 putstatic com/my/UnknownClass/someField Z)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, GetInstanceFieldUnavailableClass) {
  EXPECT_EQ(
      FormatMessageModel(
          {ClassNotLoaded, {"com.my.UnknownClass", "Lcom/my/UnknownClass;"}}),
      ExecuteExpectError(
          AssembleMethod(
              "V",
              R"(.limit stack 1
                 new java/lang/Object
                 getstatic com/my/UnknownClass/someField Z)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, PutInstanceFieldUnavailableClass) {
  EXPECT_EQ(
      FormatMessageModel(
          {ClassNotLoaded, {"com.my.UnknownClass", "Lcom/my/UnknownClass;"}}),
      ExecuteExpectError(
          AssembleMethod(
              "V",
              R"(.limit stack 2
                 new java/lang/Object
                 iconst_1
                 putstatic com/my/UnknownClass/someField Z)"),
          {}));
}


TEST_F(NanoJavaInterpreterTest, ExecuteMultipleTimes) {
  const std::string blob = AssembleMethod("Ljava/lang/String;",
                                          R"(.limit stack 2
         ldc "hello "
         aload 0
         invokevirtual java/lang/String/concat(Ljava/lang/String;)Ljava/lang/String;
         areturn)");

  for (int i = 0; i < 1000; ++i) {
    std::string suffix = "world " + std::to_string(i + 1);
    ASSERT_EQ(
        "<java.lang.String>hello " + suffix,
        ExecuteExpectSuccess(
            blob,
            { JVariant::LocalRef(JniToJavaString(suffix)) }));
  }
}

}  // namespace nanojava
}  // namespace cdbg
}  // namespace devtools


