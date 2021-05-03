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

#include <dirent.h>
#include <sys/types.h>

#include <memory>
#include <sstream>

#include "callbacks_monitor.h"
#include "common.h"
#include "debuggee_labels.h"
#include "jvm_eval_call_stack.h"
#include "jvm_internals.h"
#include "jvmti_buffer.h"
#include "statistician.h"

#include "data_visibility_scrubbed_eval_call_stack.h"
#include "glob_data_visibility_policy.h"
#include "jvmti_agent.h"
#include "yaml_data_visibility_config_reader.h"

#ifndef STANDALONE_BUILD
#include "base/commandlineflags.h"
#include "base/logging_extensions.h"
#include "base/init_google.h"
#include "base/process_state.h"
#include "base/signal-handler.h"
#endif

#ifdef GCP_HUB_CLIENT
#include "jni_proxy_api_client_datetime.h"
#include "jni_proxy_gcpbreakpointlabelsprovider.h"
#include "jni_proxy_gcphubclient.h"
#include "jni_proxy_yamlconfigparser.h"
#include "jni_bridge.h"
#include "model_json.h"
#endif

#ifdef GCP_HUB_CLIENT
using std::string;
// TODO: retire this flag in favor of debuggee labels
// set through system properties.
ABSL_FLAG(string, cdbg_description_suffix, "",
          "additional text to be appended to debuggee description");

ABSL_FLAG(bool, enable_service_account_auth, false,
          "Enables service account authentication instead of relying on "
          "a local metadata service");

ABSL_FLAG(
    string, project_id, "",
    "Explicitly set GCP project ID used when service account authentication"
    "is enabled");

ABSL_FLAG(
    string, project_number, "",
    "(Deprecated) Explicitly set GCP project number used when service account "
    "authentication is enabled");

ABSL_FLAG(string, service_account_email, "",
          "Identifier of the service account");

ABSL_FLAG(string, service_account_json_file, "",
          "Path to JSON file containing private key of the service account");
#endif

static devtools::cdbg::JvmtiAgent* g_instance = nullptr;

static devtools::cdbg::JvmInternals* g_internals = nullptr;

static void CleanupAgent();

//
// JVMTI callback doesn't have any context, so we define global function and
// just forward the callback to the singleton instance of JvmtiAgent.
//

// VMInit JVMTI event callback.
static void JNICALL JvmtiOnVMInit(
    jvmtiEnv* jvmti,
    JNIEnv* jni,
    jthread thread) {
  devtools::cdbg::set_thread_jni(jni);

  g_instance->JvmtiOnVMInit(thread);
}


// VMDeatch JVMTI event callback.
static void JNICALL JvmtiOnVMDeath(
    jvmtiEnv *jvmti,
    JNIEnv* jni) {
  devtools::cdbg::set_thread_jni(jni);

  g_instance->JvmtiOnVMDeath();

  CleanupAgent();
}


// A class load event is generated when a class is first loaded before
// ClassPrepare event.
static void JNICALL JvmtiOnClassLoad(
    jvmtiEnv* jvmti,
    JNIEnv* jni,
    jthread thread,
    jclass cls) {
  devtools::cdbg::set_thread_jni(jni);

  g_instance->JvmtiOnClassLoad(thread, cls);
}


// A class prepare event is generated when class preparation is complete.
static void JNICALL JvmtiOnClassPrepare(
    jvmtiEnv* jvmti,
    JNIEnv* jni,
    jthread thread,
    jclass cls) {
  devtools::cdbg::set_thread_jni(jni);

  g_instance->JvmtiOnClassPrepare(thread, cls);
}

// Sent when a method is compiled and loaded into memory by the VM. If it is
// unloaded, the CompiledMethodUnload event is sent. If it is moved, the
// CompiledMethodUnload event is sent, followed by a new CompiledMethodLoad
// event. Note that a single method may have multiple compiled forms, and that
// this event will be sent for each form. Note also that several methods may
// be inlined into a single address range, and that this event will be sent
// for each method.
// Note that JNIEnv* is not available through jni() call.
static void JNICALL JvmtiOnCompiledMethodLoad(
    jvmtiEnv* jvmti,
    jmethodID method,
    jint code_size,
    const void* code_addr,
    jint map_length,
    const jvmtiAddrLocationMap* map,
    const void* compile_info) {
  // Make sure JNIEnv* is consistently unavailable (rather than sometimes
  // unavailable). This way if the codepath depends on JNI, it will always
  // fail making it easier to fix.
  JNIEnv* previous_jni = devtools::cdbg::set_thread_jni(nullptr);

  g_instance->JvmtiOnCompiledMethodLoad(
      method,
      code_size,
      code_addr,
      map_length,
      map,
      compile_info);

  devtools::cdbg::set_thread_jni(previous_jni);
}

// Sent when a compiled method is unloaded from memory. This event
// invalidates breakpoint set in this method. The method ID is no
// longer valid after this call.
// Note that JNIEnv* is not available through jni() call.
static void JNICALL JvmtiOnCompiledMethodUnload(
    jvmtiEnv* jvmti,
    jmethodID method,
    const void* code_addr) {
  // Make sure JNIEnv* is consistently unavailable (rather than sometimes
  // unavailable). This way if the codepath depends on JNI, it will always
  // fail making it easier to fix.
  JNIEnv* previous_jni = devtools::cdbg::set_thread_jni(nullptr);

  g_instance->JvmtiOnCompiledMethodUnload(method, code_addr);

  devtools::cdbg::set_thread_jni(previous_jni);
}

// Breakpoint JVMTI event callback.
static void JNICALL JvmtiOnBreakpoint(
    jvmtiEnv* jvmti,
    JNIEnv* jni,
    jthread thread,
    jmethodID method,
    jlocation location) {
  devtools::cdbg::set_thread_jni(jni);

  g_instance->JvmtiOnBreakpoint(thread, method, location);
}


// Installs the global callbacks in JVMTI.
static bool InitializeJvmtiCallbacks() {
  int err = 0;

  // Initialize set of requested callbacks.
  jvmtiEventCallbacks jvmti_callbacks;
  memset(&jvmti_callbacks, 0, sizeof(jvmti_callbacks));
  jvmti_callbacks.VMInit = &JvmtiOnVMInit;
  jvmti_callbacks.VMDeath = &JvmtiOnVMDeath;
  jvmti_callbacks.ClassLoad = &JvmtiOnClassLoad;
  jvmti_callbacks.ClassPrepare = &JvmtiOnClassPrepare;
  jvmti_callbacks.CompiledMethodLoad = &JvmtiOnCompiledMethodLoad;
  jvmti_callbacks.CompiledMethodUnload = &JvmtiOnCompiledMethodUnload;
  jvmti_callbacks.Breakpoint = &JvmtiOnBreakpoint;

  // Enable JvmtiOnVMInit callback.
  err = devtools::cdbg::jvmti()->SetEventCallbacks(
      &jvmti_callbacks,
      sizeof(jvmti_callbacks));
  if (err != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "Failed to set event callbacks: " << err;
    return false;
  }

  return true;
}

// Set default log directory to Java default temporary directory. The directory
// can still be customized through FLAGS_logdir flag.
static void TrySetDefaultLogDirectory() {
  jvmtiError err = JVMTI_ERROR_NONE;

  // Default logs directory in tomcat web server is "${catalina.base}/logs".
  devtools::cdbg::JvmtiBuffer<char> catalina_base_buffer;
  err = devtools::cdbg::jvmti()->GetSystemProperty(
      "catalina.base",
      catalina_base_buffer.ref());
  if (err == JVMTI_ERROR_NONE) {
    std::string tomcat_log_dir = catalina_base_buffer.get();
    tomcat_log_dir += "/logs";

    DIR* dir = opendir(tomcat_log_dir.c_str());
    if (dir) {
      closedir(dir);
      absl::SetFlag(&FLAGS_log_dir, tomcat_log_dir);
      return;
    }
  }

  // Directory pointed by "java.io.tmpdir" is a good default for logs.
  devtools::cdbg::JvmtiBuffer<char> tmpdir_buffer;
  err = devtools::cdbg::jvmti()->GetSystemProperty(
      "java.io.tmpdir",
      tmpdir_buffer.ref());
  if (err == JVMTI_ERROR_NONE) {
    absl::SetFlag(&FLAGS_log_dir, tmpdir_buffer.get());
  }
}


static void InitEnvironment(const char* options) {
  // Split agent options to command line argument style data structure.
  std::stringstream ss((options != nullptr) ? options : "");
  std::string cur_option;
  std::vector<std::string> split_options;
  while (std::getline(ss, cur_option, ',')) {
    split_options.push_back(std::move(cur_option));
  }

  std::vector<char*> argv_vector;
  argv_vector.push_back(const_cast<char*>("cdbg_java_agent"));
  for (const std::string& split_option : split_options) {
    argv_vector.push_back(const_cast<char*>(split_option.c_str()));
  }

  // Change default options to never log to stderr (since it may impact the
  // application we are debugging).
  absl::SetFlag(&FLAGS_logtostderr, false);
  absl::SetFlag(&FLAGS_stderrthreshold,
                3);  // By default only fatal errors go to stderr.
  TrySetDefaultLogDirectory();

  int argc = argv_vector.size();
  char** argv = &argv_vector[0];

#ifdef STANDALONE_BUILD
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
#else
#endif  // STANDALONE_BUILD

  devtools::cdbg::InitializeStatisticians();
  devtools::cdbg::CallbacksMonitor::InitializeSingleton(
      devtools::cdbg::kDefaultMaxCallbackTimeMs);
}


// Releases all agent objects. This function is usually first called from
// "VMDeath" callback and then from "Agent_OnUnload" (when it doesn't do
// anything). "VMDeath" callback is skipped if JVM initialization failed.
static void CleanupAgent() {
  if (g_instance) {
    delete g_instance;
    g_instance = nullptr;
  }

  if (g_internals != nullptr) {
    g_internals->ReleaseRefs();
    delete g_internals;
    g_internals = nullptr;
  }
}


// Entry point for JVMTI agent.
JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM* vm, char* options, void* reserved) {
  // Get JVMTI interface.
  jvmtiEnv* jvmti = nullptr;
  int err = vm->GetEnv(reinterpret_cast<void**>(&jvmti), JVMTI_VERSION);
  if (err != JNI_OK) {
    return 1;
  }

  devtools::cdbg::set_jvmti(jvmti);

  InitEnvironment(options);

  g_internals = new devtools::cdbg::JvmInternals;

#ifdef STANDALONE_BUILD
  LOG(INFO) << "Build time: " __DATE__ " " __TIME__;
#endif  // STANDALONE_BUILD

  // Initialize JVMTI callbacks.
  if (!InitializeJvmtiCallbacks()) {
    return 1;
  }


  // Instantiate the Bridge interface. This can only be done after OnLoad() is
  // completed because flags are not valid before that.
#ifdef GCP_HUB_CLIENT
  std::unique_ptr<devtools::cdbg::Bridge> bridge(
      new devtools::cdbg::JniBridge(
          [] () {
              return jniproxy::GcpHubClient()->NewObject()
                  .Release(devtools::cdbg::ExceptionAction::LOG_AND_IGNORE);
          },
          devtools::cdbg::BreakpointToJson,
          devtools::cdbg::BreakpointFromJson));
#endif

// Start the agent.

  // glob_policy is captured in a lambda below and sent to JvmtiAgent for final
  // ownership after the needed initialization data is ready.  This is done so
  // that DataVisibilityScrubbedEvalCallStack can access the object.
  //
  // This is assuming that the callback is actually called.  If not, then the
  // object will never be claimed (i.e. memory leak).
  devtools::cdbg::GlobDataVisibilityPolicy* glob_policy =
      new devtools::cdbg::GlobDataVisibilityPolicy();
  g_instance = new devtools::cdbg::JvmtiAgent(
      g_internals,
      std::unique_ptr<devtools::cdbg::EvalCallStack>(
          new devtools::cdbg::DataVisibilityScrubbedEvalCallStack(
            std::unique_ptr<devtools::cdbg::EvalCallStack>(
                new devtools::cdbg::JvmEvalCallStack),
            glob_policy)),
      {
        jniproxy::BindDateTimeWithClassLoader,
        jniproxy::BindGcpBreakpointLabelsProviderWithClassLoader,
        jniproxy::BindGcpHubClientWithClassLoader,
        jniproxy::BindYamlConfigParserWithClassLoader,
      },
      std::move(bridge),
      [] () {
          return jniproxy::GcpBreakpointLabelsProvider()->NewObject()
              .Release(devtools::cdbg::ExceptionAction::LOG_AND_IGNORE);
      },
      [] () {
        // There is no user id provider in GCE environment.
        return nullptr;
      },
      [glob_policy](devtools::cdbg::ClassPathLookup* class_path_lookup,
                    devtools::cdbg::DebuggeeLabels* debuggee_labels) {
        std::string yaml_config_source;
        glob_policy->SetConfig(
            devtools::cdbg::ReadYamlDataVisibilityConfiguration(
                class_path_lookup, &yaml_config_source));
        debuggee_labels->Set(
            devtools::cdbg::DebuggeeLabels::kBlocklistSourceLabel,
            yaml_config_source);
        return std::unique_ptr<devtools::cdbg::DataVisibilityPolicy>(
            glob_policy);
      },
      true,
      true);
  if (!g_instance->OnLoad()) {
    return 1;
  }

  return 0;
}


// Entry point for JVMTI agent.
JNIEXPORT void JNICALL Agent_OnUnload(JavaVM* vm) {
  CleanupAgent();

  devtools::cdbg::CallbacksMonitor::CleanupSingleton();
  devtools::cdbg::CleanupStatisticians();
}

