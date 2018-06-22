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

#include "auto_jvmti_breakpoint.h"

#include "breakpoints_manager.h"

namespace devtools {
namespace cdbg {

bool AutoJvmtiBreakpoint::Set(
    jmethodID method,
    jlocation location,
    std::shared_ptr<Breakpoint> breakpoint) {
  absl::MutexLock lock(&mu_);

  if ((method_ == method) && (location_ == location)) {
    return true;
  }

  ClearUnsafe(breakpoint);

  if (!breakpoints_manager_->SetJvmtiBreakpoint(
          method,
          location,
          breakpoint)) {
    return false;
  }

  method_ = method;
  location_ = location;

  return true;
}


void AutoJvmtiBreakpoint::Clear(std::shared_ptr<Breakpoint> breakpoint) {
  absl::MutexLock lock(&mu_);
  ClearUnsafe(breakpoint);
}


void AutoJvmtiBreakpoint::ClearUnsafe(
    std::shared_ptr<Breakpoint> breakpoint) {
  if (method_ != nullptr) {
    breakpoints_manager_->ClearJvmtiBreakpoint(
        method_,
        location_,
        breakpoint);

    method_ = nullptr;
    location_ = 0;
  }
}

}  // namespace cdbg
}  // namespace devtools


