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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_METHOD_CALLER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_METHOD_CALLER_H_

#include "method_caller.h"
#include "gmock/gmock.h"

namespace devtools {
namespace cdbg {

class MockMethodCaller : public MethodCaller {
 public:
  // Transform this function to a version that's easier to mock.
  ErrorOr<JVariant> Invoke(
      const ClassMetadataReader::Method& metadata,
      const JVariant& source,
      std::vector<JVariant> arguments) override {
    std::string s;
    s += "class = ";
    s += metadata.class_signature.object_signature;
    s += ", method name = ";
    s += metadata.name;
    s += ", method signature = ";
    s += metadata.signature;
    s += ", source = ";
    s += source.ToString(false);
    s += ", arguments = (";
    for (int i = 0; i < arguments.size(); ++i) {
      if (i > 0) {
        s += ", ";
      }
      s += arguments[i].ToString(false);
    }
    s += ')';

    ErrorOr<const JVariant*> rc = Invoke(s);
    if (rc.is_error()) {
      return rc.error_message();
    }

    return JVariant(*rc.value());
  }

  MOCK_METHOD(ErrorOr<const JVariant*>, Invoke, (const std::string&));
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_METHOD_CALLER_H_
