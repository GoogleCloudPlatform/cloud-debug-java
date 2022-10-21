/**
 * Copyright 2022 Google Inc. All Rights Reserved.
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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_FILE_UTILS_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_FILE_UTILS_H_

#include "common.h"

namespace devtools {
namespace cdbg {

/**
 * Utility class to create a temporary directory for used by tests. Upon
 * destruction it will delete the temporary directory and all of its contents
 * (all files and all subdirectories). So the instance needs to be kept alive
 * for the duration of the test.
 */
class TempPath {
 public:
  TempPath();
  ~TempPath();

  TempPath(const TempPath&) = delete;
  void operator=(const TempPath&) = delete;

  std::string path() const;

 private:
  string temp_path_;
};

/**
 * Utility to create the given filename and copy the contents of data into it.
 *
 * Return true on success, false otherwise.
 */
bool SetFileContents(const std::string& filename, const std::string& data);

/**
 * Utility to read the contents from the given filename.
 *
 * Return true on success, false otherwise.
 */
bool GetFileContents(const std::string& filename, std::string* data);

/**
 * Create a full filename by appending the filename to the given path.
 */
std::string JoinPath(const std::string& path, const std::string& filename);

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_FILE_UTILS_H_
