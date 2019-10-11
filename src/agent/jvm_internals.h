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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_INTERNALS_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_INTERNALS_H_

#include "class_path_lookup.h"
#include "common.h"
#include "jni_utils.h"
#include "model.h"

namespace devtools {
namespace cdbg {

class JvmInternals : public ClassPathLookup {
 public:
  // Loads helper functionality implemented in Java into the local JVM. A custom
  // "ClassLoader" is used to isolate the loaded namespace from the debugged
  // program.
  bool LoadInternals();

  // Loads the helped functionality with the specified class loader.
  bool LoadInternalsWithClassLoader(jobject internals_class_loader);

  // Creates instance of "ClassPathLookup". This is expensive operation
  // because it involves scanning all application JARs. The caller should
  // postpone this call until the functionality is actually needed.
  bool CreateClassPathLookupInstance(
      bool use_default_class_path,
      jobject extra_class_path);

  // Returns true if "CreateInstance" was previously called and succeeded.
  bool HasInstance() const { return class_path_lookup_.instance; }

  // Releases all global references held by this class.
  void ReleaseRefs();

  void ResolveSourceLocation(const std::string& source_path, int line_number,
                             ResolvedSourceLocation* location) override;

  std::vector<std::string> FindClassesByName(
      const std::string& class_name) override;

  std::string ComputeDebuggeeUniquifier(const std::string& iv) override;

  std::set<std::string> ReadApplicationResource(
      const std::string& resource_path) override;

  jobject class_loader_obj() const { return class_loader_obj_; }

 private:
  // Loads and instantiates "InternalsClassLoader".
  bool LoadClassLoader(const std::string& agentdir);

  // Loads Java classes from "cdbg_java_agent_internals.jar".
  bool LoadClasses();

  // Converts Java com.google.devtools.cdbg.debuglets.java.FormatMessage
  // object to FormatMessageModel.
  FormatMessageModel ConvertFormatMessage(jobject obj_format_message);

 private:
  // Global reference to Java instance of "InternalsClassLoader" class.
  jobject class_loader_obj_ { nullptr };

  // com.google.devtools.cdbg.debuglets.java.ClassPathLookup class.
  struct {
    // Class object.
    JavaClass cls;

    // Instance of ClassPathLookup class.
    jobject instance { nullptr };

    // ClassPathLookup constructor.
    jmethodID constructor { nullptr };

    // ClassPathLookup.resolveSourceLocation method.
    jmethodID resolve_source_location_method { nullptr };

    // ClassPathLookup.findClassesByName method.
    jmethodID find_classes_by_name_method { nullptr };

    // ClassPathLookup.computeDebuggeeUniquifier method.
    jmethodID compute_debuggee_uniquifier_method { nullptr };

    // ClassPathLookup.readApplicationResource method.
    jmethodID read_application_resource_method { nullptr };
  } class_path_lookup_;

  // com.google.devtools.cdbg.debuglets.java.ResolvedSourceLocation class.
  struct {
    // Class object.
    JavaClass cls;

    // ResolvedSourceLocation.getErrorMessage method.
    jmethodID get_error_message_method { nullptr };

    // ResolvedSourceLocation.getClassSignature method.
    jmethodID get_class_signature_method { nullptr };

    // ResolvedSourceLocation.getMethodName method.
    jmethodID get_method_name_method { nullptr };

    // ResolvedSourceLocation.getMethodDescriptor method.
    jmethodID get_method_descriptor_method { nullptr };

    // ResolvedSourceLocation.getAdjustedLineNumber method.
    jmethodID get_adjusted_line_number_method { nullptr };
  } resolved_source_location_;

  // com.google.devtools.cdbg.debuglets.java.FormatMessage class.
  struct {
    // Class object.
    JavaClass cls;

    // FormatMessage.getFormat method.
    jmethodID get_format_method { nullptr };

    // FormatMessage.getParameters method.
    jmethodID get_parameters_method { nullptr };
  } format_message_;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_INTERNALS_H_
