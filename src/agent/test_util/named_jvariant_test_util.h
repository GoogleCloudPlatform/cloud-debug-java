/**
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_NAMED_JVARIANT_TEST_UTIL_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_NAMED_JVARIANT_TEST_UTIL_H_

#include <list>
#include <string>
#include <vector>

#include "src/agent/model_json.h"
#include "src/agent/type_util.h"
#include "src/agent/test_util/mock_jni_env.h"

namespace devtools {
namespace cdbg {

// Helper class for manipulating NamedJVariant objects in tests.
class NamedJvariantTestUtil {
 public:
  explicit NamedJvariantTestUtil(MockJNIEnv* jni);
  virtual ~NamedJvariantTestUtil();

  // Deep copies a vector of NamedJVariants.
  void CopyNamedJVariant(
      const std::vector<NamedJVariant>& source,
      std::vector<NamedJVariant>* destination);

  // Creates a NamedJVariant and adds it into a vector.
  void AddNamedJVariant(std::string name, JVariant* value,
                        WellKnownJClass well_known_jclass,
                        StatusMessageModel status,
                        std::vector<NamedJVariant>* variables);

  // Creates a numeric-based NamedJVariant and adds it into a vector.
  template <typename T>
  void AddNumericVariable(const std::string& name, T value,
                          std::vector<NamedJVariant>* variables) {
    JVariant value_variant = JVariant::Primitive<T>(value);
    AddNamedJVariant(
        name,
        &value_variant,
        WellKnownJClass::Unknown,
        {},
        variables);
  }

  // Creates a string-based NamedJVariant and adds it into a vector.
  void AddStringVariable(const std::string& name, const std::string& value,
                         std::vector<NamedJVariant>* variables);

  // Creates a ref-based NamedJVariant and adds it into a vector.
  void AddRefVariable(const std::string& name, jobject ref,
                      std::vector<NamedJVariant>* variables);

 private:
  // Mock JNI object used to manipulate the NamedJVariants.
  MockJNIEnv* jni_;

  // A buffer of all the jstrings created.
  std::list<std::vector<jchar>> jstring_buffers_;

  DISALLOW_COPY_AND_ASSIGN(NamedJvariantTestUtil);
};

}  // namespace cdbg
}  // namespace devtools
#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_NAMED_JVARIANT_TEST_UTIL_H_
