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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_BREAKPOINTS_MANAGER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_BREAKPOINTS_MANAGER_H_

#include <map>
#include <memory>

#include "leaky_bucket.h"
#include "common.h"

namespace devtools {
namespace cdbg {

struct BreakpointModel;
class Breakpoint;

// Manages list of active breakpoints and processes breakpoint hit events.
// This class is thread safe.
class BreakpointsManager {
 public:
  virtual ~BreakpointsManager() {}

  // Releases all the resources before the class destruction. This function may
  // only be called when there are no outstanding callbacks.
  virtual void Cleanup() = 0;

  // Sets the list of active breakpoints. This list is maintained by the hub
  // service. The caller passes list of all the active breakpoints.
  // "BreakpointsManager" adds the new ones and removes the missing ones.
  // Breakpoints that are already set are not altered.
  virtual void SetActiveBreakpointsList(
      std::vector<std::unique_ptr<BreakpointModel>> breakpoints) = 0;

  // Indicates that the specified Java method is no longer valid. The purpose
  // of this callback is to remove all references to the unloaded method. This
  // is needed because the value of jmethodID is no longer valid after
  // "JvmtiOnCompiledMethodUnload" and (theoretically) in the future a different
  // method may get the same jmethodID.
  virtual void JvmtiOnCompiledMethodUnload(jmethodID method) = 0;

  // Callback upon breakpoint event. "BreakpointsManager" maintains a map of
  // (method, location) tuple to breakpoint definition.
  virtual void JvmtiOnBreakpoint(
      jthread thread,
      jmethodID method,
      jlocation location) = 0;

  // Sets individual JVMTI breakpoint and enables routing of breakpoint hits
  // to the appropriate breakpoint objects. The main reason to have this
  // functionality is that we can have two (or more) breakpoints at the same
  // location. In this case the multiplexer needs to maintain a reference
  // counter and only clear the actual JVMTI breakpoint when the last
  // breakpoint at that location goes away.
  //
  // It is the responsibility of "Breakpoint" to make sure each call to
  // "SetJvmtiBreakpoint" is eventually followed by a call to
  // "ClearJvmtiBreakpoint".
  //
  // Returns false if the JVMTI breakpoint could not be set.
  virtual bool SetJvmtiBreakpoint(
      jmethodID method,
      jlocation location,
      std::shared_ptr<Breakpoint> breakpoint) = 0;

  // Clears breakpoint set by "SetJvmtiBreakpoint". If the breakpoint is not
  // set, this function has no effect.
  virtual void ClearJvmtiBreakpoint(
      jmethodID method,
      jlocation location,
      std::shared_ptr<Breakpoint> breakpoint) = 0;

  // Removes the breakpoint from list of active breakpoints and clears the
  // breakpoint. It is possible that some other thread is currently handling
  // breakpoint hit for this breakpoint.
  virtual void CompleteBreakpoint(std::string breakpoint_id) = 0;

  // Gets the counter for total cost incurred by evaluating conditions across
  // all enabled breakpoints. The purpose of this counter is to prevent many
  // breakpoints from consuming too much CPU together (while each breakpoint
  // is within limits).
  virtual LeakyBucket* GetGlobalConditionCostLimiter() = 0;

  // Gets the counter for total dynamic log entries spawn by all logging
  // breakpoints. The purpose of this counter is to prevent many breakpoints
  // from logging too much (while each logging breakpoint logs within limits).
  virtual LeakyBucket* GetGlobalDynamicLogLimiter() = 0;

  // Gets the counter for total dynamic log bytes spawn by all logging
  // breakpoints. The purpose of this counter is to prevent many breakpoints
  // from logging too much (while each logging breakpoint logs within limits).
  virtual LeakyBucket* GetGlobalDynamicLogBytesLimiter() = 0;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_BREAKPOINTS_MANAGER_H_
