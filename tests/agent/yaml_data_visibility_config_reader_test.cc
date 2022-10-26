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

#include "src/agent/yaml_data_visibility_config_reader.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_object.h"
#include "mock_stringwriter.h"
#include "mock_yamlconfigparser.h"
#include "src/agent/debuggee_labels.h"
#include "tests/agent/fake_jni.h"
#include "tests/agent/mock_class_path_lookup.h"
#include "tests/agent/mock_jni_env.h"
#include "tests/agent/mock_jvmti_env.h"

namespace devtools {
namespace cdbg {

using testing::_;
using testing::Eq;
using testing::ByMove;
using testing::NiceMock;
using testing::Return;

class YamlDataVisibilityConfigReaderTest : public ::testing::Test {
 protected:
  YamlDataVisibilityConfigReaderTest()
    : fake_jni_(&jni_env_full_),
      global_jvm_(fake_jni_.jvmti(), fake_jni_.jni()) {
  }

  void SetUp() override {
    jniproxy::InjectYamlConfigParser(&yaml_config_parser_);
    // StringWriter is needed because formatting the exception strings
    // that may occur while paring YAML happens in java space and uses
    // StringWriter.
    jniproxy::InjectStringWriter(&string_writer_);
  }

  void TearDown() override {
    jniproxy::InjectYamlConfigParser(nullptr);
    jniproxy::InjectStringWriter(nullptr);
  }

  NiceMock<jniproxy::MockStringWriter> string_writer_;
  NiceMock<jniproxy::MockYamlConfigParser> yaml_config_parser_;
  NiceMock<MockJNIEnv> jni_env_full_;
  FakeJni fake_jni_;
  GlobalJvmEnv global_jvm_;
  MockClassPathLookup class_path_lookup_;
  std::string blocklist_source_;
};

TEST_F(YamlDataVisibilityConfigReaderTest, NoConfigFound) {
  EXPECT_CALL(class_path_lookup_,
              ReadApplicationResource("debugger-blocklist.yaml"))
      .WillOnce(Return(std::set<std::string>()));

  EXPECT_CALL(class_path_lookup_,
              ReadApplicationResource("debugger-blacklist.yaml"))
      .WillOnce(Return(std::set<std::string>()));

  GlobDataVisibilityPolicy::Config config = ReadYamlDataVisibilityConfiguration(
      &class_path_lookup_, &blocklist_source_);

  EXPECT_FALSE(config.blocklists.Matches("foo"));
  EXPECT_FALSE(config.blocklist_exceptions.Matches("foo"));
  EXPECT_THAT(blocklist_source_, Eq(DebuggeeLabels::kBlocklistSourceNone));
}

TEST_F(YamlDataVisibilityConfigReaderTest, BlocklistFound) {
  std::set<std::string> yaml_config({"config"});

  EXPECT_CALL(class_path_lookup_,
              ReadApplicationResource("debugger-blocklist.yaml"))
      .WillOnce(Return(yaml_config));

  EXPECT_CALL(jni_env_full_, GetArrayLength(_)).WillOnce(Return(1));

  jstring foo_a = fake_jni_.CreateNewJavaString("foo.a");
  EXPECT_CALL(jni_env_full_, GetObjectArrayElement(_, 0))
      .WillOnce(Return(ByMove(foo_a)));

  ExceptionOr<JniLocalRef> array(
      nullptr, nullptr,
      JniLocalRef(fake_jni_.CreateNewObject(FakeJni::StockClass::StringArray)));

  EXPECT_CALL(yaml_config_parser_, getBlocklistPatterns(_))
      .WillOnce(Return(ByMove(std::move(array))));

  GlobDataVisibilityPolicy::Config config = ReadYamlDataVisibilityConfiguration(
      &class_path_lookup_, &blocklist_source_);

  EXPECT_TRUE(config.blocklists.Matches("foo.a"));
  EXPECT_FALSE(config.blocklist_exceptions.Matches("foo.b"));
  EXPECT_THAT(blocklist_source_, Eq(DebuggeeLabels::kBlocklistSourceFile));
}

TEST_F(YamlDataVisibilityConfigReaderTest,
       BlocklistNotFoundButDeprecatedFound) {
  std::set<std::string> not_found_yaml_config;
  std::set<std::string> found_yaml_config({"config"});

  EXPECT_CALL(class_path_lookup_,
              ReadApplicationResource("debugger-blocklist.yaml"))
      .WillOnce(Return(not_found_yaml_config));

  EXPECT_CALL(class_path_lookup_,
              ReadApplicationResource("debugger-blacklist.yaml"))
      .WillOnce(Return(found_yaml_config));

  EXPECT_CALL(jni_env_full_, GetArrayLength(_)).WillOnce(Return(1));

  jstring foo_a = fake_jni_.CreateNewJavaString("foo.a");
  EXPECT_CALL(jni_env_full_, GetObjectArrayElement(_, 0))
      .WillOnce(Return(ByMove(foo_a)));

  ExceptionOr<JniLocalRef> array(
      nullptr, nullptr,
      JniLocalRef(fake_jni_.CreateNewObject(FakeJni::StockClass::StringArray)));

  EXPECT_CALL(yaml_config_parser_, getBlocklistPatterns(_))
      .WillOnce(Return(ByMove(std::move(array))));

  GlobDataVisibilityPolicy::Config config = ReadYamlDataVisibilityConfiguration(
      &class_path_lookup_, &blocklist_source_);

  EXPECT_TRUE(config.blocklists.Matches("foo.a"));
  EXPECT_FALSE(config.blocklist_exceptions.Matches("foo.b"));
  EXPECT_THAT(blocklist_source_,
              Eq(DebuggeeLabels::kBlocklistSourceDeprecatedFile));
}

TEST_F(YamlDataVisibilityConfigReaderTest, MultipleConfigsFound) {
  std::set<std::string> yaml_config({"config1", "config2"});

  EXPECT_CALL(class_path_lookup_, ReadApplicationResource(_))
      .WillOnce(Return(yaml_config));

  GlobDataVisibilityPolicy::Config config = ReadYamlDataVisibilityConfiguration(
      &class_path_lookup_, &blocklist_source_);

  EXPECT_GT(config.parse_error.length(), 0);
  EXPECT_THAT(blocklist_source_, Eq(DebuggeeLabels::kBlocklistSourceFile));
}

TEST_F(YamlDataVisibilityConfigReaderTest, BadBlocklistConfig) {
  std::set<std::string> yaml_config({"config"});

  EXPECT_CALL(class_path_lookup_,
              ReadApplicationResource("debugger-blocklist.yaml"))
      .WillOnce(Return(yaml_config));

  ExceptionOr<JniLocalRef> exception(
      nullptr,
      JniLocalRef(fake_jni_.CreateNewObject(FakeJni::StockClass::Object)),
      nullptr);

  EXPECT_CALL(yaml_config_parser_, NewObject(_))
      .WillOnce(Return(ByMove(std::move(exception))));

  GlobDataVisibilityPolicy::Config config = ReadYamlDataVisibilityConfiguration(
      &class_path_lookup_, &blocklist_source_);

  EXPECT_EQ(config.parse_error,
            "Errors parsing debugger-blocklist.yaml. Please contact your "
            "system administrator.");
  EXPECT_THAT(blocklist_source_, Eq(DebuggeeLabels::kBlocklistSourceFile));
}

TEST_F(YamlDataVisibilityConfigReaderTest, BadBackupConfig) {
  std::set<std::string> not_found_yaml_config;
  std::set<std::string> found_yaml_config({"config"});

  EXPECT_CALL(class_path_lookup_,
              ReadApplicationResource("debugger-blocklist.yaml"))
      .WillOnce(Return(not_found_yaml_config));

  EXPECT_CALL(class_path_lookup_,
              ReadApplicationResource("debugger-blacklist.yaml"))
      .WillOnce(Return(found_yaml_config));

  ExceptionOr<JniLocalRef> exception(
      nullptr,
      JniLocalRef(fake_jni_.CreateNewObject(FakeJni::StockClass::Object)),
      nullptr);

  EXPECT_CALL(yaml_config_parser_, NewObject(_))
      .WillOnce(Return(ByMove(std::move(exception))));

  GlobDataVisibilityPolicy::Config config = ReadYamlDataVisibilityConfiguration(
      &class_path_lookup_, &blocklist_source_);

  EXPECT_EQ(config.parse_error,
            "Errors parsing debugger-blacklist.yaml. Please contact your "
            "system administrator.");
  EXPECT_THAT(blocklist_source_,
              Eq(DebuggeeLabels::kBlocklistSourceDeprecatedFile));
}

TEST_F(YamlDataVisibilityConfigReaderTest, GetBlocklistPatternsFailed) {
  std::set<std::string> yaml_config({"config"});

  EXPECT_CALL(class_path_lookup_, ReadApplicationResource(_))
      .WillOnce(Return(yaml_config));

  ExceptionOr<JniLocalRef> exception(
      nullptr,
      JniLocalRef(fake_jni_.CreateNewObject(FakeJni::StockClass::Object)),
      nullptr);

  EXPECT_CALL(yaml_config_parser_, getBlocklistPatterns(_))
      .WillOnce(Return(ByMove(std::move(exception))));

  GlobDataVisibilityPolicy::Config config = ReadYamlDataVisibilityConfiguration(
      &class_path_lookup_, &blocklist_source_);

  EXPECT_GT(config.parse_error.length(), 0);
  EXPECT_THAT(blocklist_source_, Eq(DebuggeeLabels::kBlocklistSourceFile));
}


TEST_F(YamlDataVisibilityConfigReaderTest, GetBlocklistExceptionPatternsFail) {
  std::set<std::string> yaml_config({"config"});

  EXPECT_CALL(class_path_lookup_, ReadApplicationResource(_))
      .WillOnce(Return(yaml_config));

  ExceptionOr<JniLocalRef> exception(
      nullptr,
      JniLocalRef(fake_jni_.CreateNewObject(FakeJni::StockClass::Object)),
      nullptr);

  EXPECT_CALL(yaml_config_parser_, getBlocklistExceptionPatterns(_))
      .WillOnce(Return(ByMove(std::move(exception))));

  GlobDataVisibilityPolicy::Config config = ReadYamlDataVisibilityConfiguration(
      &class_path_lookup_, &blocklist_source_);

  EXPECT_GT(config.parse_error.length(), 0);
  EXPECT_THAT(blocklist_source_, Eq(DebuggeeLabels::kBlocklistSourceFile));
}


TEST_F(YamlDataVisibilityConfigReaderTest, TestConfigWithBlocklists) {
  std::set<std::string> yaml_config({"config"});

  EXPECT_CALL(class_path_lookup_, ReadApplicationResource(_))
      .WillOnce(Return(yaml_config));

  EXPECT_CALL(jni_env_full_, GetArrayLength(_))
      .WillOnce(Return(2));

  jstring foo_a = fake_jni_.CreateNewJavaString("foo.a");
  EXPECT_CALL(jni_env_full_, GetObjectArrayElement(_, 0))
      .WillOnce(Return(ByMove(foo_a)));

  jstring foo_b = fake_jni_.CreateNewJavaString("foo.b");
  EXPECT_CALL(jni_env_full_, GetObjectArrayElement(_, 1))
      .WillOnce(Return(ByMove(foo_b)));

  ExceptionOr<JniLocalRef> array(
      nullptr,
      nullptr,
      JniLocalRef(fake_jni_.CreateNewObject(FakeJni::StockClass::StringArray)));

  EXPECT_CALL(yaml_config_parser_, getBlocklistPatterns(_))
      .WillOnce(Return(ByMove(std::move(array))));

  GlobDataVisibilityPolicy::Config config = ReadYamlDataVisibilityConfiguration(
      &class_path_lookup_, &blocklist_source_);

  EXPECT_FALSE(config.blocklist_exceptions.Matches("foo"));

  EXPECT_TRUE(config.blocklists.Matches("foo.a"));
  EXPECT_TRUE(config.blocklists.Matches("foo.b"));
  EXPECT_FALSE(config.blocklists.Matches("foo.c"));
  EXPECT_FALSE(config.blocklists.Matches("foo.d"));
}


TEST_F(YamlDataVisibilityConfigReaderTest, TestConfigWithBlocklistExceptions) {
  std::set<std::string> yaml_config({"config"});

  EXPECT_CALL(class_path_lookup_, ReadApplicationResource(_))
      .WillOnce(Return(yaml_config));

  EXPECT_CALL(jni_env_full_, GetArrayLength(_))
      .WillOnce(Return(2));

  jstring foo_a = fake_jni_.CreateNewJavaString("foo.a");
  EXPECT_CALL(jni_env_full_, GetObjectArrayElement(_, 0))
      .WillOnce(Return(ByMove(foo_a)));

  jstring foo_b = fake_jni_.CreateNewJavaString("foo.b");
  EXPECT_CALL(jni_env_full_, GetObjectArrayElement(_, 1))
      .WillOnce(Return(ByMove(foo_b)));

  ExceptionOr<JniLocalRef> array(
      nullptr,
      nullptr,
      JniLocalRef(fake_jni_.CreateNewObject(FakeJni::StockClass::StringArray)));

  EXPECT_CALL(yaml_config_parser_, getBlocklistExceptionPatterns(_))
      .WillOnce(Return(ByMove(std::move(array))));

  GlobDataVisibilityPolicy::Config config = ReadYamlDataVisibilityConfiguration(
      &class_path_lookup_, &blocklist_source_);

  EXPECT_FALSE(config.blocklists.Matches("foo"));

  EXPECT_TRUE(config.blocklist_exceptions.Matches("foo.a"));
  EXPECT_TRUE(config.blocklist_exceptions.Matches("foo.b"));
  EXPECT_FALSE(config.blocklist_exceptions.Matches("foo.c"));
}

}  // namespace cdbg
}  // namespace devtools
