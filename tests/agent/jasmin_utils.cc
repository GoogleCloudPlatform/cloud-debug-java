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

#include "tests/agent/jasmin_utils.h"

#include "jni_proxy_classfiletextifier.h"
#include "jni_proxy_jasmin_main.h"

#include "tests/agent/file_utils.h"

namespace devtools {
namespace cdbg {

// Builds Java class from assembly using Jasmin.
std::string Assemble(const std::string& asm_code) {
  TempPath temp_path;
  const std::string source_path = JoinPath(temp_path.path(), "source.j");
  const std::string destination_path = JoinPath(temp_path.path(), "TestClass.class");

  CHECK(SetFileContents(source_path, asm_code));

  ExceptionOr<std::nullptr_t> rc = jniproxy::jasmin::Main()->assemble(
      temp_path.path(),
      source_path,
      false);  // Don't emit line information.
  if (rc.HasException()) {
    rc.LogException();
    LOG(FATAL) << "Failed to assemble Java class:\n" << asm_code;
  }

  std::string blob;
  CHECK(GetFileContents(destination_path, &blob))
      << "Failed to assemble Java class:\n" << asm_code;

  LOG(INFO) << "Class assembled:\n"
            << jniproxy::ClassFileTextifier()->textify(blob, false)
                   .Release(ExceptionAction::LOG_AND_IGNORE);

  return blob;
}

// Assembles test Java class that only has a single method using Jasmin. We
// never set method arguments, since NanoJava interpreter ignores it anyway.
std::string AssembleMethod(const std::string& return_type,
                           const std::string& method_asm_code) {
  return Assemble(
      ".class public TestClass\n"
      ".super java/lang/Object\n"
      ".method public static test()" + return_type + "\n" +
      method_asm_code + "\n"
      ".end method\n");
}

}  // namespace cdbg
}  // namespace devtools
