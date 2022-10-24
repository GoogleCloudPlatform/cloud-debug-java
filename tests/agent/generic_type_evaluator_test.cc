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

#include "src/agent/generic_type_evaluator.h"

#include "gtest/gtest.h"
#include "src/agent/messages.h"
#include "src/agent/model.h"
#include "src/agent/model_util.h"
#include "src/agent/static_field_reader.h"
#include "tests/agent/fake_instance_field_reader.h"
#include "tests/agent/mock_jni_env.h"
#include "tests/agent/mock_jvmti_env.h"

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

class GenericTypeEvaluatorTest : public ::testing::Test {
 protected:
  GenericTypeEvaluatorTest()
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

      v.push_back(os.str());
    }

    return v;
  }

  void Evaluate(const ClassMetadataReader::Entry& class_metadata) {
    evaluator_.Evaluate(
        nullptr,  // "MethodCaller" not used by "GenericTypeEvaluator".
        class_metadata,
        kEvaluatedObject,
        false,
        &eval_result_);
  }

 protected:
  MockJvmtiEnv jvmti_;
  MockJNIEnv jni_;
  GlobalJvmEnv global_jvm_;
  GenericTypeEvaluator evaluator_;
  std::vector<NamedJVariant> eval_result_;
};


TEST_F(GenericTypeEvaluatorTest, EmptyObject) {
  ClassMetadataReader::Entry metadata;
  metadata.signature = { JType::Object, "LMyEvaluatedClass;" };

  Evaluate(metadata);

  std::vector<std::string> expected_results = {
      ":  [info(6) (\"Object has no fields\")]",
  };

  EXPECT_EQ(expected_results, FormatResults());
}


TEST_F(GenericTypeEvaluatorTest, SingleField) {
  FakeInstanceFieldReader fields[] = {
    { "myint", { JType::Int }, JVariant::Int(427) }
  };

  ClassMetadataReader::Entry metadata;
  metadata.signature = { JType::Object, "LMyEvaluatedClass;" };

  for (const FakeInstanceFieldReader& field : fields) {
    metadata.instance_fields.push_back(field.Clone());
  }

  Evaluate(metadata);

  EXPECT_EQ(std::vector<std::string>({"myint: <int>427"}), FormatResults());
}


TEST_F(GenericTypeEvaluatorTest, MultipleFields) {
  FakeInstanceFieldReader fields[] = {
    {
      "myint",
      { JType::Int },
      JVariant::Int(427)
    },
    {
      "mybool",
      { JType::Boolean },
      JVariant::Boolean(true)
    },
    {
      "mylong",
      { JType::Long },
      JVariant::Long(12345678987654321L)
    }
  };

  ClassMetadataReader::Entry metadata;
  metadata.signature = { JType::Object, "LMyEvaluatedClass;" };

  for (const FakeInstanceFieldReader& field : fields) {
    metadata.instance_fields.push_back(field.Clone());
  }

  Evaluate(metadata);

  std::vector<std::string> expected_results = {
      "myint: <int>427",
      "mybool: <boolean>true",
      "mylong: <long>12345678987654321",
  };

  EXPECT_EQ(expected_results, FormatResults());
}


TEST_F(GenericTypeEvaluatorTest, InstanceFieldsOmitted) {
  ClassMetadataReader::Entry metadata;
  metadata.signature = { JType::Object, "LMyEvaluatedClass;" };
  metadata.instance_fields_omitted = true;

  Evaluate(metadata);

  std::vector<std::string> expected_results = {
      ":  [info(6) (\"" + std::string(InstanceFieldsOmitted) + "\")]",
  };

  EXPECT_EQ(expected_results, FormatResults());
}


}  // namespace cdbg
}  // namespace devtools
