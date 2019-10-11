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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_BREAKPOINT_LABELS_PROVIDER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_BREAKPOINT_LABELS_PROVIDER_H_

#include <map>

#include "common.h"

namespace devtools {
namespace cdbg {

// Collects information about the local environment at the breakpoint
// capture time.
class BreakpointLabelsProvider {
 public:
  virtual ~BreakpointLabelsProvider() {}

  // Captures the environment information into set of breakpoint labels. This
  // function is called from the application thread, so it should complete as
  // quickly as possible.
  virtual void Collect() = 0;

  // Formats the breakpoint labels into breakpoint labels map. This function is
  // called from the debugger worker thread.
  virtual std::map<std::string, std::string> Format() = 0;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_BREAKPOINT_LABELS_PROVIDER_H_
