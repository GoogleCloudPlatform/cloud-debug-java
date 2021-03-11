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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_CONFIG_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_CONFIG_H_

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <set>

#include "common.h"
#include "jvariant.h"
#include "method_call_result.h"

namespace devtools {
namespace cdbg {

class SafeMethodCaller;

// Stores debuglet configuration. The configuration is immutable, hence the
// class is thread safe.
class Config {
 public:
  // Policy for a single method.
  struct Method {
    // Possible actions the debugger can take for a method.
    enum class CallAction {
      // Method call not allowed because the method is not safe.
      Block,

      // Allow calling this method without dynamically verifying its safety.
      // Should only be used on methods that we know have no side effects.
      // Note that the method may invoke its arguments making it unsafe. For
      // example "String.format" is unsafe because it calls "toString" on the
      // its arguments and those might be unsafe.
      Allow,

      // Interpret method instructions allowing the method call as long as
      // it is safe.
      Interpret,
    };

    // Method name. If empty matches any method.
    std::string name;

    // Method signature. If empty matches all methods with the right name.
    std::string signature;

    // Action for the debugger to take when calling this method.
    CallAction action = CallAction::Block;

    // Optional callback to invoke before the method is called. The callback
    // can modify the incoming arguments and can fail the method call. This
    // is only applicable if "action" is Allow. NanoJava interpreter takes care
    // of all the safety issues in runtime.
    std::function<MethodCallResult(
        SafeMethodCaller*,
        jobject,
        std::vector<JVariant>*)> thunk;

    // If true this rule will apply to derived classes that do not overload
    // this method. For example consider "x.getClass()".
    bool applies_to_derived_classes = false;

    // The method call is only allowed if the target object is a temporary one
    // (i.e. was created as part of debugger expression evaluation and is not
    // connected to application state). This is only applicable if "action"
    // is Allow. NanoJava interpreter takes care of all the safety issues in
    // runtime.
    bool require_temporary_object = false;

    // Mark returned object as a temporary one (not connected to graph of
    // objects allocated by the application). This is only applicable if
    // "action" is Allow. NanoJava interpreter takes care of all the safety
    // issues at runtime (as opposed to static validation in Oracle JVM).
    bool returns_temporary_object = false;
  };

  // Defines quota on how much time and memory we are ready to spend in safe
  // method caller. The quota is contextual: e.g. all method calls in a single
  // instance of dynamic log. It is not per-method.
  struct MethodCallQuota {
    // Maximum number of classes that the code is allowed to load during
    // method execution. Each class load takes between 100-400 microseconds.
    int32_t max_classes_load{0};

    // Maximum number of instructions that the NanoJava interpreter is allowed
    // to execute. Note that each instruction has a different cost, so this
    // limit does not provide exact timing guarantees.
    int32_t max_interpreter_instructions{0};
  };

  // Defines the type of quota (i.e. where the quota is used).
  enum MethodCallQuotaType {
    // Call quota for expression evaluation (used in conditions and watched
    // expressions).
    EXPRESSION_EVALUATION,

    // Call quota for pretty printers. For example pretty printers will invoke
    // "iterator()" method on all classes implementing "Iterable" interface.
    PRETTY_PRINTERS,

    // Call quota for dynamic logs.
    DYNAMIC_LOG,

    // Total number of different quota types. Not to be used.
    MAX_TYPES
  };

  // Builds the immutable debuglet configuration. This class is not thread safe.
  class Builder {
   public:
    Builder();

    // Sets safe caller rules for methods in the specified class.
    Builder& SetClassConfig(const std::string& class_signature,
                            std::vector<Method> rules);

    // Adds a single method rule. Creates class configuration as needed.
    // "SetClassConfig" doesn't have to be called before calling this
    // function.
    Builder& AddMethodRule(const std::string& class_signature, Method rule);

    // Sets default safe caller method rule. It will be used when none of
    // the method rules set with "SetClassConfig" and "AddMethodRule" match.
    Builder& SetDefaultMethodRule(Method rule);

    Builder& SetQuota(
        MethodCallQuotaType quota_type,
        const MethodCallQuota& quota);

    std::unique_ptr<Config> Build() { return std::move(config_); }

   private:
    std::unique_ptr<Config> config_ { new Config };

    DISALLOW_COPY_AND_ASSIGN(Builder);
  };

  ~Config();

  // Gets configuration of the specified method. "method_cls_signature" is the
  // class that implements the method. "object_cls_signature" is the object
  // class.
  // For example consider calling "x.wait()", where "x" is Integer.
  // "method_cls_signature" will be "Ljava/lang/Object;" and
  // "object_cls_signature" will be "Ljava/lang/Integer;".
  // For static methods, "object_cls_signature" is either equal to
  // "method_cls_signature" or its subclass.
  const Method& GetMethodRule(const std::string& method_cls_signature,
                              const std::string& object_cls_signature,
                              const std::string& method_name,
                              const std::string& method_signature) const;

  // Gets the method call quota for the specified use.
  const MethodCallQuota& GetQuota(MethodCallQuotaType type) const {
    DCHECK((type >= 0) && (type < arraysize(quota_)));
    return quota_[type];
  }

 private:
  // Default configuration. All method calls are blocked. All quota settings
  // are zero.
  Config();

 private:
  // Non default configuration for class methods. The key is class signature.
  // The list is scanned sequentially until a match is found. If no matches
  // found, the default is Method::kDefault.
  std::map<std::string, std::vector<Method>> classes_;

  // Default behavior for all class methods unless a method has an explicit
  // configuration.
  Method default_rule_;

  // Method call quotas.
  MethodCallQuota quota_[MAX_TYPES];

  DISALLOW_COPY_AND_ASSIGN(Config);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_CONFIG_H_
