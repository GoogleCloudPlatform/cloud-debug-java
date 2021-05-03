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

#include "jvmti_agent.h"

#include <atomic>
#include <cstdint>
#include <sstream>

#include "callbacks_monitor.h"
#include "auto_reset_event.h"
#include "bridge.h"
#include "jni_proxy_breakpointlabelsprovider.h"
#include "jni_proxy_classpathlookup.h"
#include "jni_proxy_dynamicloghelper.h"
#include "jni_proxy_gcpdebugletversion.h"
#include "jni_proxy_hubclient.h"
#include "jni_proxy_hubclient_listactivebreakpointsresult.h"
#include "config_builder.h"
#include "jni_breakpoint_labels_provider.h"
#include "jni_semaphore.h"
#include "jni_user_id_provider.h"
#include "jvm_class_metadata_reader.h"
#include "jvm_dynamic_logger.h"
#include "jvm_eval_call_stack.h"
#include "jvmti_agent_thread.h"
#include "jvmti_buffer.h"
#include "method_locals.h"
#include "stopwatch.h"
#include "jni_proxy_useridprovider.h"

ABSL_FLAG(string, cdbg_extra_class_path, "",
          "additional directories and files containing resolvable binaries");

using google::SetCommandLineOption;

namespace devtools {
namespace cdbg {

// Initialize flags from system properties. This way the user can set
// flags without messing up with JVMTI agent options.
static void InitializeFlagsFromSystemProperties() {
  jvmtiError err = JVMTI_ERROR_NONE;

  constexpr char kPrefix[] = "com.google.cdbg.agent.";

  jint count = 0;
  JvmtiBuffer<char*> properties;
  err = jvmti()->GetSystemProperties(&count, properties.ref());
  if (err != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "GetSystemProperties failed, err = " << err;
    return;
  }

  for (int i = 0; i < count; ++i) {
    const char* property = properties.get()[i];
    if (property == nullptr) {
      continue;
    }

    if (strncmp(property, kPrefix, arraysize(kPrefix) - 1) != 0) {
      continue;
    }

    const char* flag_name = property + (arraysize(kPrefix) - 1);

    JvmtiBuffer<char> value;
    err = jvmti()->GetSystemProperty(property, value.ref());
    if (err != JVMTI_ERROR_NONE) {
      LOG(ERROR) << "GetSystemProperty failed, property = " << property
                 << ", err = " << err;
      continue;
    }

    LOG(INFO) << "Setting flag from system property: FLAG_" << flag_name
              << " = " << value.get();
    SetCommandLineOption(flag_name, value.get());
  }
}


JvmtiAgent::JvmtiAgent(
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
    bool enable_jvmti_events)
    : internals_(internals),
      eval_call_stack_(std::move(eval_call_stack)),
      fn_loaders_(std::move(fn_loaders)),
      breakpoint_labels_provider_factory_(breakpoint_labels_provider_factory),
      user_id_provider_factory_(std::move(user_id_provider_factory)),
      data_visibility_policy_fn_(std::move(data_visibility_policy_fn)),
      enable_capabilities_(enable_capabilities),
      enable_jvmti_events_(enable_jvmti_events),
      scheduler_(Scheduler<>::DefaultClock),
      worker_(
          this,
          [this] () {
            return std::unique_ptr<AutoResetEvent>(new AutoResetEvent(
                std::unique_ptr<Semaphore>(new JniSemaphore)));
          },
          [this] () {
            return std::unique_ptr<AgentThread>(new JvmtiAgentThread);
          },
          internals_,
          std::move(bridge),
          &format_queue_) {
}


JvmtiAgent::~JvmtiAgent() {
  // Assert no unhealthy callbacks occurred throughout the debuglet lifetime.
  LOG_IF(WARNING, !CallbacksMonitor::GetInstance()->IsHealthy(0))
      << "Unhealthy callbacks occurred during debuglet lifetime";
}


// Loads numeric value from the specified system property. Returns the
// "default_value" if the system property was not found.
int32_t JvmtiAgent::GetSystemPropertyInt32(const char* name,
                                           int32_t default_value) {
  JvmtiBuffer<char> value;
  jvmtiError err = jvmti()->GetSystemProperty(name, value.ref());
  if (err != JVMTI_ERROR_NONE) {
    if (err != JVMTI_ERROR_NOT_AVAILABLE) {
      LOG(WARNING) << "GetSystemProperty failed, property = " << name
                   << ", err = " << err;
    }

    return default_value;
  }

  return atoi(value.get());  // NOLINT(runtime/deprecated_fn)
}

bool JvmtiAgent::OnLoad() {
  int err = 0;

  LOG(INFO) << "Java debuglet initialization started";

  // Initialize flags from system properties. This way the user can set
  // flags without messing up with JVMTI agent options.
  InitializeFlagsFromSystemProperties();

  // Generate debugger configuration.
  config_ = DefaultConfig();

  if (enable_capabilities_) {
    // Request capabilities. Without the right capabilities APIs like
    // SetBreakpoint will fail with error code 99.
    jvmtiCapabilities jvmti_capabilities;
    memset(&jvmti_capabilities, 0, sizeof(jvmti_capabilities));
    jvmti_capabilities.can_generate_breakpoint_events = true;
    jvmti_capabilities.can_maintain_original_method_order = true;
    jvmti_capabilities.can_get_line_numbers = true;
    jvmti_capabilities.can_access_local_variables = true;
    jvmti_capabilities.can_get_source_file_name = true;
    jvmti_capabilities.can_generate_compiled_method_load_events = true;
    err = jvmti()->AddCapabilities(&jvmti_capabilities);
    if (err != JVMTI_ERROR_NONE) {
      LOG(ERROR) << "AddCapabilities failed, error: " << err;
      // The best we can do here is to continue. We don't want to fail Java
      // process loading just because there was some problem with debugger.
    }
  }

  // Enable unconditional event callbacks (we need these whether debugger is
  // enabled or not).
  EnableJvmtiNotifications(
      JVMTI_ENABLE,
      { JVMTI_EVENT_VM_INIT, JVMTI_EVENT_VM_DEATH });

  LOG(INFO) << "Java debuglet initialization completed";

  return true;
}


void JvmtiAgent::JvmtiOnVMInit(jthread thread) {
  ScopedMonitoredCall monitored_call("JVMTI:VMInit");

  Stopwatch stopwatch;

  LOG(INFO) << "Java VM started";

  // Load system classes used globally throughout the debuglet code.
  if (!BindSystemClasses()) {
    LOG(ERROR) << "One of system classes not found";
    return;
  }

  worker_.Start();

  LOG(INFO) << "JvmtiAgent::JvmtiOnVMInit initialization time: "
            << stopwatch.GetElapsedMicros() << " microseconds";
}


void JvmtiAgent::JvmtiOnVMDeath() {
  Stopwatch stopwatch;

  LOG(INFO) << "Java VM termination";

  // Stop the worker threads.
  worker_.Shutdown();

  // Disable the debugger. This cleans up all breakpoints.
  EnableDebugger(false);

  // Release all pending breakpoint updates. They are never going to be sent
  // anyway...
  format_queue_.RemoveAll();

  CleanupSystemClasses();

  LOG(INFO) << "JvmtiAgent::JvmtiOnVMDeath cleanup time: "
            << stopwatch.GetElapsedMicros() << " microseconds";
}


// This callback is there just for completeness, but we are not subscribing
// to it. The class provided in JvmtiOnClassLoad doesn't have methods
// initialized so it not very useful for debugger.
void JvmtiAgent::JvmtiOnClassLoad(jthread thread, jclass cls) {
  // ScopedMonitoredCall monitored_call("JVMTI:ClassLoad");
}


void JvmtiAgent::JvmtiOnClassPrepare(jthread thread, jclass cls) {
  ScopedMonitoredCall monitored_call("JVMTI:ClassPrepare");

  std::shared_ptr<Debugger> debugger = debugger_;

  if (debugger != nullptr) {
    debugger->JvmtiOnClassPrepare(thread, cls);
  }
}


void JvmtiAgent::JvmtiOnCompiledMethodLoad(
    jmethodID method,
    jint code_size,
    const void* code_addr,
    jint map_length,
    const jvmtiAddrLocationMap* map,
    const void* compile_info) {
  // ScopedMonitoredCall monitored_call("JVMTI:CompiledMethodLoad");
}


void JvmtiAgent::JvmtiOnCompiledMethodUnload(
    jmethodID method,
    const void* code_addr) {
  ScopedMonitoredCall monitored_call("JVMTI:CompiledMethodUnload");

  std::shared_ptr<Debugger> debugger = debugger_;

  if (debugger != nullptr) {
    debugger->JvmtiOnCompiledMethodUnload(method, code_addr);
  }
}


void JvmtiAgent::JvmtiOnBreakpoint(
    jthread thread,
    jmethodID method,
    jlocation location) {
  ScopedMonitoredCall monitored_call("JVMTI:Breakpoint");

  // Ignore breakpoint events from debugger worker threads. Debugging
  // the debugger may cause deadlock.
  if (JvmtiAgentThread::IsInAgentThread()) {
    return;
  }

  std::shared_ptr<Debugger> debugger = debugger_;

  if (debugger != nullptr) {
    debugger->JvmtiOnBreakpoint(thread, method, location);
  }
}


void JvmtiAgent::EnableJvmtiNotifications(
    jvmtiEventMode mode,
    std::initializer_list<jvmtiEvent> event_types) {
  if (!enable_jvmti_events_) {
    return;
  }

  for (jvmtiEvent event_type : event_types) {
    jvmtiError err =
        jvmti()->SetEventNotificationMode(mode, event_type, nullptr);
    if (err != JVMTI_ERROR_NONE) {
      LOG(ERROR) << "SetEventNotificationMode failed, mode: " << mode
                 << ", event_type: " << event_type
                 << ", error: " << err;
      // The best we can do here is to continue.
    }
  }
}


void JvmtiAgent::EnableJvmtiDebuggerNotifications(jvmtiEventMode mode) {
  EnableJvmtiNotifications(
      mode,
      {
        JVMTI_EVENT_CLASS_PREPARE,
        JVMTI_EVENT_COMPILED_METHOD_UNLOAD,
        JVMTI_EVENT_BREAKPOINT
      });
}


bool JvmtiAgent::OnWorkerReady(DebuggeeLabels* debuggee_labels) {
  // Connect to Java internals implementation.
  {
    if (!internals_->LoadInternals()) {
      LOG(ERROR) << "Internals could not be initialized";
      return false;
    }
  }

  std::vector<bool (*)(jobject)> jni_bind_methods = {
    jniproxy::BindBreakpointLabelsProviderWithClassLoader,
    jniproxy::BindClassPathLookupWithClassLoader,
    jniproxy::BindDynamicLogHelperWithClassLoader,
    jniproxy::BindGcpDebugletVersionWithClassLoader,
    jniproxy::BindHubClientWithClassLoader,
    jniproxy::BindHubClient_ListActiveBreakpointsResultWithClassLoader,
    jniproxy::BindUserIdProviderWithClassLoader,
  };

  jni_bind_methods.insert(
      jni_bind_methods.end(),
      fn_loaders_.begin(),
      fn_loaders_.end());

  for (auto jni_bind_method : jni_bind_methods) {
    if (!jni_bind_method(internals_->class_loader_obj())) {
      return false;
    }
  }

  LOG(INFO) << "Initializing Cloud Debugger Java agent version: "
            << jniproxy::GcpDebugletVersion()->getVersion().GetData();

  // Split the extra class path into individual components.
  std::vector<std::string> extra_class_path;
  std::stringstream extra_class_path_stream(
      absl::GetFlag(FLAGS_cdbg_extra_class_path));
  std::string item;
  while (std::getline(extra_class_path_stream, item, ':')) {
      extra_class_path.push_back(item);
  }

  // Currently we need "ClassPathLookup" very early to compute uniquifier.
  if (!internals_->CreateClassPathLookupInstance(
        true,
        JniToJavaStringArray(extra_class_path).get())) {
    return false;
  }

  // Load data visibility configuration.
  data_visibility_policy_ =
      data_visibility_policy_fn_(internals_, debuggee_labels);

  return true;
}


void JvmtiAgent::OnIdle() {
  ScopedMonitoredCall monitored_call("Agent:Idle");

  // Invoke scheduled callbacks.
  // The precision of "scheduler_" has the granularity of "OnIdle" calls. This
  // is typically in the order of minutes. This precision is good enough
  // because the scheduler is only used to expire breakpoints and the
  // expiration time defaults to 24 hours. If scheduler is used for more fine
  // grained tasks, "Worker" will need to take the next scheduled time into
  // account when going into wait.
  scheduler_.Process();
}


void JvmtiAgent::OnBreakpointsUpdated(
    std::vector<std::unique_ptr<BreakpointModel>> breakpoints) {
  std::shared_ptr<Debugger> debugger = debugger_;
  if (debugger != nullptr) {
    debugger->SetActiveBreakpointsList(std::move(breakpoints));
  }
}


// Converts DataVisibilityPolicy error strings into a
// StatusMessageModel.  Returns nullptr if no there is no error.
static std::unique_ptr<StatusMessageModel> GetSetupErrorOrNull(
    const DataVisibilityPolicy& policy) {
  std::string error;
  return policy.HasSetupError(&error) ?
      StatusMessageBuilder()
          .set_error()
          .set_format(error)
          .set_refers_to(StatusMessageModel::Context::UNSPECIFIED)
          .build() :
      nullptr;
}


void JvmtiAgent::EnableDebugger(bool is_enabled) {
  ScopedMonitoredCall monitored_call(
      is_enabled ? "Agent:EnableDebugger" : "Agent:DisableDebugger");

  if (is_enabled) {
    // Attach debugger if needed
    if (debugger_ == nullptr) {
      LOG(INFO) << "Attaching Java debuglet";

      // Enable debugger specific event callbacks.
      EnableJvmtiDebuggerNotifications(JVMTI_ENABLE);

      std::unique_ptr<JvmDynamicLogger> dynamic_logger(new JvmDynamicLogger);
      dynamic_logger->Initialize();

      // Start the debugger. "debugger_" will start receiving JVMTI events
      // right away (not after "Initialize"). Debugger::Initialize initializes
      // JvmClassIndexer. JvmClassIndexer needs to know about all the classes.
      // It calls "jvmti->GetLoadedClasses" to retrieve the initial set of
      // prepared classes. Then it assumes that it will receive CLASS_PREPARED
      // notification for every class loaded after that point. Therefore the
      // notifications needs to be enabled before calling "Initialize".
      debugger_ = std::make_shared<Debugger>(
          &scheduler_,
          config_.get(),
          eval_call_stack_.get(),
          std::unique_ptr<MethodLocals>(
              new MethodLocals(data_visibility_policy_.get())),
          std::unique_ptr<ClassMetadataReader>(
              new JvmClassMetadataReader(data_visibility_policy_.get())),
          GetSetupErrorOrNull(*data_visibility_policy_),
          internals_,
          std::move(dynamic_logger),
          std::bind(&JvmtiAgent::BuildBreakpointLabelsProvider, this),
          std::bind(&JvmtiAgent::BuildUserIdProvider, this),
          &format_queue_,
          worker_.canary_control());
      debugger_->Initialize();
    }
  } else {
    // Detach debugger if needed
    if (debugger_ != nullptr) {
      // Disable debugger specific event callbacks.
      EnableJvmtiDebuggerNotifications(JVMTI_DISABLE);

      // The "Debugger" instance might not get released here if there is
      // ongoing JVMTI callback being processed.
      debugger_ = nullptr;

      // Remove all pending breakpoint updates. It is still possible that a
      // pending breakpoint will enqueue an update after "RemoveAll". We don't
      // care about it.
      format_queue_.RemoveAll();
    }
  }
}


std::unique_ptr<BreakpointLabelsProvider>
JvmtiAgent::BuildBreakpointLabelsProvider() {
  return std::unique_ptr<BreakpointLabelsProvider>(
      new JniBreakpointLabelsProvider(breakpoint_labels_provider_factory_));
}


std::unique_ptr<UserIdProvider> JvmtiAgent::BuildUserIdProvider() {
  return std::unique_ptr<UserIdProvider>(
      new JniUserIdProvider(user_id_provider_factory_));
}

}  // namespace cdbg
}  // namespace devtools
