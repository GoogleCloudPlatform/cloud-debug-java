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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_CAPTURE_DATA_COLLECTOR_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_CAPTURE_DATA_COLLECTOR_H_

#include <list>
#include <memory>

#include "breakpoint_labels_provider.h"
#include "class_indexer.h"
#include "class_metadata_reader.h"
#include "common.h"
#include "eval_call_stack.h"
#include "jobject_map.h"
#include "jvm_evaluators.h"
#include "model.h"
#include "readers_factory.h"
#include "type_util.h"
#include "user_id_provider.h"

namespace devtools {
namespace cdbg {

// Number of top frames for which BreakpointsManager will be reading values of
// local variables.
constexpr int kMethodLocalsFrames = 5;

// Quota for total size of all the variables we collect. Once this quota is
// reached, the data collection stops. The main purpose of this limit is to
// limit the time we pause the service on a breakpoint event.
constexpr int kBreakpointMaxCaptureSize = 65536;

struct CompiledExpression;
class ExpressionEvaluator;
class MethodLocals;
class ObjectEvaluator;
class ClassFilesCache;

// Orchestrates functionality of all the evaluation classes together to
// collect the state of the program upon breakpoint hit. This includes call
// stack, values of local variables and some referenced Java objects. The
// "CaptureDataCollector" has a quota on how much time/memory it can spare for
// collection
// (to keep impact on the debugged service minimal).
// The "CaptureDataCollector" class is supposed to be created for each
// breakpoint hit and should not be reused. Therefore the actual data collection
// shoud happen immediately after construction of the object.
// The collection process has two phases:
// 1. Actual reading variables from JVM. This phase happens while the thread
//    that hit the breakpoint is paused. The collection phase should be
//    heavily optimized and should defer as much as possible to formatting
//    phase.
// 2. Formatting of the data collected in the first phase into the message that
//    will be transmitted to the Hub service.
class CaptureDataCollector {
 public:
  explicit CaptureDataCollector(JvmEvaluators* evaluators);

  virtual ~CaptureDataCollector();

  // Reads the state of the the debugged program.
  void Collect(
      const std::vector<CompiledExpression>& watches,
      jthread thread);

  // Releases the all global reference to Java objects. This function must be
  // called before the object is destroyed. After "ReleaseRefs" has been called,
  // "Format" should not be called.
  void ReleaseRefs();

  // Formats the captured data into the specified Breakpoint message.
  void Format(BreakpointModel* breakpoint) const;

 private:
  // Information about the call frame that we keep around for formatting.
  struct CallFrame {
    // Reference to the name of the class and method of the call frame.
    int frame_info_key;

    // Collected method arguments.
    std::vector<NamedJVariant> arguments;

    // Collected local variables.
    std::vector<NamedJVariant> local_variables;
  };

  // Collected state of a single memory object.
  struct MemoryObject {
    // Global reference to Java object. The reference is held by one of the
    // "JVariant" instances, which is either a local variable or a member
    // of another memory object. Multiple variables may point to the same
    // object. Note that comparison of object references has to be done with
    // JNI::IsSameObject function.
    jobject object_ref { nullptr };

    // Optional status message produced during object evaluation. It can be
    // either error or informational. Example of such a message is: "only first
    // 10 elements out of 1578 were captured".
    StatusMessageModel status;

    // Member variables of the Java object.
    std::vector<NamedJVariant> members;
  };

  // Holds a result of expression evaluation. These scenarios are supported:
  // 1. Watched expression was valid and the value was captured successfully:
  //        compile_error_message.format is empty
  //        value.status.description.format is empty
  // 2. Watched expression was valid, but the value could not be captured:
  //        compile_error_message.format is empty
  //        value.status.description.format is non-empty
  // 3. Watched expression was invalid and could not compiled:
  //        compile_error_message.format is non-empty
  struct EvaluatedExpression {
    // Original expression string to populate in variable name.
    std::string expression;

    // Compilation error message or empty if expression was compiled
    // successfully.
    FormatMessageModel compile_error_message;

    // Expression evaluation result (whether successful or not).
    NamedJVariant evaluation_result;
  };

  // Decodes call frame key into a user friendly function name (like
  // "MyOrg.MyClass.MyMethod").
  std::string GetFunctionName(int depth) const;

  // Retrieves the location of the source code at the specified call frame.
  std::unique_ptr<SourceLocationModel> GetCallFrameSourceLocation(
      int depth) const;

  // Evaluates a single watched expression and appends the result to
  // "watch_results_". If "watch" is nullptr or evaluation fails, the function
  // sets an error message in "result".
  void EvaluateWatchedExpression(
      const EvaluationContext& evaluation_context,
      const ExpressionEvaluator& watch_evaluator,
      NamedJVariant* result);

  // Applies all internal bookkeeping to set the specified variable (quota
  // calculation and list of references to memory objects).
  void PostProcessVariable(const NamedJVariant& variable);

  // Applies "PostProcessVariable" to an array of variables.
  void PostProcessVariables(const std::vector<NamedJVariant>& variables);

  // Adds the referenced object to the list of member objects that need to
  // be collected. If "var" is not a reference, the function does nothing.
  // If the memory object was already referenced (by either local variable or
  // another memory object), no action is taken as well.
  void EnqueueRef(const NamedJVariant& var);

  // Checks whether this instance has more quota to evaluate additional memory
  // objects.
  bool CanCollectMoreMemoryObjects() const;

  // Formats list of "NamedJVariant" into the corresponding API message
  // structure.
  void FormatVariablesArray(
      const std::vector<NamedJVariant>& source,
      std::vector<std::unique_ptr<VariableModel>>* target) const;

  // Prints results of watched expressions evaluation into the corresponding
  // API message structure.
  void FormatWatchedExpressions(
      std::vector<std::unique_ptr<VariableModel>>* target) const;

  // Formats a single "JVariant" instance into the corresponding API message
  // structure.
  std::unique_ptr<VariableModel> FormatVariable(
      const NamedJVariant& source,
      bool is_watched_expression) const;

 protected:
  // Reads local variables at a particular call frame. The function is marked
  // as virtual and protected for unit testing purposes.
  virtual void ReadLocalVariables(
      const EvaluationContext& evaluation_context,
      jmethodID method,
      jlocation location,
      std::vector<NamedJVariant>* arguments,
      std::vector<NamedJVariant>* local_variables);

 private:
  // Collects and evaluates watch expressions, adding pending memory objects
  // to "unexplored_memory_objects_".
  void CollectWatchExpressions(
      const std::vector<CompiledExpression>& watches,
      jthread thread,
      const std::vector<EvalCallStack::JvmFrame>& jvm_frames);

  // Collects and evaluates call frames, adding pending memory objects
  // to "unexplored_memory_objects_".
  void CollectCallStack(
      jthread thread,
      const std::vector<EvalCallStack::JvmFrame>& jvm_frames,
      MethodCaller* pretty_printers_method_caller);

  // Evaluates objects enqueued into "unexplored_memory_objects_". Moves them
  // into "explored_memory_objects_". Updates map of indexes for object
  // references in "explored_memory_objects_".
  void EvaluateEnqueuedObjects(
      bool is_watch_expression,
      MethodCaller* pretty_printers_method_caller);

  // Adds additional base64 and (possibly) utf8 fields for a byte array.
  void FormatByteArray(
      const std::vector<NamedJVariant>& source,
      std::vector<std::unique_ptr<VariableModel>>* target) const;

  // Bundles all the evaluation classes together. Evaluators are guaranteed
  // to be valid throughout the lifetime of "CaptureDataCollector".
  // Not owned by this class.
  JvmEvaluators* const evaluators_;

  // Captures information about local environment into breakpoint labels.
  std::unique_ptr<BreakpointLabelsProvider> breakpoint_labels_provider_;

  // Captures information about end user identity.
  std::unique_ptr<UserIdProvider> user_id_provider_;

  // Captured data of call frames that can be formatted into the message
  // for Hub service.
  std::vector<CallFrame> call_frames_;

  // Evaluated watched expressions.
  std::vector<EvaluatedExpression> watch_results_;

  // This map is used to make sure an object that is referenced from multiple
  // locals/expressions is evaluated only once, and all subsequent references
  // reuse the evaluation result.
  JobjectMap<JObject_NoRef, int> unique_objects_;

  // Set of pending memory objects. Newly discovered unique memory objects are
  // appended to the end of the list. This scheme enables BFS-like exporation
  // of the object tree. Use linked list here (rather than vector) so that we
  // can add new elements without relocating existing elements.
  std::list<MemoryObject> unexplored_memory_objects_;

  // Set of collected memory objects. Objects in the list are
  // identified by index. Use linked list here (rather than vector) so that we
  // can add new elements without relocating existing elements.
  // Since C++11, list.size() is no longer O(n). It is now O(1) so we can use
  // directly instead of keeping a separate counter for the list size.
  // http://www.cplusplus.com/reference/list/list/size/
  std::list<MemoryObject> explored_memory_objects_;

  // Maps discovered Java objects to index in "explored_memory_objects_".
  // The map does not hold any reference to Java objects and assumes that the
  // global reference is maintained by "VariableFormatter" instances somewhere
  // in this class.
  JobjectMap<JObject_NoRef, int> explored_object_index_map_;

  // Total approximated size of collected variables. This size is compared
  // against a quota. The data collection will stop once the threshold has been
  // reached. Both variables name and data are computed. This size is not
  // precise (the formatted message might be smaller or larger). The size does
  // not account for formatting overhead in the actual message.
  int total_variables_size_ = 0;

  DISALLOW_COPY_AND_ASSIGN(CaptureDataCollector);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_CAPTURE_DATA_COLLECTOR_H_
