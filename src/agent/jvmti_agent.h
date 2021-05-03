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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVMTI_AGENT_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVMTI_AGENT_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "common.h"
#include "config.h"
#include "data_visibility_policy.h"
#include "debuggee_labels.h"
#include "debugger.h"
#include "eval_call_stack.h"
#include "jvm_internals.h"
#include "scheduler.h"
#include "user_id_provider.h"
#include "worker.h"

namespace devtools {
namespace cdbg {

class Bridge;

// JVMTI based debugger agent. Most of the actual work is done by Debugger
// class. This separation is built to support unloading as much of the debugger
// as possible when the debuggee is disabled. The agent also maintains worker
// threads that take care of background processing.
// For more details about JVMTI see:
// http://docs.oracle.com/javase/6/docs/platform/jvmti/jvmti.html
class JvmtiAgent : public Worker::Provider {
 public:
  // "internals" and "logger" are not owned by this class, but this class is
  // responsible for the lifetime of these objects (including initialization
  // and cleanup on VM death).
  // "load_service_account_auth" indicates whether "internals" initialization
  // should also load "cdbg_service_account_auth.jar" and create the
  // appropriate object.
  JvmtiAgent(
      JvmInternals* internals,
      std::unique_ptr<EvalCallStack> eval_call_stack,
      std::vector<bool (*)(jobject)> fn_loaders,
      std::unique_ptr<Bridge> bridge,
      std::function<JniLocalRef()> breakpoint_labels_provider_factory,
      std::function<JniLocalRef()> user_id_provider_factory,
      std::function<std::unique_ptr<DataVisibilityPolicy>(ClassPathLookup*,
                                                          DebuggeeLabels*)>
          data_visibility_policy_fn,
      bool enable_capabilities,
      bool enable_jvmti_events);

  ~JvmtiAgent() override;

  // Loads numeric value from the specified system property. Returns the
  // "default_value" if the system property was not found.
  static int32_t GetSystemPropertyInt32(const char* name,
                                        int32_t default_value);

  // Very first callback from JVM when the shared library is loaded.
  bool OnLoad();

  // The VM initialization event signals the completion of VM initialization.
  void JvmtiOnVMInit(jthread thread);

  // Notification about termination of the VM.
  void JvmtiOnVMDeath();

  // A class load event is generated when a class is first loaded before
  // ClassPrepare event. This callback is there just for completeness, but
  // we are not subscribing to it. The class provided in JvmtiOnClassLoad
  // doesn't have methods initialized so it not very useful for debugger.
  void JvmtiOnClassLoad(jthread thread, jclass cls);

  // A class prepare event is generated when Java class is ready to be used
  // by Java code, but before any method (including constructor and static
  // initializer) is actually called.
  void JvmtiOnClassPrepare(jthread thread, jclass cls);

  // Sent when a method is compiled and loaded into memory by the VM. If it is
  // unloaded, the CompiledMethodUnload event is sent. If it is moved, the
  // CompiledMethodUnload event is sent, followed by a new CompiledMethodLoad
  // event. Note that a single method may have multiple compiled forms, and that
  // this event will be sent for each form. Note also that several methods may
  // be inlined into a single address range, and that this event will be sent
  // for each method.
  void JvmtiOnCompiledMethodLoad(
      jmethodID method,
      jint code_size,
      const void* code_addr,
      jint map_length,
      const jvmtiAddrLocationMap* map,
      const void* compile_info);

  // Sent when a compiled method is unloaded from memory. This event
  // invalidates breakpoint set in this method. The method ID is no
  // longer valid after this call.
  void JvmtiOnCompiledMethodUnload(jmethodID method, const void* code_addr);

  // Callback for breakpoint event.
  void JvmtiOnBreakpoint(
      jthread thread,
      jmethodID method,
      jlocation location);

  //
  // Implementation of Worker::Provider interface
  //

  bool OnWorkerReady(DebuggeeLabels* debuggee_labels) override;

  void OnIdle() override;

  void OnBreakpointsUpdated(
      std::vector<std::unique_ptr<BreakpointModel>> breakpoints) override;

  void EnableDebugger(bool is_enabled) override;

 private:
  // Enables or disables certain JVMTI callbacks.
  void EnableJvmtiNotifications(
      jvmtiEventMode mode,
      std::initializer_list<jvmtiEvent> event_types);

  // Enables or disables debugger specific JVMTI callbacks.
  void EnableJvmtiDebuggerNotifications(jvmtiEventMode mode);

  // Creates the instance of "BreakpointLabelsProvider" to use for the debugger.
  std::unique_ptr<BreakpointLabelsProvider> BuildBreakpointLabelsProvider();

  // Creates the instance of "UserIdProvider" to use in the Debugger.
  std::unique_ptr<UserIdProvider> BuildUserIdProvider();

 private:
  // Proxy class to access Java internals implementation.
  // Not owned by this class.
  JvmInternals* const internals_;

  // Call stack implementation.  This class takes ownership
  // of eval_call_stack_
  std::unique_ptr<EvalCallStack> eval_call_stack_;

  // Agent configuration.
  std::unique_ptr<Config> config_;

  // Vector of function pointers that load Java based classes.
  std::vector<bool (*)(jobject)> fn_loaders_;

  // Factory for a class implementing the Java
  // com.google.devtools.cdbg.debuglets.java.BreakpointLabelsProvider interface.
  const std::function<JniLocalRef()> breakpoint_labels_provider_factory_;

  // Factory for a class implementing the Java
  // com.google.devtools.cdbg.debuglets.java.UserIdProvider interface.
  const std::function<JniLocalRef()> user_id_provider_factory_;

  // Reads data visibility configuration from .JAR files.
  const std::function<std::unique_ptr<DataVisibilityPolicy>(ClassPathLookup*,
                                                            DebuggeeLabels*)>
      data_visibility_policy_fn_;

  // When false, don't enable JVMTI capabilities.
  const bool enable_capabilities_;

  // When false, don't enable JVMTI events as debugger gets enabled/disabled.
  const bool enable_jvmti_events_;

  // Schedules callbacks at a specified time in the future.
  Scheduler<> scheduler_;

  // Breakpoint hit results that wait to be reported to the hub.
  FormatQueue format_queue_;

  // Worker threads responsible to talk to the backend.
  Worker worker_;

  // Manages data visibility policy based on configuration in .JAR files.
  std::unique_ptr<DataVisibilityPolicy> data_visibility_policy_;

  // Currently attached debugger instance. The use of "shared_ptr" here is
  // making sure that the "Debugger" instance doesn't go away in the middle
  // of the callback processing.
  std::shared_ptr<Debugger> debugger_;

  DISALLOW_COPY_AND_ASSIGN(JvmtiAgent);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVMTI_AGENT_H_
