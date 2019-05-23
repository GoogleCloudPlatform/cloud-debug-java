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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_AUTO_JVMTI_BREAKPOINT_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_AUTO_JVMTI_BREAKPOINT_H_

#include <memory>

#include "common.h"
#include "mutex.h"

namespace devtools {
namespace cdbg {

class BreakpointsManager;
class Breakpoint;

// Utility class to ensure proper coupling of "SetJvmtiBreakpoint" and
// "ClearJvmtiBreakpoint".
class AutoJvmtiBreakpoint {
 public:
  explicit AutoJvmtiBreakpoint(
      BreakpointsManager* breakpoints_manager)
      : breakpoints_manager_(breakpoints_manager) {
  }

  ~AutoJvmtiBreakpoint() {
    DCHECK(method_ == nullptr);
  }

  bool Set(
      jmethodID method,
      jlocation location,
      std::shared_ptr<Breakpoint> breakpoint);

  void Clear(std::shared_ptr<Breakpoint> breakpoint);

 private:
  void ClearUnsafe(std::shared_ptr<Breakpoint> breakpoint);

 private:
  BreakpointsManager* const breakpoints_manager_;
  absl::Mutex mu_;
  jmethodID method_ { nullptr };
  jlocation location_ { 0 };

  DISALLOW_COPY_AND_ASSIGN(AutoJvmtiBreakpoint);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_AUTO_JVMTI_BREAKPOINT_H_
