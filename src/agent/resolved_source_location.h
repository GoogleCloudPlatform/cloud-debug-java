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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_RESOLVED_SOURCE_LOCATION_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_RESOLVED_SOURCE_LOCATION_H_

#include <string>

#include "model.h"

namespace devtools {
namespace cdbg {

// Represents a resolved source code location. It unambiguously points
// to a code statement. The represented location may still not be loaded,
// hence we keep around line number rather than "jlocation" that only becomes
// available after Java class has been loaded and prepared.
struct ResolvedSourceLocation {
  // If "error_message.format" is empty, the resolution was successful.
  // Otherwise it contains an error message that needs to be sent to the end
  // user.
  FormatMessageModel error_message;

  // Class signature (like "com/prod/MyClass$MyInnerClass").
  std::string class_signature;

  // Short method name (does not include the class name).
  std::string method_name;

  // Method signature (like "(Lcom/prod/MyClass$MyInnerClass;)Z").
  std::string method_signature;

  // Line number pointing to the beginning of the statement.
  int adjusted_line_number { -1 };
};

}  // namespace cdbg
}  // namespace devtools


#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_RESOLVED_SOURCE_LOCATION_H_
