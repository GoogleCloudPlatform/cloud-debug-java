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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_EVALUATORS_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_EVALUATORS_H_

namespace devtools {
namespace cdbg {

class ClassIndexer;
class ClassMetadataReader;
class ClassPathLookup;
class EvalCallStack;
class MethodLocals;
class Config;
class ObjectEvaluator;
class ClassFilesCache;

// Convinience structure that bundles all the evaluation classes together
// to avoid passing a lot of parameters.
// All the fields have to be set.
struct JvmEvaluators {
  // Debugger agent configuration.
  // TODO(vlif): remove it from this structure.
  const Config* config = nullptr;

  // Proxy for ClassPathLookup class implemented in
  // cdbg_java_agent_internals.jar
  // TODO(vlif): remove it from this structure.
  ClassPathLookup* class_path_lookup = nullptr;

  // Indexes all the available Java classes and locates classes based on
  // a type name.
  ClassIndexer* class_indexer = nullptr;

  // Reads stack trace upon a breakpoint hit.
  EvalCallStack* eval_call_stack = nullptr;

  // Evaluates values of local variables in a given call frame.
  MethodLocals* method_locals = nullptr;

  // Factory class for "InstanceFieldReader" objects.
  ClassMetadataReader* class_metadata_reader = nullptr;

  // Evaluates members of Java objects.
  ObjectEvaluator* object_evaluator = nullptr;

  // Global cache of loaded class files for safe caller.
  ClassFilesCache* class_files_cache = nullptr;
};


}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_EVALUATORS_H_
