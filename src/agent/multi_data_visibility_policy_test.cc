/**
 * Copyright 2017 Google Inc. All Rights Reserved.
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

#include "multi_data_visibility_policy.h"

#include "mock_data_visibility_policy.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;
using testing::ByMove;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;

namespace devtools {
namespace cdbg {

class MultiDataVisibilityPolicyTest : public ::testing::Test {
 protected:
  MultiDataVisibilityPolicyTest() {}

  // Preconstructed objects for convienence.
  std::unique_ptr<MockDataVisibilityPolicy> policy1_ {
    new MockDataVisibilityPolicy() };

  std::unique_ptr<MockDataVisibilityPolicy> policy2_ {
    new MockDataVisibilityPolicy() };

  std::unique_ptr<MockDataVisibilityPolicy> policy3_ {
    new MockDataVisibilityPolicy() };

  std::unique_ptr<MockDataVisibilityPolicy::Class> class1_ {
    new MockDataVisibilityPolicy::Class() };

  std::unique_ptr<MockDataVisibilityPolicy::Class> class2_ {
    new MockDataVisibilityPolicy::Class() };
};


// If all visibility policies return a null, so should this one
TEST_F(MultiDataVisibilityPolicyTest, AllNulls) {
  EXPECT_CALL(*policy1_, GetClassVisibility(_))
      .WillOnce(Return(ByMove(nullptr)));

  EXPECT_CALL(*policy2_, GetClassVisibility(_))
      .WillOnce(Return(ByMove(nullptr)));

  EXPECT_CALL(*policy3_, GetClassVisibility(_))
      .WillOnce(Return(ByMove(nullptr)));

  MultiDataVisibilityPolicy policy(
      std::move(policy1_),
      std::move(policy2_),
      std::move(policy3_));

  EXPECT_EQ(nullptr, policy.GetClassVisibility(nullptr));
}


// If only one policy returns a non-null, return a pointer to that policy.
TEST_F(MultiDataVisibilityPolicyTest, OneNonNull) {
  EXPECT_CALL(*policy1_, GetClassVisibility(_))
      .WillOnce(Return(ByMove(nullptr)));

  DataVisibilityPolicy::Class* class1_ptr = class1_.get();
  EXPECT_CALL(*policy2_, GetClassVisibility(_))
      .WillOnce(Return(ByMove(std::move(class1_))));

  EXPECT_CALL(*policy3_, GetClassVisibility(_))
      .WillOnce(Return(ByMove(nullptr)));

  MultiDataVisibilityPolicy policy(
      std::move(policy1_),
      std::move(policy2_),
      std::move(policy3_));

  EXPECT_EQ(class1_ptr, policy.GetClassVisibility(nullptr).get());
}


// All child classes indicate the field is visible.
TEST_F(MultiDataVisibilityPolicyTest, FieldVisible) {
  EXPECT_CALL(*class1_, IsFieldVisible("name", 1234))
      .WillOnce(Return(true));

  EXPECT_CALL(*policy1_, GetClassVisibility(_))
      .WillOnce(Return(ByMove(std::move(class1_))));

  EXPECT_CALL(*class2_, IsFieldVisible("name", 1234))
      .WillOnce(Return(true));

  EXPECT_CALL(*policy2_, GetClassVisibility(_))
      .WillOnce(Return(ByMove(std::move(class2_))));

  MultiDataVisibilityPolicy policy(
      std::move(policy1_),
      std::move(policy2_));

  std::unique_ptr<DataVisibilityPolicy::Class> class_policy =
      policy.GetClassVisibility(nullptr);

  EXPECT_TRUE(class_policy->IsFieldVisible("name", 1234));
}


// One child class indicates the field is not visible.
TEST_F(MultiDataVisibilityPolicyTest, FieldNotVisible) {
  EXPECT_CALL(*class1_, IsFieldVisible("name", 1234))
      .WillOnce(Return(true));

  EXPECT_CALL(*policy1_, GetClassVisibility(_))
      .WillOnce(Return(ByMove(std::move(class1_))));

  EXPECT_CALL(*class2_, IsFieldVisible("name", 1234))
      .WillOnce(Return(false));

  EXPECT_CALL(*policy2_, GetClassVisibility(_))
      .WillOnce(Return(ByMove(std::move(class2_))));

  MultiDataVisibilityPolicy policy(
      std::move(policy1_),
      std::move(policy2_));

  std::unique_ptr<DataVisibilityPolicy::Class> class_policy =
      policy.GetClassVisibility(nullptr);

  EXPECT_FALSE(class_policy->IsFieldVisible("name", 1234));
}


// All child classes indicate the field data is visible.
TEST_F(MultiDataVisibilityPolicyTest, FieldDataVisible) {
  EXPECT_CALL(*class1_, IsFieldDataVisible("name", 1234, _))
      .WillOnce(Return(true));

  EXPECT_CALL(*policy1_, GetClassVisibility(_))
      .WillOnce(Return(ByMove(std::move(class1_))));

  EXPECT_CALL(*class2_, IsFieldDataVisible("name", 1234, _))
      .WillOnce(Return(true));

  EXPECT_CALL(*policy2_, GetClassVisibility(_))
      .WillOnce(Return(ByMove(std::move(class2_))));

  MultiDataVisibilityPolicy policy(
      std::move(policy1_),
      std::move(policy2_));

  std::unique_ptr<DataVisibilityPolicy::Class> class_policy =
      policy.GetClassVisibility(nullptr);

  std::string reason;
  EXPECT_TRUE(class_policy->IsFieldDataVisible("name", 1234, &reason));
}


// One child class indicates the field data is not visible.
TEST_F(MultiDataVisibilityPolicyTest, FieldDataNotVisible) {
  EXPECT_CALL(*class1_, IsFieldDataVisible("name", 1234, _))
      .WillOnce(Return(true));

  EXPECT_CALL(*policy1_, GetClassVisibility(_))
      .WillOnce(Return(ByMove(std::move(class1_))));

  EXPECT_CALL(*class2_, IsFieldDataVisible("name", 1234, _))
      .WillOnce(DoAll(SetArgPointee<2>("reason"), Return(false)));

  EXPECT_CALL(*policy2_, GetClassVisibility(_))
      .WillOnce(Return(ByMove(std::move(class2_))));

  MultiDataVisibilityPolicy policy(
      std::move(policy1_),
      std::move(policy2_));

  std::unique_ptr<DataVisibilityPolicy::Class> class_policy =
      policy.GetClassVisibility(nullptr);

  std::string reason;
  EXPECT_FALSE(class_policy->IsFieldDataVisible("name", 1234, &reason));
  EXPECT_EQ("reason", reason);
}


// All child classes indicate the method is visible.
TEST_F(MultiDataVisibilityPolicyTest, MethodVisible) {
  EXPECT_CALL(*class1_, IsMethodVisible("name", "sig", 1234))
      .WillOnce(Return(true));

  EXPECT_CALL(*policy1_, GetClassVisibility(_))
      .WillOnce(Return(ByMove(std::move(class1_))));

  EXPECT_CALL(*class2_, IsMethodVisible("name", "sig", 1234))
      .WillOnce(Return(true));

  EXPECT_CALL(*policy2_, GetClassVisibility(_))
      .WillOnce(Return(ByMove(std::move(class2_))));

  MultiDataVisibilityPolicy policy(
      std::move(policy1_),
      std::move(policy2_));

  std::unique_ptr<DataVisibilityPolicy::Class> class_policy =
      policy.GetClassVisibility(nullptr);

  EXPECT_TRUE(class_policy->IsMethodVisible("name", "sig", 1234));
}


// One child class indicates the method is not visible.
TEST_F(MultiDataVisibilityPolicyTest, MethodNotVisible) {
  EXPECT_CALL(*class1_, IsMethodVisible("name", "sig", 1234))
      .WillOnce(Return(true));

  EXPECT_CALL(*policy1_, GetClassVisibility(_))
      .WillOnce(Return(ByMove(std::move(class1_))));

  EXPECT_CALL(*class2_, IsMethodVisible("name", "sig", 1234))
      .WillOnce(Return(false));

  EXPECT_CALL(*policy2_, GetClassVisibility(_))
      .WillOnce(Return(ByMove(std::move(class2_))));

  MultiDataVisibilityPolicy policy(
      std::move(policy1_),
      std::move(policy2_));

  std::unique_ptr<DataVisibilityPolicy::Class> class_policy =
      policy.GetClassVisibility(nullptr);

  EXPECT_FALSE(class_policy->IsMethodVisible("name", "sig", 1234));
}


// All child classes indicate the variable is visible.
TEST_F(MultiDataVisibilityPolicyTest, VariableVisible) {
  EXPECT_CALL(*class1_, IsVariableVisible("name", "sig", "vname"))
      .WillOnce(Return(true));

  EXPECT_CALL(*policy1_, GetClassVisibility(_))
      .WillOnce(Return(ByMove(std::move(class1_))));

  EXPECT_CALL(*class2_, IsVariableVisible("name", "sig", "vname"))
      .WillOnce(Return(true));

  EXPECT_CALL(*policy2_, GetClassVisibility(_))
      .WillOnce(Return(ByMove(std::move(class2_))));

  MultiDataVisibilityPolicy policy(
      std::move(policy1_),
      std::move(policy2_));

  std::unique_ptr<DataVisibilityPolicy::Class> class_policy =
      policy.GetClassVisibility(nullptr);

  EXPECT_TRUE(class_policy->IsVariableVisible("name", "sig", "vname"));
}


// One child class indicates the variable is not visible.
TEST_F(MultiDataVisibilityPolicyTest, VariableNotVisible) {
  EXPECT_CALL(*class1_, IsVariableVisible("name", "sig", "vname"))
      .WillOnce(Return(true));

  EXPECT_CALL(*policy1_, GetClassVisibility(_))
      .WillOnce(Return(ByMove(std::move(class1_))));

  EXPECT_CALL(*class2_, IsVariableVisible("name", "sig", "vname"))
      .WillOnce(Return(false));

  EXPECT_CALL(*policy2_, GetClassVisibility(_))
      .WillOnce(Return(ByMove(std::move(class2_))));

  MultiDataVisibilityPolicy policy(
      std::move(policy1_),
      std::move(policy2_));

  std::unique_ptr<DataVisibilityPolicy::Class> class_policy =
      policy.GetClassVisibility(nullptr);

  EXPECT_FALSE(class_policy->IsVariableVisible("name", "sig", "vname"));
}


// All child classes indicate the variable data is visible.
TEST_F(MultiDataVisibilityPolicyTest, VariableDataVisible) {
  EXPECT_CALL(*class1_, IsVariableDataVisible("name", "sig", "vname", _))
      .WillOnce(Return(true));

  EXPECT_CALL(*policy1_, GetClassVisibility(_))
      .WillOnce(Return(ByMove(std::move(class1_))));

  EXPECT_CALL(*class2_, IsVariableDataVisible("name", "sig", "vname", _))
      .WillOnce(Return(true));

  EXPECT_CALL(*policy2_, GetClassVisibility(_))
      .WillOnce(Return(ByMove(std::move(class2_))));

  MultiDataVisibilityPolicy policy(
      std::move(policy1_),
      std::move(policy2_));

  std::unique_ptr<DataVisibilityPolicy::Class> class_policy =
      policy.GetClassVisibility(nullptr);

  std::string reason;
  EXPECT_TRUE(class_policy->IsVariableDataVisible(
      "name", "sig", "vname", &reason));
}


// One child class indicates the variable data is not visible.
TEST_F(MultiDataVisibilityPolicyTest, VariableDataNotVisible) {
  EXPECT_CALL(*class1_, IsVariableDataVisible("name", "sig", "vname", _))
      .WillOnce(Return(true));

  EXPECT_CALL(*policy1_, GetClassVisibility(_))
      .WillOnce(Return(ByMove(std::move(class1_))));

  EXPECT_CALL(*class2_, IsVariableDataVisible("name", "sig", "vname", _))
      .WillOnce(DoAll(SetArgPointee<3>("reason"), Return(false)));

  EXPECT_CALL(*policy2_, GetClassVisibility(_))
      .WillOnce(Return(ByMove(std::move(class2_))));

  MultiDataVisibilityPolicy policy(
      std::move(policy1_),
      std::move(policy2_));

  std::unique_ptr<DataVisibilityPolicy::Class> class_policy =
      policy.GetClassVisibility(nullptr);

  std::string reason;
  EXPECT_FALSE(class_policy->IsVariableDataVisible(
      "name", "sig", "vname", &reason));
  EXPECT_EQ("reason", reason);
}

}  // namespace cdbg
}  // namespace devtools
