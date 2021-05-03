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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_BRIDGE_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_BRIDGE_H_

#include <memory>

#include "common.h"
#include "debuggee_labels.h"
#include "model.h"

namespace devtools {
namespace cdbg {

class ClassPathLookup;

// Interface for communication with the Hub service through Java code.
// Only synchronous operations are supported.
// This interface is thread safe.
class Bridge {
 public:
  enum class HangingGetResult {
    SUCCESS,
    FAIL,
    TIMEOUT
  };

  virtual ~Bridge() { }

  // HubClient initialization. "jni" is not stored beyond the "Bind" call,
  // but "class_path_lookup" is used throughout the lifetime of this class.
  // The caller is responsible for "class_path_lookup" lifetime. "Bind" will
  // always fail if called after "Shutdown".
  virtual bool Bind(ClassPathLookup* class_path_lookup) = 0;

  // Attempts to shutdown all pending requests to the Cloud Debugger backend.
  virtual void Shutdown() = 0;

  // Registers the debuggee with the controller. Returns true if the request
  // was successful. When registration succeeds "is_enabled" will usually be
  // set to true (unless the Hub remotely disables the debuglet).
  virtual bool RegisterDebuggee(bool* is_enabled,
                                const DebuggeeLabels& debugee_labels) = 0;

  // Queries for the list of currently active breakpoints. Returns false if the
  // network call failed.
  virtual HangingGetResult ListActiveBreakpoints(
      std::vector<std::unique_ptr<BreakpointModel>>* breakpoints) = 0;

  // Enqueues the next breakpoint update for transmission to the Hub service.
  virtual void EnqueueBreakpointUpdate(
      std::unique_ptr<BreakpointModel> breakpoint) = 0;

  // Attempts transmission of pending breakpoints. "HasPendingMessages"
  // can be used to check whether all pending messages have been sent
  // successfully.
  virtual void TransmitBreakpointUpdates() = 0;

  // Checks whether there are still pending messages to be transmitted to the
  // Hub service.
  virtual bool HasPendingMessages() const = 0;

  // Notifies the backend that a canary agent enabled the breakpoint.
  virtual bool RegisterBreakpointCanary(const std::string& breakpoint_id) = 0;

  // Approves the breakpoint for a global rollout.
  virtual bool ApproveBreakpointCanary(const std::string& breakpoint_id) = 0;

  // Tries to determine if the debugger is enabled.  If the debugger is enabled,
  // sets is_enabled to true and returns true.  If the debugger is disabled,
  // sets is_enabled to false and returns true.  If the method can not determine
  // status, sets is_enabled to false and returns false.
  //
  // Note: This method might be better put into it's own interface.
  // The reason it's not done now is that creating a new interface involves
  // a lot of change and boilerplate.  It would seems reasonable to see the need
  // for an interface expanded a bit before putting the definitions and
  // rules in place.
  virtual bool IsEnabled(bool* is_enabled) = 0;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_BRIDGE_H_
