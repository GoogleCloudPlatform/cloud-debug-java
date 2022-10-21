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

#include "file_utils.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fstream>

#include "common.h"
#include "gmock/gmock.h"

namespace devtools {
namespace cdbg {

namespace {

// Done using the C library, as std::filesystem was introduced in C++17, and
// we're still targetting building with C++11.
void remove_all_files_and_dirs(const std::string& path) {
  DIR* dir;
  struct dirent* ent;

  dir = opendir(path.c_str());
  PCHECK(dir != nullptr) << ", directory: " << path;

  while ((ent = readdir(dir)) != NULL) {
    std::string ent_name = std::string(ent->d_name);

    if (ent_name == "." || ent_name == "..") {
      continue;
    }

    std::string ent_full_path_name = path + "/" + ent_name;

    if (ent->d_type == DT_DIR) {
      remove_all_files_and_dirs(ent_full_path_name);
    } else {
      PCHECK(unlink(ent_full_path_name.c_str()) == 0)
          << ", path: " << ent_full_path_name;
    }
  }

  closedir(dir);
  PCHECK(rmdir(path.c_str()) == 0) << ", directory name: " << path;
}

}  // namespace

/**
 * Small utility to wrap a c string and manage its memory. Needed for the call
 * to mkdtemp, which requires a writeable c string.
 *
 * To note, the value returned from the c_str() and data methods of std::string
 * are not useable usabled as is, because in C++11 they both return const char*,
 * so are not writable.
 */
class CString {
 public:
  CString(std::string str) {
    c_str_.reset(strdup(str.c_str()));
    PCHECK(c_str_.get() != nullptr);
  }

  char* c_str() { return c_str_.get(); }

 private:
  struct free_deleter {
    void operator()(void* x) { free(x); }
  };

  std::unique_ptr<char, free_deleter> c_str_;
};

TempPath::TempPath() {
  CString temp_path(::testing::TempDir() + "/cdbg-temp.XXXXXX");

  char* retval = mkdtemp(temp_path.c_str());
  PCHECK(retval != nullptr);
  temp_path_ = std::string(temp_path.c_str());
}

TempPath::~TempPath() {
  if (temp_path_ == "") {
    return;
  }

  remove_all_files_and_dirs(temp_path_);
  temp_path_ = "";
}

std::string TempPath::path() const { return temp_path_; }

bool SetFileContents(const std::string& filename, const std::string& data) {
  try {
    std::ofstream out(filename);
    out << data;
  } catch (std::exception& e) {
    LOG(ERROR) << "Error opening/writting to " << filename << ", " << e.what();
    return false;
  }

  return true;
}

bool GetFileContents(const std::string& filename, std::string* data) {
  try {
    std::ifstream in(filename);
    std::stringstream buffer;
    buffer << in.rdbuf();
    *data = buffer.str();
  } catch (std::exception& e) {
    LOG(ERROR) << "Error opening/reading from " << filename << ", " << e.what();
    return false;
  }

  return true;
}

std::string JoinPath(const std::string& path, const std::string& filename) {
  return path + '/' + filename;
}

}  // namespace cdbg
}  // namespace devtools
