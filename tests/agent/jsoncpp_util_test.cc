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

#include "src/agent/jsoncpp_util.h"

#include "gmock/gmock.h"

namespace devtools {
namespace cdbg {

TEST(JsonCppUtilTest, StringAttributeAsString) {
  Json::Value root;
  root["a"] = Json::Value("hello");

  EXPECT_EQ("hello", JsonCppGetString(root, "a"));
}


TEST(JsonCppUtilTest, StringAttributeAsBool) {
  Json::Value root;
  root["t"] = Json::Value(true);
  root["f"] = Json::Value(false);

  EXPECT_TRUE(JsonCppGetBool(root, "t", false));
  EXPECT_FALSE(JsonCppGetBool(root, "f", true));
}


TEST(JsonCppUtilTest, StringAttributeAsInt) {
  Json::Value root;
  root["i"] = Json::Value(34875643);

  EXPECT_EQ(34875643, JsonCppGetInt(root, "i", 0));
}


TEST(JsonCppUtilTest, MissingAttribute) {
  Json::Value root;

  EXPECT_EQ("", JsonCppGetString(root, "missing"));
  EXPECT_TRUE(JsonCppGetBool(root, "missing", true));
  EXPECT_EQ(734, JsonCppGetInt(root, "missing", 734));
}


TEST(JsonCppUtilTest, StringAttributeInvalidType) {
  Json::Value root;
  root["x"] = Json::Value(123);

  EXPECT_EQ("", JsonCppGetString(root, "x"));
}


TEST(JsonCppUtilTest, BoolAttributeInvalidType) {
  Json::Value root;
  root["x"] = Json::Value(123);

  EXPECT_FALSE(JsonCppGetBool(root, "x", false));
}


TEST(JsonCppUtilTest, IntAttributeInvalidType) {
  Json::Value root;
  root["x"] = Json::Value("123");

  EXPECT_EQ(0, JsonCppGetInt(root, "x", 0));
}

}  // namespace cdbg
}  // namespace devtools
