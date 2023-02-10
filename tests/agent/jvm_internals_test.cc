/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "src/agent/jvm_internals.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "tests/agent/file_utils.h"

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

namespace devtools {
namespace cdbg {

class JvmInternalsTest : public ::testing::Test {
 protected:
  JvmInternalsTest()
      : temp_path_(new TempPath), agent_dir_(temp_path_->path()) {}

  std::string CreateFile(const std::string& file_name) {
    std::string full_path = JoinPath(agent_dir_, file_name);
    SetFileContents(full_path, "foo");
    return full_path;
  }

  std::unique_ptr<TempPath> temp_path_;
  std::string agent_dir_;
};

TEST_F(JvmInternalsTest, NoFiles) {
  EXPECT_THAT(JvmInternals::GetInternalsJarPaths(agent_dir_), IsEmpty());
}

TEST_F(JvmInternalsTest, SingleMainJar) {
  std::string jar_file_full_path = CreateFile("cdbg_java_agent_internals.jar");
  EXPECT_THAT(JvmInternals::GetInternalsJarPaths(agent_dir_),
              UnorderedElementsAre(jar_file_full_path));
}

TEST_F(JvmInternalsTest, SplitJarsOne) {
  std::string jar0_file_full_path =
      CreateFile("cdbg_java_agent_internals-0000.jar");
  EXPECT_THAT(JvmInternals::GetInternalsJarPaths(agent_dir_),
              UnorderedElementsAre(jar0_file_full_path));
}

TEST_F(JvmInternalsTest, SplitJarsMultiple) {
  std::string jar0_file_full_path =
      CreateFile("cdbg_java_agent_internals-0000.jar");

  std::string jar1_file_full_path =
      CreateFile("cdbg_java_agent_internals-0001.jar");

  std::string jar2_file_full_path =
      CreateFile("cdbg_java_agent_internals-0002.jar");

  EXPECT_THAT(JvmInternals::GetInternalsJarPaths(agent_dir_),
              UnorderedElementsAre(jar0_file_full_path, jar1_file_full_path, jar2_file_full_path));
}

// This test simply ensures the code can handle differing numbers of digits, as
// the jar splitter utility can be configured differently, we ensure the code
// isn't hardcoded to expect 4 digits.
TEST_F(JvmInternalsTest, SplitJarsDifferentNumberOfDigits) {
  std::string jar1_file_full_path =
      CreateFile("cdbg_java_agent_internals-1.jar");

  std::string jar2_file_full_path =
      CreateFile("cdbg_java_agent_internals-02.jar");

  std::string jar3_file_full_path =
      CreateFile("cdbg_java_agent_internals-003.jar");

  EXPECT_THAT(JvmInternals::GetInternalsJarPaths(agent_dir_),
              UnorderedElementsAre(jar1_file_full_path, jar2_file_full_path, jar3_file_full_path));
}

}  // namespace cdbg
}  // namespace devtools
