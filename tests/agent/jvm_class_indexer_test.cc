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

#include "src/agent/jvm_class_indexer.h"

#include "gtest/gtest.h"
#include "src/agent/jni_utils.h"
#include "tests/agent/fake_jni.h"
#include "tests/agent/mock_jni_env.h"
#include "tests/agent/mock_jvmti_env.h"

using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::IsNull;
using testing::NiceMock;
using testing::NotNull;
using testing::Test;
using testing::Return;
using testing::ReturnNew;
using testing::SaveArg;
using testing::StrEq;
using testing::WithArgs;
using testing::Invoke;
using testing::InvokeArgument;
using testing::SetArgPointee;

namespace devtools {
namespace cdbg {

class JvmClassIndexerTest : public ::testing::Test {
 protected:
  JvmClassIndexerTest()
      : fake_jni_(&jvmti_, &jni_),
        global_jvm_(&jvmti_, &jni_) {
    fake_classes_ = {
      fake_jni_.CreateNewClass({ "Class1.java", "Lcom/myprod/Class1;" }),
      fake_jni_.CreateNewClass({ "Class2.java", "Lcom/myprod/Class2;" }),
      fake_jni_.CreateNewClass({ "Class3.java", "Lcom/myprod/Class3;" }),
      fake_jni_.CreateNewClass({ "Class4.java", "Lcom/myprod/Class4;" }),
      fake_jni_.CreateNewClass({ "Ambiguous.java", "Lcom/myprod/Amb;" }),
      fake_jni_.CreateNewClass({ "Ambiguous.java", "Lcom/myprod$Amb;" })
    };
  }

  void TearDown() override {
    for (jobject fake_class : fake_classes_) {
      jni_.DeleteLocalRef(fake_class);
    }
  }

 protected:
  NiceMock<MockJvmtiEnv> jvmti_;
  NiceMock<MockJNIEnv> jni_;
  FakeJni fake_jni_;
  GlobalJvmEnv global_jvm_;

  // List of fake classes that test cases use.
  std::vector<jclass> fake_classes_;
};


TEST_F(JvmClassIndexerTest, FindClassByName) {
  JvmClassIndexer class_indexer;
  class_indexer.Initialize();

  EXPECT_TRUE(jni_.IsSameObject(
      class_indexer.FindClassByName("com.myprod.Class1").get(),
      fake_classes_[0]));

  class_indexer.Cleanup();
}


TEST_F(JvmClassIndexerTest, FindClassByNameNegative) {
  JvmClassIndexer class_indexer;
  class_indexer.Initialize();

  EXPECT_EQ(nullptr, class_indexer.FindClassByName("com.myprod.Class1A"));

  class_indexer.Cleanup();
}


TEST_F(JvmClassIndexerTest, FindClassBySignature) {
  JvmClassIndexer class_indexer;
  class_indexer.Initialize();

  EXPECT_TRUE(jni_.IsSameObject(
      class_indexer.FindClassBySignature("Lcom/myprod/Class1;").get(),
      fake_classes_[0]));

  class_indexer.Cleanup();
}


TEST_F(JvmClassIndexerTest, FindReclaimedClassBySignature) {
  JvmClassIndexer class_indexer;
  class_indexer.Initialize();

  JniLocalRef cls(fake_jni_.CreateNewClass({ "My.java", "LMy;" }));
  jobject weak_ref = jni_.NewWeakGlobalRef(cls.get());

  EXPECT_CALL(jni_, NewWeakGlobalRef(NotNull()))
      .WillOnce(Return(weak_ref));

  class_indexer.JvmtiOnClassPrepare(static_cast<jclass>(cls.get()));

  EXPECT_TRUE(jni_.IsSameObject(
      class_indexer.FindClassBySignature("LMy;").get(),
      cls.get()));

  fake_jni_.InvalidateObject(weak_ref);

  EXPECT_EQ(nullptr, class_indexer.FindClassBySignature("LMy;"));

  class_indexer.Cleanup();
}


TEST_F(JvmClassIndexerTest, AmbiguousClassBySignature) {
  JvmClassIndexer class_indexer;
  class_indexer.Initialize();

  EXPECT_TRUE(jni_.IsSameObject(
      class_indexer.FindClassBySignature("Lcom/myprod$Amb;").get(),
      fake_classes_[5]));

  EXPECT_TRUE(jni_.IsSameObject(
      class_indexer.FindClassBySignature("Lcom/myprod/Amb;").get(),
      fake_classes_[4]));

  class_indexer.Cleanup();
}


TEST_F(JvmClassIndexerTest, FindClassBySignatureNegative) {
  JvmClassIndexer class_indexer;
  class_indexer.Initialize();

  EXPECT_EQ(nullptr,
            class_indexer.FindClassBySignature("Lcom/myprod/Class1A;"));

  class_indexer.Cleanup();
}


TEST_F(JvmClassIndexerTest, ClassPrepare) {
  JvmClassIndexer class_indexer;

  EXPECT_EQ(nullptr, class_indexer.FindClassByName("com.myprod.Class3"));

  class_indexer.JvmtiOnClassPrepare(fake_classes_[2]);

  EXPECT_TRUE(jni_.IsSameObject(
      class_indexer.FindClassByName("com.myprod.Class3").get(),
      fake_classes_[2]));

  class_indexer.Cleanup();
}


TEST_F(JvmClassIndexerTest, OnClassPrepareEvent) {
  JvmClassIndexer class_indexer;

  int callback_counter = 0;
  auto cookie = class_indexer.SubscribeOnClassPreparedEvents(
      [this, &callback_counter](const std::string& type_name,
                                const std::string& class_signature) {
        ++callback_counter;
        EXPECT_EQ(1, callback_counter);

        EXPECT_EQ("com.myprod.Class3", type_name);
        EXPECT_EQ("Lcom/myprod/Class3;", class_signature);
      });

  class_indexer.JvmtiOnClassPrepare(fake_classes_[2]);

  class_indexer.UnsubscribeOnClassPreparedEvents(std::move(cookie));

  class_indexer.Cleanup();
}


TEST_F(JvmClassIndexerTest, ClassUnloaded) {
  JvmClassIndexer class_indexer;
  class_indexer.Initialize();

  fake_jni_.InvalidateObject(fake_classes_[0]);

  EXPECT_EQ(nullptr,
            class_indexer.FindClassBySignature("Lcom/myprod/Class1;"));

  class_indexer.Cleanup();
}


TEST_F(JvmClassIndexerTest, ClassReferenceLoadedClass) {
  JvmClassIndexer class_indexer;
  class_indexer.Initialize();

  std::shared_ptr<ClassIndexer::Type> type =
      class_indexer.GetReference("Lcom/prod/MyClass1;");
  EXPECT_EQ(JType::Object, type->GetType());
  EXPECT_EQ("Lcom/prod/MyClass1;", type->GetSignature());
  EXPECT_TRUE(jni_.IsSameObject(
      fake_jni_.GetStockClass(FakeJni::StockClass::MyClass1),
      type->FindClass()));
}


TEST_F(JvmClassIndexerTest, ClassReferenceUnknownClass) {
  JvmClassIndexer class_indexer;
  class_indexer.Initialize();

  std::shared_ptr<ClassIndexer::Type> type =
      class_indexer.GetReference("Lcom/prod/UnknownClass;");
  EXPECT_EQ(JType::Object, type->GetType());
  EXPECT_EQ("Lcom/prod/UnknownClass;", type->GetSignature());
  EXPECT_EQ(nullptr, type->FindClass());
}

}  // namespace cdbg
}  // namespace devtools
