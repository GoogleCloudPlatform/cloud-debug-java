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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_VERSION_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_VERSION_H_

namespace devtools {
namespace cdbg {

#define DEBUGLET_VERSION_CODE      "1.25"

// Version ID of the debuglet. The version ID is structured as following:
//   "domain/type/vmajor.minor"
// Since we want to define different version strings for each hub client,
// the actual definition of this string is in an appropriate .cc file.
extern const char* kDebugletVersion;

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_VERSION_H_
