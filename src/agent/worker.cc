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

#include "worker.h"

#include <cstdint>

#include "callbacks_monitor.h"
#include "agent_thread.h"
#include "bridge.h"

ABSL_FLAG(int32, hub_retry_delay_ms,
          10000,  // 10 seconds
          "amount of time in milliseconds to sleep before retrying failed "
          "requests to Cloud Debugger backend");

ABSL_FLAG(int32, debuggee_disabled_delay_ms,
          600000,  // 10 minutes
          "amount of time in milliseconds to sleep before checking whether "
          "the debugger was enabled back");

namespace devtools {
namespace cdbg {

int g_is_enabled_attempts = 0;

Worker::Worker(
    Provider* provider,
    std::function<std::unique_ptr<AutoResetEvent>()> event_factory,
    std::function<std::unique_ptr<AgentThread>()> agent_thread_factory,
    ClassPathLookup* class_path_lookup,
    std::unique_ptr<Bridge> bridge,
    FormatQueue* format_queue)
    : provider_(provider),
      main_thread_event_(event_factory()),
      transmission_thread_event_(event_factory()),
      main_thread_(agent_thread_factory()),
      transmission_thread_(agent_thread_factory()),
      class_path_lookup_(class_path_lookup),
      bridge_(std::move(bridge)),
      canary_control_(CallbacksMonitor::GetInstance(), bridge_.get()),
      format_queue_(format_queue) {
  // Subscribe to receive synchronous notifications every time a breakpoint
  // update is enqueued. In return we get a cookie that must be returned
  // back to unsubscribe (on shutdown).
  on_breakpoint_update_enqueued_cookie_ =
      format_queue_->SubscribeOnItemEnqueuedEvents([this]() {
        transmission_thread_event_->Signal();
      });
}


Worker::~Worker() {
}


void Worker::Start() {
  // Initialize the thread event. The debugger thread would be a better place
  // to have this initialization to reduce the impact on application startup
  // time. The problem is that if "Shutdown" is called before the event has
  // been created, it will not be able to signal the debugger thread to stop.
  if (!main_thread_event_->Initialize()) {
    LOG(ERROR) << "Debugger thread event could not be initialized. "
                  "Debugger will not be available.";
    return;
  }

  if (!main_thread_->Start(
        "CloudDebugger_main_worker_thread",
        std::bind(&Worker::MainThreadProc, this))) {
    LOG(ERROR) << "Java debugger worker thread could not be started. "
                  "Debugger will not be available.";
    return;
  }
}


void Worker::Shutdown() {
  format_queue_->UnsubscribeOnItemEnqueuedEvents(
      std::move(on_breakpoint_update_enqueued_cookie_));

  is_unloading_ = true;

  // Cancel all pending requests to the backend.
  bridge_->Shutdown();

  // Signal for the debugger thread to exit. Then wait until the thread
  // terminates.
  main_thread_event_->Signal();
  if (main_thread_->IsStarted()) {
    main_thread_->Join();
  }

  // Now wait until subscriber thread terminates. We only stop the subscriber
  // thread after the main worker thread exits to make sure that the subscriber
  // thread does not get created again while we are waiting for the main worker
  // thread to terminate.
  provider_->EnableDebugger(false);

  LOG(INFO) << "Debugger threads terminated";
}


void Worker::MainThreadProc() {
  //
  // One time initialization of Worker. This initialization logically belongs
  // to "Start", but it was moved here to reduce the impact of debugger on
  // application startup time.

  // Deferred initialization of the agent.
  if (!provider_->OnWorkerReady(&debuggee_labels_)) {
    LOG(ERROR) << "Agent initialization failed: "
                  "debugger thread can't continue.";
    return;  // Signal to stop the main debugger thread.
  }

  if (!transmission_thread_event_->Initialize()) {
    LOG(ERROR) << "Transmission event could not be initialized: "
                  "debugger thread can't continue.";
    return;
  }

  // Initialize Hub client.
  if (!bridge_->Bind(class_path_lookup_)) {
    LOG(ERROR) << "HubClient not available: debugger thread can't continue.";
    return;  // Signal to stop the main debugger thread.
  }

  bool is_enabled = false;
  while (!is_unloading_ && !bridge_->IsEnabled(&is_enabled)) {
    // Wait for classes to load.
    ++g_is_enabled_attempts;
    provider_->OnIdle();
    main_thread_event_->Wait(100);  // Wait for 100ms to lower CPU usage
  }

  if (g_is_enabled_attempts > 0) {
    LOG(INFO) << "Debugger had " << g_is_enabled_attempts
              << " unsuccessful IsEnabled attempts.";
  }

  // This log message only makes sense if the jvm is not being shutdown, ie
  // is_unloading_ is false, it is misleading otherwise. This situation can
  // occur in very short lived tasks where the debugger did not have enough time
  // to initialize before the JVM gets shutdown.
  if (!is_unloading_ && !is_enabled) {
    LOG(WARNING) << "The debugger is disabled on this process.";
    return;
  }

  while (!is_unloading_) {
    // Register debuggee if not registered or if previous call to list active
    // breakpoints failed.
    if (!is_registered_) {
      RegisterDebuggee();
    } else {
      // Issue a hanging get request to list active breakpoints.
      ListActiveBreakpoints();
    }

    canary_control_.ApproveHealtyBreakpoints();

    provider_->OnIdle();
  }

  // This thread owns the transmission thread. Stop it now.
  if (transmission_thread_->IsStarted()) {
    transmission_thread_event_->Signal();
    transmission_thread_->Join();
  }
}


void Worker::TransmissionThreadProc() {
  while (!is_unloading_) {
    // Wait until one of the following:
    // 1. New breakpoint update has been enqueued.
    // 2. Shutdown.
    // 3. Previously failed transmissions and we are past the retry interval.
    transmission_thread_event_->Wait(
        bridge_->HasPendingMessages() ? absl::GetFlag(FLAGS_hub_retry_delay_ms)
                                      : 100000000);  // arbitrary long delay.

    // Enqueue new breakpoint updates for transmission.
    while (!is_unloading_) {
      std::unique_ptr<BreakpointModel> breakpoint =
          format_queue_->FormatAndPop();

      if (breakpoint == nullptr) {
        break;
      }

      bridge_->EnqueueBreakpointUpdate(std::move(breakpoint));
    }

    // Post breakpoint hit results (both the new ones and retry previously
    // failed messages).
    bridge_->TransmitBreakpointUpdates();
  }
}


void Worker::RegisterDebuggee() {
  bool new_is_enabled = false;
  is_registered_ = bridge_->RegisterDebuggee(&new_is_enabled, debuggee_labels_);

  if (is_registered_) {
    provider_->EnableDebugger(new_is_enabled);

    if (!new_is_enabled) {
      is_registered_ = false;
      main_thread_event_->Wait(absl::GetFlag(FLAGS_debuggee_disabled_delay_ms));
    }
  } else {
    // Delay before attempting to retry.
    main_thread_event_->Wait(absl::GetFlag(FLAGS_hub_retry_delay_ms));
  }
}


void Worker::ListActiveBreakpoints() {
  // Query list of active breakpoints.
  std::vector<std::unique_ptr<BreakpointModel>> breakpoints;

  auto rc = bridge_->ListActiveBreakpoints(&breakpoints);
  switch (rc) {
    case Bridge::HangingGetResult::SUCCESS:
      // Start the transmission thread first time a breakpoint is set. We then
      // never stop the transmission thread until shutdown (for simplicity).
      if (!breakpoints.empty() && !transmission_thread_->IsStarted()) {
        if (!transmission_thread_->Start(
              "CloudDebugger_transmission_thread",
              std::bind(&Worker::TransmissionThreadProc, this))) {
          LOG(ERROR) << "Transmission thread could not be started.";
        }
      }

      // Update the list of active breakpoints.
      provider_->OnBreakpointsUpdated(std::move(breakpoints));
      return;

    case Bridge::HangingGetResult::FAIL:
      is_registered_ = false;
      return;

    case Bridge::HangingGetResult::TIMEOUT:
      return;
  }
}

}  // namespace cdbg
}  // namespace devtools

