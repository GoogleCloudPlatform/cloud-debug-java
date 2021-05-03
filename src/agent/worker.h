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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_WORKER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_WORKER_H_

#include <atomic>
#include <map>
#include <memory>

#include "auto_reset_event.h"
#include "canary_control.h"
#include "common.h"
#include "debuggee_labels.h"
#include "format_queue.h"
#include "model.h"
#include "stopwatch.h"

namespace devtools {
namespace cdbg {

class AgentThread;
class Bridge;
class ClassPathLookup;

// Implements background worker threads of the debuglet. The main worker thread
// communicate with the backend and call the agent back when list of active
// breakpoints changes. A second worker thread is used to send breakpoint
// updates to the backend.
class Worker {
 public:
  // Callback interface to used by the worker owner
  class Provider {
   public:
    virtual ~Provider() {}

    // One time initialization invoked from a worker thread. Unlike JVMTI
    // callbacks, actions done from this function don't impact the start up
    // time of the application. If this function returns false, "Worker" will
    // stop and the debugger will not be functioning.
    virtual bool OnWorkerReady(DebuggeeLabels* debuggee_labels) = 0;

    // Called periodically by the worker thread to give opportunity to the
    // agent to perform routine tasks. Examples: flushing logs, garbage
    // collecting objects that we don't need.
    // All "OnIdle" calls are invoked from the same thread.
    virtual void OnIdle() = 0;

    // Called upon a change in a set of active breakpoints.
    // TODO: add support for added/removed actions for a single
    // breakpoint.
    virtual void OnBreakpointsUpdated(
        std::vector<std::unique_ptr<BreakpointModel>> breakpoints) = 0;

    // Attaches or detaches the debugger as necessary.
    virtual void EnableDebugger(bool is_enabled) = 0;
  };

  // The "provider", "class_path_lookup" and "format_queue" not owned by this
  // class and must outlive this object.
  Worker(
      Provider* provider,
      std::function<std::unique_ptr<AutoResetEvent>()> event_factory,
      std::function<std::unique_ptr<AgentThread>()> agent_thread_factory,
      ClassPathLookup* class_path_lookup,
      std::unique_ptr<Bridge> bridge,
      FormatQueue* format_queue);

  ~Worker();

  // Starts the worker.
  void Start();

  // Stops the worker threads. This function should only be called when an
  // agent gets unloaded.
  void Shutdown();

  // Gets the canary breakpoints manager.
  CanaryControl* canary_control() { return &canary_control_; }

 private:
  // Main debugger worker thread (registration and list active breakpoints).
  void MainThreadProc();

  // Transmission worker thread (sends breakpoint updates to the backend).
  void TransmissionThreadProc();

  // Attaches/detaches debugger.
  void EnableDebugger(bool new_is_enabled);

  // Sends "RegisterDebuggee" message to the backend and updates the local
  // state.
  void RegisterDebuggee();

  // Updates list of active breakpoints in the actual debugger based on the
  // cues we received from pub/sub thread.
  void ListActiveBreakpoints();

 private:
  // Callback interface. The provider feeds the worker thread with the
  // breakpoint updates to be sent to the backend and listens for notifications
  // about new breakpoints and debuggee getting disabled.
  // Not owned by this class.
  Provider* const provider_;

  // Notification event to wake up list breakpoints thread.
  std::unique_ptr<AutoResetEvent> main_thread_event_;

  // Notification event to wake up transmission thread.
  std::unique_ptr<AutoResetEvent> transmission_thread_event_;

  // Main debugger worker thread (registration and list active breakpoints).
  std::unique_ptr<AgentThread> main_thread_;

  // Worker thread to send breakpoint updates to the backend.
  std::unique_ptr<AgentThread> transmission_thread_;

  // Java implementation of "ClassPathLookup" class. Not owned by this class.
  ClassPathLookup* const class_path_lookup_;

  // Implementation of a protocol client with the Hub service or a test.
  std::unique_ptr<Bridge> bridge_;

  // Manages canary breakpoints.
  CanaryControl canary_control_;

  // Breakpoint hit results that wait to be reported to the hub.
  FormatQueue* const format_queue_;

  // Registration of a callback when a new breakpoint is enqueued.
  FormatQueue::OnItemEnqueued::Cookie on_breakpoint_update_enqueued_cookie_;

  // Flag indicating that the JVMTI agent is being unloaded.
  std::atomic<bool> is_unloading_ { false };

  // Result of last call to "RegisterDebuggee".
  bool is_registered_ { false };

  // Debuggee labels gathered from the native code to be included in the set of
  // labels for the Debuggee in the RegisterDebuggee call.
  //
  // NOTE: Once the label are gathered before the first call to
  // RegisterDebuggee, we must be sure not to update it again, the same set of
  // labels should be used in every subsequent call since the labels are used in
  // the debuggee ID generation, so we don't want to cause duplicate IDs for the
  // same agent.
  DebuggeeLabels debuggee_labels_;

  DISALLOW_COPY_AND_ASSIGN(Worker);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_WORKER_H_
