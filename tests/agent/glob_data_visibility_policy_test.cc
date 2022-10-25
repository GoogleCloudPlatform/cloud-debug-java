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

#include "src/agent/glob_data_visibility_policy.h"

#include <cstdint>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "re2/re2.h"
#include "tests/agent/fake_jni.h"
#include "tests/agent/mock_jvmti_env.h"

namespace devtools {
namespace cdbg {

using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::WithArg;

static const jclass kClass = reinterpret_cast<jclass>(0x12345678);

class GlobDataVisibilityPolicyTest : public ::testing::Test {
 protected:
  GlobDataVisibilityPolicyTest()
      : fake_jni_(&jvmti_),
        global_jvm_(&jvmti_, fake_jni_.jni()) {
  }

  void SetUp() override {
    EXPECT_CALL(jvmti_, Deallocate(NotNull()))
        .WillRepeatedly(Invoke([] (unsigned char* buffer) {
          delete[] buffer;
          return JVMTI_ERROR_NONE;
        }));
  }

  void SetClassSignature(const std::string& signature) {
    EXPECT_CALL(jvmti_, GetClassSignature(kClass, NotNull(), nullptr))
        .WillRepeatedly(WithArg<1>(Invoke([signature] (char** buffer) {
          *buffer = new char[signature.size() + 1];
          memcpy(*buffer, signature.c_str(), signature.size() + 1);
          return JVMTI_ERROR_NONE;
        })));
  }

  void CheckIsBlocked(const std::vector<std::string>& signatures,
                          const GlobDataVisibilityPolicy::Config& config) {
    GlobDataVisibilityPolicy data_visibility;
    data_visibility.SetConfig(config);
    for (const std::string& signature : signatures) {
      SetClassSignature(signature);
      auto class_visibility = data_visibility.GetClassVisibility(kClass);
      ASSERT_NE(nullptr, class_visibility);

      std::string reason;
      EXPECT_TRUE(class_visibility->IsFieldVisible("someField", 0));
      EXPECT_FALSE(class_visibility->IsMethodVisible("myMethod", "()V", 0));
      EXPECT_FALSE(
          class_visibility->IsFieldDataVisible("someField", 0, &reason));
      EXPECT_TRUE(
          class_visibility->IsVariableVisible("myMethod", "()V", "var"));
      EXPECT_FALSE(
          class_visibility->IsVariableDataVisible(
              "myMethod", "()V", "var", &reason));
    }
  }

  void CheckIsNull(const std::vector<std::string>& paths,
                   const GlobDataVisibilityPolicy::Config& config) {
    GlobDataVisibilityPolicy data_visibility;
    data_visibility.SetConfig(config);
    for (const std::string& path : paths) {
      SetClassSignature(path);
      auto class_visibility = data_visibility.GetClassVisibility(kClass);
      ASSERT_EQ(nullptr, class_visibility);
    }
  }

 protected:
  MockJvmtiEnv jvmti_;
  FakeJni fake_jni_;
  GlobalJvmEnv global_jvm_;
};


TEST_F(GlobDataVisibilityPolicyTest, BadClassSignature) {
  std::vector<std::string> bad_signatures = {
      "Lcom/public/MyClass",
      "com/public/MyClass;",
      "L;",
      "L",
      ";"
      "",
  };

  CheckIsNull(bad_signatures, {});
}


TEST_F(GlobDataVisibilityPolicyTest, NothingIsBlocked) {
  std::vector<std::string> not_blocked = {
      "Lcom/public/MyClass;",
      "Lcom/public/MyClass$Inner1;",
      "Lcom/public/MyClass$Inner1$Inner2;",
  };

  GlobDataVisibilityPolicy::Config config;
  config.blocklists.Prepare();
  config.blocklist_exceptions.Prepare();
  CheckIsNull(not_blocked, config);
}


TEST_F(GlobDataVisibilityPolicyTest, PackageBlocked) {
  std::vector<std::string> blocked = {
      "Lcom/secret/MyClass;",
      "Lcom/secret/MyClass$Inner1;",
      "Lcom/secret/MyClass$Inner1$Inner2;",
  };

  std::vector<std::string> not_blocked = {
      "Lcom/public/MyClass;",
      "Lcom/public/MyClass$Inner1;",
      "Lcom/public/MyClass$Inner1$Inner2;",
  };

  GlobDataVisibilityPolicy::Config config;
  config.blocklists.Add("com.secret");
  config.blocklists.Prepare();
  config.blocklist_exceptions.Prepare();

  CheckIsBlocked(blocked, config);
  CheckIsNull(not_blocked, config);
}


TEST_F(GlobDataVisibilityPolicyTest, PackageIsNotBlocked) {
  std::vector<std::string> blocked = {
      "Lcom/secret/MyClass;",
      "Lcom/secret/MyClass$Inner1;",
      "Lcom/secret/MyClass$Inner1$Inner2;",
  };

  std::vector<std::string> not_blocked = {
      "Lcom/public/MyClass;",
      "Lcom/public/MyClass$Inner1;",
      "Lcom/public/MyClass$Inner1$Inner2;",
  };

  GlobDataVisibilityPolicy::Config config;
  config.blocklists.Add("!com.public");
  config.blocklists.Prepare();
  config.blocklist_exceptions.Prepare();

  CheckIsBlocked(blocked, config);
  CheckIsNull(not_blocked, config);
}


TEST_F(GlobDataVisibilityPolicyTest, PackageWhitelisted) {
  std::vector<std::string> blocked = {
      "Lcom/secret/MyClass;",
      "Lcom/secret/MyClass$Inner1;",
      "Lcom/secret/MyClass$Inner1$Inner2;",
  };

  std::vector<std::string> not_blocked = {
      "Lcom/public/MyClass;",
      "Lcom/public/MyClass$Inner1;",
      "Lcom/public/MyClass$Inner1$Inner2;",
  };

  GlobDataVisibilityPolicy::Config config;
  config.blocklists.Add("*");
  config.blocklist_exceptions.Add("com.public");
  config.blocklists.Prepare();
  config.blocklist_exceptions.Prepare();

  CheckIsBlocked(blocked, config);
  CheckIsNull(not_blocked, config);
}


TEST_F(GlobDataVisibilityPolicyTest, PackageWhitelistedWithInverse) {
  std::vector<std::string> blocked = {
      "Lcom/secret/MyClass;",
      "Lcom/secret/MyClass$Inner1;",
      "Lcom/secret/MyClass$Inner1$Inner2;",
  };

  std::vector<std::string> not_blocked = {
      "Lcom/public/MyClass;",
      "Lcom/public/MyClass$Inner1;",
      "Lcom/public/MyClass$Inner1$Inner2;",
  };

  GlobDataVisibilityPolicy::Config config;
  config.blocklists.Add("!com.test");
  config.blocklist_exceptions.Add("com.public");
  config.blocklists.Prepare();
  config.blocklist_exceptions.Prepare();

  CheckIsBlocked(blocked, config);
  CheckIsNull(not_blocked, config);
}


TEST_F(GlobDataVisibilityPolicyTest, ClassBlocked) {
  std::vector<std::string> blocked = {
      "Lcom/secret/MyClass;",
      "Lcom/secret/MyClass$Inner1;",
      "Lcom/secret/MyClass$Inner1$Inner2;",
  };

  std::vector<std::string> not_blocked = {
      "Lcom/public/MyClass;",
      "Lcom/public/MyClass$Inner1;",
      "Lcom/public/MyClass$Inner1$Inner2;",
  };

  GlobDataVisibilityPolicy::Config config;
  config.blocklists.Add("com.secret.MyClass");
  config.blocklists.Prepare();
  config.blocklist_exceptions.Prepare();

  CheckIsBlocked(blocked, config);
  CheckIsNull(not_blocked, config);
}


TEST_F(GlobDataVisibilityPolicyTest, ClassWhitelisted) {
  std::vector<std::string> blocked = {
      "Lcom/secret/MyClass;",
      "Lcom/secret/MyClass$Inner1;",
      "Lcom/secret/MyClass$Inner1$Inner2;",
  };

  std::vector<std::string> not_blocked = {
      "Lcom/public/MyClass;",
      "Lcom/public/MyClass$Inner1;",
      "Lcom/public/MyClass$Inner1$Inner2;",
  };

  GlobDataVisibilityPolicy::Config config;
  config.blocklists.Add("*");
  config.blocklist_exceptions.Add("com.public.MyClass");
  config.blocklists.Prepare();
  config.blocklist_exceptions.Prepare();

  CheckIsBlocked(blocked, config);
  CheckIsNull(not_blocked, config);
}


TEST_F(GlobDataVisibilityPolicyTest, OnlyClassNotBlocked) {
  std::vector<std::string> blocked = {
      "Lcom/secret/MyClass;",
      "Lcom/secret/MyClass$Inner1;",
      "Lcom/secret/MyClass$Inner1$Inner2;",
  };

  std::vector<std::string> not_blocked = {
      "Lcom/public/MyClass;",
      "Lcom/public/MyClass$Inner1;",
      "Lcom/public/MyClass$Inner1$Inner2;",
  };

  GlobDataVisibilityPolicy::Config config;
  config.blocklists.Add("!com.public.MyClass");
  config.blocklists.Prepare();
  config.blocklist_exceptions.Prepare();

  CheckIsBlocked(blocked, config);
  CheckIsNull(not_blocked, config);
}

// Blocklist everything not in com.public
// Also blocklist com.public.User
// Allow com.public.User.Test (and children)
TEST_F(GlobDataVisibilityPolicyTest, ExceptionWithinNamespace) {
  std::vector<std::string> blocked = {
      "Lcom/secret/MyClass;",
      "Lcom/public/User;",
      "Lcom/public/User/Password;",
  };

  std::vector<std::string> not_blocked = {
      "Lcom/public/MyClass;",
      "Lcom/public/User/Test;",
      "Lcom/public/User/Test/UserTest;",
  };

  GlobDataVisibilityPolicy::Config config;
  config.blocklists.Add("!com.public");
  config.blocklists.Add("com.public.User");
  config.blocklist_exceptions.Add("com.public.User.Test");
  config.blocklists.Prepare();
  config.blocklist_exceptions.Prepare();

  CheckIsBlocked(blocked, config);
  CheckIsNull(not_blocked, config);
}


TEST_F(GlobDataVisibilityPolicyTest, ParseError) {
  GlobDataVisibilityPolicy::Config config;
  config.parse_error = "parse error";

  GlobDataVisibilityPolicy policy;
  policy.SetConfig(config);
  std::string setup_error;
  EXPECT_TRUE(policy.HasSetupError(&setup_error));
  EXPECT_EQ("parse error", setup_error);

  std::unique_ptr<DataVisibilityPolicy::Class> cls =
      policy.GetClassVisibility(nullptr);

  EXPECT_TRUE(cls->IsFieldVisible("name", 0));
  EXPECT_FALSE(cls->IsMethodVisible("name", "sig", 0));
  EXPECT_TRUE(cls->IsVariableVisible("method_name", "method_sig", "name"));

  std::string field_error;
  EXPECT_FALSE(cls->IsFieldDataVisible("name", 0, &field_error));
  EXPECT_EQ(config.parse_error, field_error);

  std::string variable_error;
  EXPECT_FALSE(cls->IsVariableDataVisible(
      "method_name",
      "method_sig",
      "name",
      &variable_error));
  EXPECT_EQ(config.parse_error, variable_error);
}


TEST_F(GlobDataVisibilityPolicyTest, Uninitialized) {
  GlobDataVisibilityPolicy policy;
  std::string error;
  EXPECT_TRUE(policy.HasSetupError(&error));

  std::unique_ptr<DataVisibilityPolicy::Class> cls =
      policy.GetClassVisibility(nullptr);

  EXPECT_FALSE(cls->IsMethodVisible("name", "sig", 0));
  EXPECT_FALSE(cls->IsFieldDataVisible("name", 0, &error));
  EXPECT_FALSE(cls->IsVariableDataVisible(
      "method_name",
      "method_sig",
      "name",
      &error));
}


TEST(GlobSetTest, MatchesAny) {
  GlobDataVisibilityPolicy::GlobSet glob;
  glob.Add("*");
  glob.Prepare();
  EXPECT_TRUE(glob.Matches("foo"));
  EXPECT_TRUE(glob.Matches(""));
}

TEST(GlobSetTest, MatchesNone) {
  GlobDataVisibilityPolicy::GlobSet glob;
  glob.Add("!*");
  glob.Prepare();
  EXPECT_FALSE(glob.Matches("foo"));
  EXPECT_FALSE(glob.Matches(""));
}

TEST(GlobSetTest, MatchesPrefix) {
  GlobDataVisibilityPolicy::GlobSet glob;
  glob.Add("foo*");
  glob.Prepare();
  EXPECT_TRUE(glob.Matches("foo"));
  EXPECT_TRUE(glob.Matches("foot"));
  EXPECT_FALSE(glob.Matches("fo"));
  EXPECT_FALSE(glob.Matches("fog"));
}

TEST(GlobSetTest, DoesNotMatchPrefix) {
  GlobDataVisibilityPolicy::GlobSet glob;
  glob.Add("!foo*");
  glob.Prepare();
  EXPECT_FALSE(glob.Matches("foo"));
  EXPECT_FALSE(glob.Matches("foot"));
  EXPECT_TRUE(glob.Matches("fo"));
  EXPECT_TRUE(glob.Matches("fog"));
}

TEST(GlobSetTest, MatchesSuffix) {
  GlobDataVisibilityPolicy::GlobSet glob;
  glob.Add("*foo");
  glob.Prepare();
  EXPECT_TRUE(glob.Matches("foo"));
  EXPECT_TRUE(glob.Matches("tfoo"));
  EXPECT_FALSE(glob.Matches("fo"));
  EXPECT_FALSE(glob.Matches("fog"));
  EXPECT_FALSE(glob.Matches("foot"));
  EXPECT_FALSE(glob.Matches(""));
}

TEST(GlobSetTest, DoesNotMatchSuffix) {
  GlobDataVisibilityPolicy::GlobSet glob;
  glob.Add("!*foo");
  glob.Prepare();
  EXPECT_FALSE(glob.Matches("foo"));
  EXPECT_FALSE(glob.Matches("tfoo"));
  EXPECT_TRUE(glob.Matches("fo"));
  EXPECT_TRUE(glob.Matches("fog"));
  EXPECT_TRUE(glob.Matches("foot"));
  EXPECT_TRUE(glob.Matches(""));
}

TEST(GlobSetTest, MatchesExact) {
  GlobDataVisibilityPolicy::GlobSet glob;
  glob.Add("foo");
  glob.Prepare();
  EXPECT_TRUE(glob.Matches("foo"));
  EXPECT_FALSE(glob.Matches("fo"));
  EXPECT_FALSE(glob.Matches("oo"));
  EXPECT_FALSE(glob.Matches("foot"));
  EXPECT_FALSE(glob.Matches("tfoo"));
  EXPECT_FALSE(glob.Matches(""));
}

TEST(GlobSetTest, DoesNotMatchExact) {
  GlobDataVisibilityPolicy::GlobSet glob;
  glob.Add("!foo");
  glob.Prepare();
  EXPECT_FALSE(glob.Matches("foo"));
  EXPECT_TRUE(glob.Matches("fo"));
  EXPECT_TRUE(glob.Matches("oo"));
  EXPECT_TRUE(glob.Matches("foot"));
  EXPECT_TRUE(glob.Matches("tfoo"));
  EXPECT_TRUE(glob.Matches(""));
}

TEST(GlobSetTest, MultipleGlobs) {
  GlobDataVisibilityPolicy::GlobSet glob;
  glob.Add("foo");
  glob.Add("bar");
  glob.Add("baz*");
  glob.Prepare();
  EXPECT_TRUE(glob.Matches("foo"));
  EXPECT_TRUE(glob.Matches("bar"));
  EXPECT_TRUE(glob.Matches("baz2"));
  EXPECT_FALSE(glob.Matches("abc"));
  EXPECT_FALSE(glob.Matches(""));
}

TEST(GlobSetTest, RestrictNamespace) {
  GlobDataVisibilityPolicy::GlobSet glob;
  glob.Add("!com.foo.*");
  glob.Add("com.foo.security.*");
  glob.Add("com.foo.user");
  glob.Prepare();
  EXPECT_TRUE(glob.Matches("java.util.Arrays"));
  EXPECT_TRUE(glob.Matches("com.foo.security.Token"));
  EXPECT_TRUE(glob.Matches("com.foo.user"));
  EXPECT_FALSE(glob.Matches("com.foo.test.TestCase"));
}

TEST(GlobSetTest, RestrictNamespaceNoGlobs) {
  GlobDataVisibilityPolicy::GlobSet glob;
  glob.Add("!com.foo");
  glob.Add("com.foo.security");
  glob.Add("com.foo.user");
  glob.Prepare();
  EXPECT_TRUE(glob.Matches("java.util.Arrays"));
  EXPECT_TRUE(glob.Matches("com.foo.security.Token"));
  EXPECT_TRUE(glob.Matches("com.foo.user"));
  EXPECT_FALSE(glob.Matches("com.foo.test.TestCase"));
}

TEST(GlobSetTest, Empty) {
  GlobDataVisibilityPolicy::GlobSet glob;
  EXPECT_TRUE(glob.Empty());
  glob.Add("foo");
  EXPECT_FALSE(glob.Empty());
}

TEST(GlobSetTest, InverseEmpty) {
  GlobDataVisibilityPolicy::GlobSet glob;
  EXPECT_TRUE(glob.Empty());
  glob.Add("!foo");
  EXPECT_FALSE(glob.Empty());
}

}  // namespace cdbg
}  // namespace devtools
