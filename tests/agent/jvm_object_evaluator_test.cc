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

#include "src/agent/jvm_object_evaluator.h"

#include "gtest/gtest.h"
#include "jni_proxy_bigdecimal.h"
#include "jni_proxy_biginteger.h"
#include "jni_proxy_iterable.h"
#include "jni_proxy_ju_map.h"
#include "jni_proxy_ju_map_entry.h"
#include "src/agent/instance_field_reader.h"
#include "src/agent/messages.h"
#include "src/agent/jni_utils.h"
#include "src/agent/model.h"
#include "src/agent/static_field_reader.h"
#include "src/agent/type_evaluator.h"
#include "tests/agent/fake_instance_field_reader.h"
#include "tests/agent/fake_jni.h"
#include "tests/agent/mock_class_indexer.h"
#include "tests/agent/mock_class_metadata_reader.h"
#include "tests/agent/mock_jvmti_env.h"
#include "tests/agent/mock_method_caller.h"

using testing::_;
using testing::Invoke;
using testing::ReturnRef;

namespace devtools {
namespace cdbg {

class JvmObjectEvaluatorTest : public ::testing::Test {
 protected:
  JvmObjectEvaluatorTest()
      : global_jvm_(fake_jni_.jvmti(), fake_jni_.jni()),
        evaluator_(&class_metadata_reader_) {
  }

  void SetUp() override {
    EXPECT_CALL(class_metadata_reader_, GetClassMetadata(_))
        .WillRepeatedly(Invoke([this] (jclass cls)
            -> const ClassMetadataReader::Entry& {
          ClassMetadataReader::Entry* entry = new ClassMetadataReader::Entry;
          entry->signature = JSignatureFromSignature(
              fake_jni_.MutableClassMetadata(cls).signature);

          metadata_cache_.push_back(
              std::unique_ptr<ClassMetadataReader::Entry>(entry));

          return *entry;
        }));

    CHECK(jniproxy::BindBigDecimal());
    CHECK(jniproxy::BindBigInteger());
    CHECK(jniproxy::BindIterable());
    CHECK(jniproxy::BindMap());
    CHECK(jniproxy::BindMap_Entry());
  }

  void TearDown() override {
    jniproxy::CleanupBigDecimal();
    jniproxy::CleanupBigInteger();
    jniproxy::CleanupIterable();
    jniproxy::CleanupMap();
    jniproxy::CleanupMap_Entry();
  }

  void Evaluate(jobject obj) {
    evaluator_.Evaluate(&method_caller_, obj, false, &members_);
  }

 protected:
  FakeJni fake_jni_;
  GlobalJvmEnv global_jvm_;
  MockClassMetadataReader class_metadata_reader_;
  MockMethodCaller method_caller_;
  JvmObjectEvaluator evaluator_;

  // Evaluation results.
  std::vector<NamedJVariant> members_;

  // "ClassMetadataReader.GetClassMetadata" returns a reference, so we keep
  // all the entries in a vector to make it safe.
  std::list<std::unique_ptr<ClassMetadataReader::Entry>> metadata_cache_;
};


TEST_F(JvmObjectEvaluatorTest, NullObject) {
  evaluator_.Initialize();

  Evaluate(nullptr);

  ASSERT_EQ(1, members_.size());
  ASSERT_EQ("", members_[0].name);
  ASSERT_EQ(JType::Void, members_[0].value.type());
  EXPECT_TRUE(members_[0].status.is_error);
  EXPECT_FALSE(members_[0].status.description.format.empty());
}


TEST_F(JvmObjectEvaluatorTest, StringSpecialCase) {
  evaluator_.Initialize();

  JniLocalRef jstr(fake_jni_.CreateNewJavaString("abc"));

  Evaluate(jstr.get());

  ASSERT_EQ(1, members_.size());
  EXPECT_EQ(WellKnownJClass::String, members_[0].well_known_jclass);
  EXPECT_TRUE(members_[0].status.description.format.empty());

  jobject actual_obj = nullptr;
  EXPECT_TRUE(members_[0].value.get<jobject>(&actual_obj));
  EXPECT_TRUE(fake_jni_.jni()->IsSameObject(jstr.get(), actual_obj));
}


TEST_F(JvmObjectEvaluatorTest, BadArray) {
  evaluator_.Initialize();

  FakeJni::ClassMetadata bad_array_metadata;
  bad_array_metadata.signature = "[V";

  JniLocalRef cls(fake_jni_.CreateNewClass(bad_array_metadata));
  EXPECT_FALSE(cls == nullptr);

  JniLocalRef obj(fake_jni_.CreateNewObject(static_cast<jclass>(cls.get())));
  EXPECT_FALSE(obj == nullptr);

  Evaluate(obj.get());

  ASSERT_EQ(1, members_.size());
  ASSERT_EQ("", members_[0].name);
  ASSERT_EQ(JType::Void, members_[0].value.type());
  EXPECT_TRUE(members_[0].status.is_error);
  EXPECT_FALSE(members_[0].status.description.format.empty());
}


TEST_F(JvmObjectEvaluatorTest, GenericObject) {
  evaluator_.Initialize();

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
    }
  };

  ClassMetadataReader::Entry metadata;
  metadata.signature = { JType::Object, "Lcom/prod/MyClass1;" };

  for (const FakeInstanceFieldReader& field : fields) {
    metadata.instance_fields.push_back(field.Clone());
  }

  EXPECT_CALL(class_metadata_reader_, GetClassMetadata(_))
      .WillRepeatedly(ReturnRef(metadata));

  JniLocalRef obj(fake_jni_.CreateNewObject(FakeJni::StockClass::MyClass1));
  EXPECT_FALSE(obj == nullptr);

  Evaluate(obj.get());

  ASSERT_EQ(2, members_.size());

  EXPECT_EQ("myint", members_[0].name);
  EXPECT_EQ("<int>427", members_[0].value.ToString(false));

  EXPECT_EQ("mybool", members_[1].name);
  EXPECT_EQ("<boolean>true", members_[1].value.ToString(false));
}


TEST_F(JvmObjectEvaluatorTest, EvaluatorSelection) {
  evaluator_.Initialize();

  const struct {
    std::string class_signature;
    std::string expected_evaluator_name;
  } test_cases[] = {
    { "Lcom/prod/MyClass;", "GenericTypeEvaluator" },
    { "[Z", "ArrayTypeEvaluator<jboolean>" },
    { "[C", "ArrayTypeEvaluator<jchar>" },
    { "[B", "ArrayTypeEvaluator<jbyte>" },
    { "[S", "ArrayTypeEvaluator<jshort>" },
    { "[I", "ArrayTypeEvaluator<jint>" },
    { "[J", "ArrayTypeEvaluator<jlong>" },
    { "[F", "ArrayTypeEvaluator<jfloat>" },
    { "[D", "ArrayTypeEvaluator<jdouble>" },
    { "[Lcom/prod/MyClass;", "ArrayTypeEvaluator<jobject>" },
  };

  for (const auto& test_case : test_cases) {
    ClassMetadataReader::Entry metadata;
    metadata.signature = JSignatureFromSignature(test_case.class_signature);

    auto* type_evaluator = evaluator_.SelectEvaluator(nullptr, metadata);

    EXPECT_EQ(
        test_case.expected_evaluator_name,
        type_evaluator->GetEvaluatorName())
        << test_case.class_signature;
  }
}

TEST_F(JvmObjectEvaluatorTest, EvaluatorSelectionForPrettyPrinters) {
  evaluator_.Initialize();

  const struct {
    FakeJni::StockClass stock_class;
    std::string expected_evaluator_name;
  } test_cases[] = {
      {FakeJni::StockClass::Iterable, "IterableTypeEvaluator"},
      {FakeJni::StockClass::Map, "MapTypeEvaluator"},
      {FakeJni::StockClass::MapEntry, "MapEntryTypeEvaluator"},
      {FakeJni::StockClass::BigDecimal, "StringableTypeEvaluator"},
      {FakeJni::StockClass::BigInteger, "StringableTypeEvaluator"},
  };

  for (const auto& test_case : test_cases) {
    ClassMetadataReader::Entry metadata;
    metadata.signature = JSignatureFromSignature(
        fake_jni_.MutableStockClassMetadata(test_case.stock_class).signature);

    auto* type_evaluator = evaluator_.SelectEvaluator(
        fake_jni_.GetStockClass(test_case.stock_class), metadata);

    EXPECT_EQ(test_case.expected_evaluator_name,
              type_evaluator->GetEvaluatorName())
        << metadata.signature.object_signature;
  }
}

TEST_F(JvmObjectEvaluatorTest, EvaluatorSelectionForPrettyPrintersDisabled) {
  JvmObjectEvaluator::Options options;
  options.pretty_print_iterable = false;
  options.pretty_print_map = false;
  options.pretty_print_map_entry = false;
  options.pretty_print_stringable = false;
  evaluator_.Initialize(options);

  const FakeJni::StockClass stock_classes[] = {
      FakeJni::StockClass::Iterable,
      FakeJni::StockClass::Map,
      FakeJni::StockClass::MapEntry,
      FakeJni::StockClass::BigDecimal,
      FakeJni::StockClass::BigInteger
  };

  for (const auto stock_class : stock_classes) {
    ClassMetadataReader::Entry metadata;
    metadata.signature = JSignatureFromSignature(
        fake_jni_.MutableStockClassMetadata(stock_class).signature);

    auto* type_evaluator = evaluator_.SelectEvaluator(
        fake_jni_.GetStockClass(stock_class), metadata);

    EXPECT_EQ("GenericTypeEvaluator", type_evaluator->GetEvaluatorName())
        << metadata.signature.object_signature;
  }
}

}  // namespace cdbg
}  // namespace devtools
