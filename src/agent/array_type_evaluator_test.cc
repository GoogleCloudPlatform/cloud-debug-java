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

#include "array_type_evaluator.h"

#include "src/agent/test_util/fake_instance_field_reader.h"
#include "src/agent/test_util/mock_jni_env.h"
#include "src/agent/test_util/mock_jvmti_env.h"
#include "model.h"
#include "model_util.h"
#include "static_field_reader.h"
#include "gmock/gmock.h"

using testing::_;
using testing::AtMost;
using testing::DoAll;
using testing::Eq;
using testing::IsNull;
using testing::NotNull;
using testing::Test;
using testing::Return;
using testing::ReturnRef;
using testing::StrEq;
using testing::WithArgs;
using testing::Invoke;
using testing::InvokeArgument;
using testing::SetArgPointee;

namespace devtools {
namespace cdbg {

static const jobject kEvaluatedObject =
    reinterpret_cast<jobject>(0x8723456467453L);

static constexpr char kLocalTruncationMessage[] =
    ":  [info(6) (\"Only first $0 items were captured. Use in an expression "
    "to see more items.\", \"3\")]";

static constexpr char kExpressionTruncationMessage[] =
    ":  [info(6) (\"Only first $0 items were captured.\", \"3\")]";

class ArrayTypeEvaluatorTest : public ::testing::Test {
 protected:
  ArrayTypeEvaluatorTest()
      : global_jvm_(&jvmti_, &jni_) {
  }

  void SetUp() override {
    EXPECT_CALL(jni_, NewGlobalRef(_))
        .WillRepeatedly(Invoke([] (jobject obj) { return obj; }));

    EXPECT_CALL(jni_, DeleteLocalRef(_))
        .WillRepeatedly(Return());

    EXPECT_CALL(jni_, DeleteGlobalRef(_))
        .WillRepeatedly(Return());

    EXPECT_CALL(jni_, IsSameObject(_, _))
        .WillRepeatedly(Invoke([] (jobject obj1, jobject obj2) {
          return obj1 == obj2;
        }));

    EXPECT_CALL(jni_, GetObjectRefType(_))
        .WillRepeatedly(Return(JNILocalRefType));
  }

  static bool IsSameObject(jobject obj1, jobject obj2) {
    return obj1 == obj2;
  }

  static jobject NewRef(jobject src) {
    return src;
  }

  std::vector<std::string> FormatResults() {
    std::vector<std::string> v;
    for (const NamedJVariant& var : eval_result_) {
      std::ostringstream os;
      os << var.name << ": ";

      if (!var.status.description.format.empty()) {
        os << " [" << var.status << "]";
      } else {
        os << var.value.ToString(false);
      }

      switch (var.well_known_jclass) {
        case WellKnownJClass::Unknown:
          break;

        case WellKnownJClass::String:
          os << ", well known jclass: String";
          break;

        case WellKnownJClass::Array:
          os << ", well known jclass: Array";
          break;
      }

      v.push_back(os.str());
    }

    return v;
  }

  void SetUpPrimitiveArray(
      int array_length,
      const void* array_data) {
    EXPECT_CALL(jni_, GetArrayLength(static_cast<jarray>(kEvaluatedObject)))
        .WillRepeatedly(Return(array_length));

    EXPECT_CALL(
        jni_,
        GetPrimitiveArrayCritical(static_cast<jarray>(kEvaluatedObject), _))
        .WillOnce(Return(const_cast<void*>(array_data)));

    EXPECT_CALL(
        jni_,
        ReleasePrimitiveArrayCritical(
            static_cast<jarray>(kEvaluatedObject),
            const_cast<void*>(array_data),
            0))
        .WillOnce(Return());
  }

  void Evaluate(
      const ClassMetadataReader::Entry& class_metadata,
      bool is_watch_expression) {
    eval_result_.clear();
    evaluator_->Evaluate(
        nullptr,  // "MethodCaller" not used by array evaluator.
        class_metadata,
        kEvaluatedObject,
        is_watch_expression,
        &eval_result_);
  }

  template <typename TArrayType>
  void TestMaxCaptureArrayOf(const char* object_signature,
                             const char* typeName,
                             const char* expected_signature) {
    // Verify array twice - as watch expression and as local
    for (bool is_watch_expression : { true, false }) {
      // We set cap to 3 and expect trimmed data.
      evaluator_.reset(new ArrayTypeEvaluator<TArrayType>
            (is_watch_expression ? 3 : 0,    // max_capture_expression_elements
             is_watch_expression ? 0 : 3,    // max_capture_object_elements
             is_watch_expression ? 0 : 3));  // max_capture_primitive_elements

      EXPECT_EQ(std::string("ArrayTypeEvaluator<") + typeName + ">",
                evaluator_->GetEvaluatorName());

      ClassMetadataReader::Entry metadata;
      metadata.signature = { JType::Object, object_signature };

      // Make array that is larger than limit
      const TArrayType arr[] = { 0, 1, 2, 3 };

      SetUpPrimitiveArray(arraysize(arr), arr);
      Evaluate(metadata, is_watch_expression);

      const std::vector<std::string> expected_result = {
          "length: <int>4",
          std::string("[0]: <") + expected_signature + ">0",
          std::string("[1]: <") + expected_signature + ">1",
          std::string("[2]: <") + expected_signature + ">2",
          is_watch_expression ? kExpressionTruncationMessage
                              : kLocalTruncationMessage,
      };

      EXPECT_EQ(expected_result, FormatResults());
    }
  }

 protected:
  MockJvmtiEnv jvmti_;
  MockJNIEnv jni_;
  GlobalJvmEnv global_jvm_;
  std::unique_ptr<TypeEvaluator> evaluator_;
  std::vector<NamedJVariant> eval_result_;
};

// Evaluation of Boolean array
TEST_F(ArrayTypeEvaluatorTest, ArrayBoolean) {
  evaluator_.reset(new ArrayTypeEvaluator<jboolean>);

  EXPECT_EQ("ArrayTypeEvaluator<jboolean>", evaluator_->GetEvaluatorName());

  ClassMetadataReader::Entry metadata;
  metadata.signature = { JType::Object, "[Z" };

  const jboolean arr[] = {
    true,
    false,
    true,
    false
  };

  SetUpPrimitiveArray(arraysize(arr), arr);
  Evaluate(metadata, false);
  EXPECT_EQ(std::vector<std::string>({
                "length: <int>4",
                "[0]: <boolean>true",
                "[1]: <boolean>false",
                "[2]: <boolean>true",
                "[3]: <boolean>false",
            }),
            FormatResults());
}


// Evaluation of Boolean array that exceeds max capture limit
TEST_F(ArrayTypeEvaluatorTest, ArrayBooleanExceedsMaxCapture) {
  ClassMetadataReader::Entry metadata;
  metadata.signature = { JType::Object, "[Z" };

  const struct {
    bool is_watch_expression;
    std::vector<std::string> expected_formatted_members;
  } test_cases[] = {
    {
      false,
      {
        "length: <int>4",
        "[0]: <boolean>true",
        "[1]: <boolean>true",
        "[2]: <boolean>true",
        kLocalTruncationMessage,
      }
    },
    {
      true,
      {
        "length: <int>4",
        "[0]: <boolean>true",
        "[1]: <boolean>true",
        "[2]: <boolean>true",
        kExpressionTruncationMessage,
      }
    },
  };

  // Make array that is larger than limit
  const jboolean arr[] = {
    true,
    true,
    true,
    false,  // beyond limit, should not appear in results (trimmed)
  };

  for (const auto& test_case : test_cases) {
    // Set max capture element limit to 3
    evaluator_.reset(new ArrayTypeEvaluator<jboolean>
                     (test_case.is_watch_expression ? 3 : 0,
                      test_case.is_watch_expression ? 0 : 3,
                      test_case.is_watch_expression ? 0 : 3));
    EXPECT_EQ("ArrayTypeEvaluator<jboolean>", evaluator_->GetEvaluatorName());
    SetUpPrimitiveArray(arraysize(arr), arr);
    Evaluate(metadata, test_case.is_watch_expression);
    EXPECT_EQ(test_case.expected_formatted_members, FormatResults());
  }
}


// Evaluation of Byte array
TEST_F(ArrayTypeEvaluatorTest, ArrayByte) {
  evaluator_.reset(new ArrayTypeEvaluator<jbyte>);

  EXPECT_EQ("ArrayTypeEvaluator<jbyte>", evaluator_->GetEvaluatorName());

  ClassMetadataReader::Entry metadata;
  metadata.signature = { JType::Object, "[B" };

  const jbyte arr[] = {
    42,
    43
  };

  SetUpPrimitiveArray(arraysize(arr), arr);
  Evaluate(metadata, false);
  EXPECT_EQ(std::vector<std::string>(
                {"length: <int>2", "[0]: <byte>42", "[1]: <byte>43"}),
            FormatResults());
}


// Evaluation of Byte array that exceeds max capture limit
TEST_F(ArrayTypeEvaluatorTest, ArrayByteExceedsMaxCapture) {
  TestMaxCaptureArrayOf<jbyte>("[B", "jbyte", "byte");
}


// Evaluation of Char array
TEST_F(ArrayTypeEvaluatorTest, ArrayChar) {
  evaluator_.reset(new ArrayTypeEvaluator<jchar>);

  EXPECT_EQ("ArrayTypeEvaluator<jchar>", evaluator_->GetEvaluatorName());

  ClassMetadataReader::Entry metadata;
  metadata.signature = { JType::Object, "[C" };

  const jchar arr[] = {
    1,
    10000,
    20000
  };

  SetUpPrimitiveArray(arraysize(arr), arr);
  Evaluate(metadata, false);
  EXPECT_EQ(std::vector<std::string>({
                "length: <int>3",
                "[0]: <char>1",
                "[1]: <char>10000",
                "[2]: <char>20000",
            }),
            FormatResults());
}


// Evaluation of Char array that exceeds max capture limit
TEST_F(ArrayTypeEvaluatorTest, ArrayCharExceedsMaxCapture) {
  TestMaxCaptureArrayOf<jchar>("[C", "jchar", "char");
}


// Evaluation of Short array
TEST_F(ArrayTypeEvaluatorTest, ArrayShort) {
  evaluator_.reset(new ArrayTypeEvaluator<jshort>);

  EXPECT_EQ("ArrayTypeEvaluator<jshort>", evaluator_->GetEvaluatorName());

  ClassMetadataReader::Entry metadata;
  metadata.signature = { JType::Object, "[S" };

  const jshort arr[] = {
    1,
    -23456,
  };

  SetUpPrimitiveArray(arraysize(arr), arr);
  Evaluate(metadata, false);
  EXPECT_EQ(std::vector<std::string>({
                "length: <int>2",
                "[0]: <short>1",
                "[1]: <short>-23456",
            }),
            FormatResults());
}


// Evaluation of Short array that exceeds max capture limit
TEST_F(ArrayTypeEvaluatorTest, ArrayShortExceedsMaxCapture) {
  TestMaxCaptureArrayOf<jshort>("[S", "jshort", "short");
}


// Evaluation of Int array
TEST_F(ArrayTypeEvaluatorTest, ArrayInt) {
  evaluator_.reset(new ArrayTypeEvaluator<jint>);

  EXPECT_EQ("ArrayTypeEvaluator<jint>", evaluator_->GetEvaluatorName());

  ClassMetadataReader::Entry metadata;
  metadata.signature = { JType::Object, "[I" };

  const jint arr[] = {
    3487,
    19072348,
    -897345
  };

  SetUpPrimitiveArray(arraysize(arr), arr);
  Evaluate(metadata, false);
  EXPECT_EQ(std::vector<std::string>({
                "length: <int>3",
                "[0]: <int>3487",
                "[1]: <int>19072348",
                "[2]: <int>-897345",
            }),
            FormatResults());
}


// Evaluation of Int array that exceeds max capture limit
TEST_F(ArrayTypeEvaluatorTest, ArrayIntExceedsMaxCapture) {
  TestMaxCaptureArrayOf<jint>("[I", "jint", "int");
}


// Evaluation of Long array
TEST_F(ArrayTypeEvaluatorTest, ArrayLong) {
  evaluator_.reset(new ArrayTypeEvaluator<jlong>);

  EXPECT_EQ("ArrayTypeEvaluator<jlong>", evaluator_->GetEvaluatorName());

  ClassMetadataReader::Entry metadata;
  metadata.signature = { JType::Object, "[J" };

  const jlong arr[] = {
    3487893487344L
  };

  SetUpPrimitiveArray(arraysize(arr), arr);
  Evaluate(metadata, false);
  EXPECT_EQ(
      std::vector<std::string>({"length: <int>1", "[0]: <long>3487893487344"}),
      FormatResults());
}


// Evaluation of Long array that exceeds max capture limit
TEST_F(ArrayTypeEvaluatorTest, ArrayLongExceedsMaxCapture) {
  TestMaxCaptureArrayOf<jlong>("[J", "jlong", "long");
}


// Evaluation of Float array
TEST_F(ArrayTypeEvaluatorTest, ArrayFloat) {
  evaluator_.reset(new ArrayTypeEvaluator<jfloat>);

  EXPECT_EQ("ArrayTypeEvaluator<jfloat>", evaluator_->GetEvaluatorName());

  ClassMetadataReader::Entry metadata;
  metadata.signature = { JType::Object, "[F" };

  const jfloat arr[] = {
    12.44f,
    -34.66f,
    -3.1415f
  };

  SetUpPrimitiveArray(arraysize(arr), arr);
  Evaluate(metadata, false);
  EXPECT_EQ(std::vector<std::string>({
                "length: <int>3",
                "[0]: <float>12.44",
                "[1]: <float>-34.66",
                "[2]: <float>-3.1415",
            }),
            FormatResults());
}


// Evaluation of Float array that exceeds max capture limit
TEST_F(ArrayTypeEvaluatorTest, ArrayFloatExceedsMaxCapture) {
  TestMaxCaptureArrayOf<jfloat>("[F", "jfloat", "float");
}


// Evaluation of Double array
TEST_F(ArrayTypeEvaluatorTest, ArrayDouble) {
  evaluator_.reset(new ArrayTypeEvaluator<jdouble>);

  EXPECT_EQ("ArrayTypeEvaluator<jdouble>", evaluator_->GetEvaluatorName());

  ClassMetadataReader::Entry metadata;
  metadata.signature = { JType::Object, "[D" };

  const jdouble arr[] = {
    567.32,
    -23423.7
  };

  SetUpPrimitiveArray(arraysize(arr), arr);
  Evaluate(metadata, false);
  EXPECT_EQ(std::vector<std::string>({"length: <int>2", "[0]: <double>567.32",
                                      "[1]: <double>-23423.7"}),
            FormatResults());
}


// Evaluation of Double array that exceeds max capture limit
TEST_F(ArrayTypeEvaluatorTest, ArrayDoubleExceedsMaxCapture) {
  TestMaxCaptureArrayOf<jdouble>("[D", "jdouble", "double");
}


// Evaluation of Object array
TEST_F(ArrayTypeEvaluatorTest, ArrayOfObjects) {
  evaluator_.reset(new ArrayTypeEvaluator<jobject>);

  EXPECT_EQ("ArrayTypeEvaluator<jobject>", evaluator_->GetEvaluatorName());

  static const jclass kElementClass = reinterpret_cast<jclass>(0x18223426534L);

  const struct {
    std::string array_signature;
    std::vector<std::string> expected_formatted_members;
  } test_cases[] = {
    {
      "[I",
      {
        "length: <int>3",
        "[0]: <Object>",
        "[1]: <Object>",
        "[2]: null"
      }
    },
    {
      "[Ljava/lang/Thread;",
      {
        "length: <int>3",
        "[0]: <Object>",
        "[1]: <Object>",
        "[2]: null"
      }
    },
    {
      "[[Ljava/lang/String;",
      {
        "length: <int>3",
        "[0]: <Object>, well known jclass: Array",
        "[1]: <Object>, well known jclass: Array",
        "[2]: null, well known jclass: Array"
      }
    },
    {
      "[Ljava/lang/String;",
      {
        "length: <int>3",
        "[0]: <Object>, well known jclass: String",
        "[1]: <Object>, well known jclass: String",
        "[2]: null, well known jclass: String"
      }
    }
  };

  const jobject kArr[] = {
    reinterpret_cast<jobject>(0x2378462834L),
    reinterpret_cast<jobject>(0x763565425L),
    nullptr
  };

  for (jobject obj : kArr) {
    if (obj != nullptr) {
      EXPECT_CALL(jni_, GetObjectClass(obj))
          .WillRepeatedly(Return(kElementClass));
    }
  }

  for (const auto& test_case : test_cases) {
    ClassMetadataReader::Entry array_metadata;
    array_metadata.signature = { JType::Object, test_case.array_signature };

    EXPECT_CALL(jni_, GetArrayLength(static_cast<jarray>(kEvaluatedObject)))
        .WillRepeatedly(Return(arraysize(kArr)));

    for (int i = 0; i != arraysize(kArr); ++i) {
      EXPECT_CALL(
          jni_,
          GetObjectArrayElement(
              static_cast<jobjectArray>(kEvaluatedObject),
              i))
          .WillOnce(Return(kArr[i]));
    }

    Evaluate(array_metadata, false);
    EXPECT_EQ(test_case.expected_formatted_members, FormatResults());
  }
}


// Evaluation of Object array that exceeds max capture limit
TEST_F(ArrayTypeEvaluatorTest, ArrayOfObjectsExceedsMaxCapture) {
  static const jclass kElementClass = reinterpret_cast<jclass>(0x18223426534L);

  const struct {
    bool is_watch_expression;
    std::string array_signature;
    std::vector<std::string> expected_formatted_members;
  } test_cases[] = {
    {
      false,
      "[I",
      {
        "length: <int>4",
        "[0]: <Object>",
        "[1]: <Object>",
        "[2]: null",
        kLocalTruncationMessage,
      }
    },
    {
      true,
      "[I",
      {
        "length: <int>4",
        "[0]: <Object>",
        "[1]: <Object>",
        "[2]: null",
        kExpressionTruncationMessage,
      }
    },
  };

  // Make array that is larger than limit
  const jobject kArr[] = {
    reinterpret_cast<jobject>(0x2378462834L),
    reinterpret_cast<jobject>(0x763565425L),
    nullptr,
    nullptr,  // beyond limit, should not appear in results (trimmed)
  };

  for (jobject obj : kArr) {
    if (obj != nullptr) {
      EXPECT_CALL(jni_, GetObjectClass(obj))
          .WillRepeatedly(Return(kElementClass));
    }
  }

  for (const auto& test_case : test_cases) {
    // Set max capture element limit to 3
    evaluator_.reset(new ArrayTypeEvaluator<jobject>
                     (test_case.is_watch_expression ? 3 : 0,
                      test_case.is_watch_expression ? 0 : 3,
                      test_case.is_watch_expression ? 0 : 3));
    EXPECT_EQ("ArrayTypeEvaluator<jobject>", evaluator_->GetEvaluatorName());

    ClassMetadataReader::Entry array_metadata;
    array_metadata.signature = { JType::Object, test_case.array_signature };

    EXPECT_CALL(jni_, GetArrayLength(static_cast<jarray>(kEvaluatedObject)))
        .WillRepeatedly(Return(arraysize(kArr)));

    for (int i = 0; i < 3; ++i) {
      EXPECT_CALL(
          jni_,
          GetObjectArrayElement(
              static_cast<jobjectArray>(kEvaluatedObject),
              i))
          .WillOnce(Return(kArr[i]));
    }

    Evaluate(array_metadata, test_case.is_watch_expression);
    EXPECT_EQ(test_case.expected_formatted_members, FormatResults());
  }
}

}  // namespace cdbg
}  // namespace devtools
