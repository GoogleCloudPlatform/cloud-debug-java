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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_DEBUGGER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_DEBUGGER_H_

#include <atomic>
#include <memory>

#include "breakpoint_labels_provider.h"
#include "canary_control.h"
#include "class_files_cache.h"
#include "class_metadata_reader.h"
#include "common.h"
#include "dynamic_logger.h"
#include "eval_call_stack.h"
#include "jvm_class_indexer.h"
#include "jvm_evaluators.h"
#include "jvm_object_evaluator.h"
#include "method_locals.h"
#include "model.h"
#include "scheduler.h"
#include "user_id_provider.h"

namespace devtools {
namespace cdbg {

class BreakpointsManager;
class ClassPathLookup;
class FormatQueue;

// Debugger module loaded by JVMTI agent. The module is separated from the
// agent to allow dynamic loading and unloading as directed by the Hub
// through "RegisterDebuggee" message.
class Debugger {
 public:
  // All pointer arguments are not owned by this class and must outlive
  // this object.
  Debugger(
      Scheduler<>* scheduler,
      Config* config,
      EvalCallStack* eval_call_stack,
      std::unique_ptr<MethodLocals> method_locals,
      std::unique_ptr<ClassMetadataReader> class_metadata_reader,
      std::unique_ptr<StatusMessageModel> setup_error,
      ClassPathLookup* class_path_lookup,
      std::unique_ptr<DynamicLogger> dynamic_logger,
      std::function<std::unique_ptr<BreakpointLabelsProvider>()> labels_factory,
      std::function<std::unique_ptr<UserIdProvider>()> user_id_provider_factory,
      FormatQueue* format_queue,
      CanaryControl* canary_control = nullptr);

  ~Debugger();

  // Initializes the debugger. Note that this class may be receiving JVMTI
  // notifications before "Initialize" is called and while "Initialize" is
  // being called. This is to missing CLASS_PREPARE events.
  void Initialize();

  // A class prepare event is generated when class preparation is complete.
  void JvmtiOnClassPrepare(jthread thread, jclass cls);

  // Sent when a compiled method is unloaded from memory. This event
  // invalidates breakpoint set in this method. The method ID is no
  // longer valid after this call.
  void JvmtiOnCompiledMethodUnload(jmethodID method, const void* code_addr);

  // Callback for breakpoint event.
  void JvmtiOnBreakpoint(
      jthread thread,
      jmethodID method,
      jlocation location);

  // Sets the list of active breakpoints.
  void SetActiveBreakpointsList(
      std::vector<std::unique_ptr<BreakpointModel>> breakpoints);

 private:
  // Makes a copy of setup_error_ and returns it
  std::unique_ptr<StatusMessageModel> CopySetupError();

 private:
  // Debugger agent configuration.
  Config* const config_;

  // Reads stack trace upon a breakpoint hit. Not owned by this class.
  EvalCallStack* const eval_call_stack_;

  // Indexes all the available Java classes and locates classes based on
  // a type name.
  JvmClassIndexer class_indexer_;

  // Evaluates values of local variables in a given call frame.
  std::unique_ptr<MethodLocals> method_locals_;

  // Indexes and caches class field readers and class methods.
  std::unique_ptr<ClassMetadataReader> class_metadata_reader_;

  // If set to non-null, breakpoints will immediately be set to this status
  std::unique_ptr<StatusMessageModel> setup_error_;

  // Evaluates members of Java objects.
  JvmObjectEvaluator object_evaluator_;

  // Global cache of loaded class files for safe caller.
  ClassFilesCache class_files_cache_;

  // Bundles all the evaluation classes together.
  JvmEvaluators evaluators_;

  // Logger for dynamic logs.
  std::unique_ptr<DynamicLogger> dynamic_logger_;

  // Manages breakpoints and computes the state of the program on breakpoint
  // hit.
  std::unique_ptr<BreakpointsManager> breakpoints_manager_;

  DISALLOW_COPY_AND_ASSIGN(Debugger);
};


}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_DEBUGGER_H_
