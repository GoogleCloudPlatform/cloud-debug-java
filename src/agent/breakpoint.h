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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_BREAKPOINT_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_BREAKPOINT_H_

#include "common.h"
#include "model.h"

namespace devtools {
namespace cdbg {

// Single active breakpoint in Java code. A breakpoint can be in one of these
// states:
//   1. Uninitialized: this is the state the breakpoint gets right after the
//      object is constructed.
//   2. Pending: we know that the breakpoint is valid, but we still don't know
//      the Java jmethodID and jlocation and the JVMTI breakpoint is not set.
//      A breakpoint will remain pending until the class containing the source
//      location is loaded by JVM (JVM loads classes when they are referenced).
//   3. Active: the {jmethodID, jlocation} tuple has been resolved and JVMTI
//      breakpoint is set.
//
// JvmBreakpoint might get deactivated or completed on one thread while another
// thread is processing a breakpoint hit. The implementation is using
// shared_ptr to snapshot the state. Locks must not be used in callbacks that
// may invoke Java method (through JNI). This is because Java methods may
// inadvertently trigger other synchronous callbacks causing application
// deadlock.
//
// "Breakpoint" will start receiving "OnClassPrepared" event right after the
// object is constructed (even in uninitialized state).
//
// Each active breakpoint is associated with a single code location. Unlike in
// C++ generics in Java don't duplicate compiled code.
class Breakpoint {
 public:
  virtual ~Breakpoint() {}

  // Getter for breakpoint ID.
  virtual const std::string& id() const = 0;

  // Initializes the breakpoint to either active or pending state. If the
  // breakpoint is invalid, sends a final breakpoint update and completes
  // the breakpoint.
  virtual void Initialize() = 0;

  // Invalidates the breakpoint state back to pending. Clears JVMTI breakpoint
  // as necessary.
  virtual void ResetToPending() = 0;

  // Callback invoked when JVM initialized (aka prepared) a Java class. The
  // class might be unrelated to this breakpoint.
  virtual void OnClassPrepared(const std::string& type_name,
                               const std::string& class_signature) = 0;

  // Takes action on a hit over a single breakpoint.
  virtual void OnJvmBreakpointHit(
      jthread thread,
      jmethodID method,
      jlocation location) = 0;

  // Finalizes the breakpoint with the specified status message and removes
  // it from the list of active breakpoints.
  virtual void CompleteBreakpointWithStatus(
      std::unique_ptr<StatusMessageModel> status) = 0;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_BREAKPOINT_H_
