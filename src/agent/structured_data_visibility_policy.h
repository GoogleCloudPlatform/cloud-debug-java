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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_STRUCTURED_DATA_VISIBILITY_POLICY_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_STRUCTURED_DATA_VISIBILITY_POLICY_H_

#include <map>
#include <memory>
#include <vector>

#include "common.h"
#include "data_visibility_policy.h"

namespace devtools {
namespace cdbg {

// Exposes application specific configuration of debugger invisible elements.
// Such elements are methods, local variables, arguments and fields.
//
// This class has only immutable data structures, so it is thread safe.
class StructuredDataVisibilityPolicy : public DataVisibilityPolicy {
 public:
  // Raw configuration of visibility rules for classes, variables, etc. The
  // agent would read this configuration from the .JAR file in an
  // environment specific format.
  struct Config {
    struct Variable {
      // Name of the local variable or the argument.
      std::string name;

      // If true, the value value of this variable will not be captured by the
      // Cloud Debugger.
      bool invisible = false;
    };

    struct Field {
      // Name of the field (e.g. "myField").
      std::string name;

      // If true, the value of this field will be omitted by the Cloud Debugger.
      bool invisible = false;
    };

    struct Method {
      // Name of the method (e.g. "myMethod").
      std::string name;

      // JVMTI signature of the method (e.g. "(IIJ)V").
      std::string signature;

      // Configuration of method variables.
      std::vector<Variable> variables;
    };

    struct Class {
      // If true, all fields in this class and all local variables in all the
      // methods of this class will be omitted by the Cloud Debugger. This
      // will also apply to nested classes.
      bool invisible = false;

      // Configuration of fields in this class.
      std::vector<Field> fields;

      // Configuration of methods in this class.
      std::vector<Method> methods;

      // The key is a simple name of the class (e.g. "MyStaticClass"). It does
      // not include name of the parent class or a package.
      std::map<std::string, Class> nested_classes;
    };

    struct Package {
      // If true, all values of all variables inside the package will be omitted
      // by the Cloud Debugger.
      bool invisible = false;

      // Configuration of top level classes in this package.
      // The key is a simple name of the class (e.g. "MyClass"). It does
      // not include name of the parent class or a package.
      std::map<std::string, Class> classes;
    };

    // The key is an internal name of the package (e.g. "com/google/common").
    std::map<std::string, Package> packages;
  };

  // Initializes with an empty config that blocks nothing.
  StructuredDataVisibilityPolicy() { }

  // Sets the config.
  void SetConfig(Config config) {
    config_ = std::move(config);
  }

  std::unique_ptr<Class> GetClassVisibility(jclass cls) override;

  bool HasSetupError(std::string* error) const override { return false; }

 private:
  // Raw visibility configuration. It is equivalent to the format stored in
  // the .JAR file. This configuration packs data in an efficient way, but it
  // is relatively slow for lookup.
  Config config_;

  DISALLOW_COPY_AND_ASSIGN(StructuredDataVisibilityPolicy);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_STRUCTURED_DATA_VISIBILITY_POLICY_H_
