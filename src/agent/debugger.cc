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

#include "debugger.h"

#include <cstdint>
#include <memory>

#include "jni_utils.h"
#include "jvm_breakpoint.h"
#include "jvm_breakpoints_manager.h"
#include "safe_method_caller.h"
#include "statistician.h"
#include "stopwatch.h"

ABSL_FLAG(int32, cdbg_class_files_cache_size,
          1024 * 1024,  // 1 MB.
          "Cache size for class files used in safe method caller");

namespace devtools {
namespace cdbg {

Debugger::Debugger(
    Scheduler<>* scheduler, Config* config, EvalCallStack* eval_call_stack,
    std::unique_ptr<MethodLocals> method_locals,
    std::unique_ptr<ClassMetadataReader> class_metadata_reader,
    std::unique_ptr<StatusMessageModel> setup_error,
    ClassPathLookup* class_path_lookup,
    std::unique_ptr<DynamicLogger> dynamic_logger,
    std::function<std::unique_ptr<BreakpointLabelsProvider>()> labels_factory,
    std::function<std::unique_ptr<UserIdProvider>()> user_id_provider_factory,
    FormatQueue* format_queue, CanaryControl* canary_control /* = nullptr */)
    : config_(config),
      eval_call_stack_(eval_call_stack),
      method_locals_(std::move(method_locals)),
      class_metadata_reader_(std::move(class_metadata_reader)),
      setup_error_(std::move(setup_error)),
      object_evaluator_(class_metadata_reader_.get()),
      class_files_cache_(&class_indexer_,
                         absl::GetFlag(FLAGS_cdbg_class_files_cache_size)),
      dynamic_logger_(std::move(dynamic_logger)) {
  evaluators_.class_path_lookup = class_path_lookup;
  evaluators_.class_indexer = &class_indexer_;
  evaluators_.eval_call_stack = eval_call_stack_;
  evaluators_.method_locals = method_locals_.get();
  evaluators_.class_metadata_reader = class_metadata_reader_.get();
  evaluators_.object_evaluator = &object_evaluator_;
  evaluators_.method_caller_factory = [this](Config::MethodCallQuotaType type) {
    return std::unique_ptr<MethodCaller>(new SafeMethodCaller(
        config_,
        config_->GetQuota(type),
        &class_indexer_,
        &class_files_cache_));
  };
  evaluators_.labels_factory = std::move(labels_factory);
  evaluators_.user_id_provider_factory = std::move(user_id_provider_factory);

  auto factory = [this, scheduler, format_queue](
      BreakpointsManager* breakpoints_manager,
      std::unique_ptr<BreakpointModel> breakpoint_definition) {
    return std::make_shared<JvmBreakpoint>(
        scheduler,
        &evaluators_,
        format_queue,
        dynamic_logger_.get(),
        breakpoints_manager,
        CopySetupError(),
        std::move(breakpoint_definition));
  };

  breakpoints_manager_.reset(new JvmBreakpointsManager(
      factory,
      &evaluators_,
      format_queue,
      canary_control));
}

Debugger::~Debugger() {
  breakpoints_manager_->Cleanup();
  class_indexer_.Cleanup();
}


void Debugger::Initialize() {
  Stopwatch stopwatch;

  LOG(INFO) << "Initializing Java debuglet";

  // Get the set of already loaded classes. Other classes will be indexed
  // as they get loaded by JVM.
  class_indexer_.Initialize();

  // Initialize pretty printers.
  object_evaluator_.Initialize();

  LOG(INFO) << "Debugger::Initialize initialization time: "
            << stopwatch.GetElapsedMillis() << " ms";
}

std::unique_ptr<StatusMessageModel> Debugger::CopySetupError() {
  return setup_error_ == nullptr ?
      nullptr :
      StatusMessageBuilder(*setup_error_).build();
}

void Debugger::JvmtiOnClassPrepare(jthread thread, jclass cls) {
  // Log the accumulated time. "OnClassPrepare" handler is a tax we are
  // paying upfront whether debugger is used or not. It is therefore very
  // important to keep this function fast.
  ScopedStat ss(statClassPrepareTime);

  // Index the new class.
  class_indexer_.JvmtiOnClassPrepare(cls);
}


// Note: JNIEnv* is not available through jni() call.
void Debugger::JvmtiOnCompiledMethodUnload(
    jmethodID method,
    const void* code_addr) {
  eval_call_stack_->JvmtiOnCompiledMethodUnload(method);
  method_locals_->JvmtiOnCompiledMethodUnload(method);
  breakpoints_manager_->JvmtiOnCompiledMethodUnload(method);
}


void Debugger::JvmtiOnBreakpoint(
    jthread thread,
    jmethodID method,
    jlocation location) {
  breakpoints_manager_->JvmtiOnBreakpoint(thread, method, location);
}


void Debugger::SetActiveBreakpointsList(
    std::vector<std::unique_ptr<BreakpointModel>> breakpoints) {
  breakpoints_manager_->SetActiveBreakpointsList(std::move(breakpoints));
}


}  // namespace cdbg
}  // namespace devtools
