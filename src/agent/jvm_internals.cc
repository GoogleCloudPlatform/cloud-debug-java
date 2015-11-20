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

#include "jvm_internals.h"

#include <fstream>  // NOLINT
#include "jvariant.h"
#include "model_util.h"
#include "resolved_source_location.h"
#include "stopwatch.h"

DECLARE_bool(enable_transformer);

namespace devtools {
namespace cdbg {

// Name of the ClassLoader class that we are loading from
// "cdbg_java_agent_internals_loader.class".
static constexpr char kClassLoaderClassPath[] =
    "com/google/devtools/cdbg/debuglets/java/InternalsClassLoader";

static constexpr char kClassPathLookupClassName[] =
    "com.google.devtools.cdbg.debuglets.java.ClassPathLookup";

static constexpr char kResolvedSourceLocationClassName[] =
    "com.google.devtools.cdbg.debuglets.java.ResolvedSourceLocation";

static constexpr char kFormatMessageClassName[] =
    "com.google.devtools.cdbg.debuglets.java.FormatMessage";

static constexpr char kFormatMessageClassSignature[] =
    "Lcom/google/devtools/cdbg/debuglets/java/FormatMessage;";

// Loads the content of "cdbg_java_agent_internals_loader.class" file expected
// to be present in agentdir. Returns empty buffer if the file is not there.
static std::vector<jbyte> GetClassLoaderBinary(const string& agentdir) {
  const string classloader_path =
      agentdir + "/cdbg_java_agent_internals_loader.class";

  LOG(INFO) << "Loading class loader from " << classloader_path;

  // Open the file:
  std::ifstream file(classloader_path, std::ios::binary);
  if (!file.is_open()) {
    LOG(ERROR) << "Class loader file not found: " << classloader_path;
    return std::vector<jbyte>();
  }

  // Read the data
  return std::vector<jbyte>(
      std::istreambuf_iterator<char>(file),
      std::istreambuf_iterator<char>());
}


bool JvmInternals::LoadInternals(const string& agentdir) {
  if (!LoadClassLoader(agentdir)) {
    return false;
  }

  if (!LoadClasses()) {
    return false;
  }

  return true;
}


bool JvmInternals::LoadInternalsWithClassLoader(
    jobject internals_class_loader) {
  DCHECK(class_loader_obj_ == nullptr);

  if (internals_class_loader == nullptr) {
    LOG(ERROR) << "internals_class_loader == nullptr";
    return false;
  }

  class_loader_obj_ = jni()->NewGlobalRef(internals_class_loader);

  if (!LoadClasses()) {
    return false;
  }

  return true;
}


bool JvmInternals::CreateClassPathLookupInstance(
    bool use_default_class_path,
    jobject extra_class_path,
    const string& config_file_path) {
  Stopwatch stopwatch;

  JniLocalRef instance_local_ref(jni()->NewObject(
      class_path_lookup_.cls.get(),
      class_path_lookup_.constructor,
      use_default_class_path,
      extra_class_path,
      JniToJavaString(config_file_path).get()));

  if (!JniCheckNoException("new ClassPathLookup(...)")) {
    return false;
  }

  if (instance_local_ref == nullptr) {
    return false;
  }

  class_path_lookup_.instance =
      jni()->NewGlobalRef(instance_local_ref.get());

  LOG(INFO) << "ClassPathLookup constructor time: "
            << stopwatch.GetElapsedMicros() << " microseconds";

  return true;
}


void JvmInternals::ReleaseRefs() {
  if (class_loader_obj_ != nullptr) {
    jni()->DeleteGlobalRef(class_loader_obj_);
    class_loader_obj_ = nullptr;
  }

  if (class_path_lookup_.instance != nullptr) {
    jni()->DeleteGlobalRef(class_path_lookup_.instance);
    class_path_lookup_.instance = nullptr;
  }

  class_path_lookup_.cls.ReleaseRef();
  resolved_source_location_.cls.ReleaseRef();
  format_message_.cls.ReleaseRef();

  class_path_lookup_.constructor = nullptr;
  class_path_lookup_.resolve_source_location_method = nullptr;
  class_path_lookup_.find_classes_by_name_method = nullptr;
  class_path_lookup_.compute_debuggee_uniquifier_method = nullptr;
  class_path_lookup_.read_application_resource_method = nullptr;
  resolved_source_location_.get_class_signature_method = nullptr;
  resolved_source_location_.get_method_name_method = nullptr;
  resolved_source_location_.get_adjusted_line_number_method = nullptr;
  format_message_.get_format_method = nullptr;
  format_message_.get_parameters_method = nullptr;
}


void JvmInternals::ResolveSourceLocation(
    const string& source_path,
    int line_number,
    ResolvedSourceLocation* location) {
  *location = ResolvedSourceLocation();

  // Initialize error to "internal error", so that this function can just
  // return if something goes wrong.
  location->error_message = INTERNAL_ERROR_MESSAGE;

  if (!class_path_lookup_.instance) {
    LOG(ERROR) << "JvmInternals not initialized";
    return;
  }

  // rsl = ClassPathLookup.resolveSourceLocation(source_path, line_number)
  JniLocalRef location_local_ref(jni()->CallObjectMethod(
      class_path_lookup_.instance,
      class_path_lookup_.resolve_source_location_method,
      JniToJavaString(source_path).get(),
      static_cast<jint>(line_number)));

  if (!JniCheckNoException("ClassPathLookup.resolveSourceLocation")) {
    return;
  }

  if (location_local_ref == nullptr) {
    return;
  }

  JniLocalRef jobj;

  // rsl.getErrorMessage()
  jobj.reset(jni()->CallObjectMethod(
      location_local_ref.get(),
      resolved_source_location_.get_error_message_method));

  if (!JniCheckNoException("ResolvedSourceLocation.getErrorMessage")) {
    return;
  }

  if (jobj != nullptr) {
    location->error_message = ConvertFormatMessage(jobj.get());

    LOG(INFO) << "Failed to resolve source location, "
                 "source path: " << source_path
              << ", line number: " << line_number
              << ", error message: " << location->error_message;
    return;
  }

  // rsl.getClassSignature()
  jobj.reset(jni()->CallObjectMethod(
      location_local_ref.get(),
      resolved_source_location_.get_class_signature_method));

  if (!JniCheckNoException("ResolvedSourceLocation.getClassSignature")) {
    return;
  }

  location->class_signature = JniToNativeString(jobj.get());
  if (location->class_signature.empty()) {
    LOG(ERROR) << "Empty class signature returned";
    return;
  }

  // rsl.getMethodName()
  jobj.reset(jni()->CallObjectMethod(
      location_local_ref.get(),
      resolved_source_location_.get_method_name_method));

  if (!JniCheckNoException("ResolvedSourceLocation.getMethodName")) {
    return;
  }

  location->method_name = JniToNativeString(jobj.get());
  if (location->method_name.empty()) {
    LOG(ERROR) << "Empty method name returned";
    return;
  }

  // rsl.getAdjustedLineNumber()
  location->adjusted_line_number =
      jni()->CallIntMethod(
          location_local_ref.get(),
          resolved_source_location_.get_adjusted_line_number_method);
  if (location->adjusted_line_number <= 0) {
    LOG(ERROR) << "Invalid adjusted line number returned";
  }

  location->error_message = FormatMessageModel();
}


std::vector<string> JvmInternals::FindClassesByName(const string& class_name) {
  if (!class_path_lookup_.instance) {
    LOG(ERROR) << "JvmInternals not initialized";
    return {};
  }

  JniLocalRef signatures_array(jni()->CallObjectMethod(
      class_path_lookup_.instance,
      class_path_lookup_.find_classes_by_name_method,
      JniToJavaString(class_name).get()));

  if (!JniCheckNoException("ClassPathLookup.findClassesByName")) {
    return {};
  }

  return JniToNativeStringArray(signatures_array.get());;
}


string JvmInternals::ComputeDebuggeeUniquifier(const string& iv) {
  Stopwatch stopwatch;

  if (!class_path_lookup_.instance) {
    LOG(ERROR) << "JvmInternals not initialized";
    return string();
  }

  JniLocalRef uniquifier_jstr(jni()->CallObjectMethod(
      class_path_lookup_.instance,
      class_path_lookup_.compute_debuggee_uniquifier_method,
      JniToJavaString(iv).get()));

  if (!JniCheckNoException("ClassPathLookup.computeDebuggeeUniquifier")) {
    return string();
  }

  LOG(INFO) << "ComputeDebuggeeUniquifier time: "
            << stopwatch.GetElapsedMicros() << " microseconds";

  return JniToNativeString(uniquifier_jstr.get());
}


std::set<string> JvmInternals::ReadApplicationResource(
    const string& resource_path) {
  if (!class_path_lookup_.instance) {
    LOG(ERROR) << "JvmInternals not initialized";
    return std::set<string>();
  }

  JniLocalRef resources_jarray(jni()->CallObjectMethod(
      class_path_lookup_.instance,
      class_path_lookup_.read_application_resource_method,
      JniToJavaString(resource_path).get()));

  if (!JniCheckNoException("ClassPathLookup.readApplicationResource")) {
    return std::set<string>();
  }

  std::vector<string> resources =
      JniToNativeStringArray(resources_jarray.get());

  return std::set<string>(resources.begin(), resources.end());
}


bool JvmInternals::LoadClassLoader(const string& agentdir) {
  DCHECK(class_loader_obj_ == nullptr);

  // Load the class loader binary.
  std::vector<jbyte> class_loader = GetClassLoaderBinary(agentdir);
  if (class_loader.empty()) {
    LOG(ERROR) << "InternalsClassLoader not available";
    return false;
  }

  // Load the class in JVM.
  JavaClass class_loader_cls;
  if (!class_loader_cls.DefineClass(
        kClassLoaderClassPath,
        class_loader)) {
    LOG(ERROR) << "InternalsClassLoader could not be loaded into JVM";
    return false;
  }

  jmethodID constructor_method =
      class_loader_cls.GetConstructor("(Ljava/lang/String;)V");
  if (constructor_method == nullptr) {
    LOG(ERROR) << "Couldn't find constructor of InternalsClassLoader class";
    class_loader_cls.ReleaseRef();
    return false;
  }

  // Create class loader instance exposing classes from
  // "cdbg_java_agent_internals.jar".
  const string internals_jar_path =
      agentdir + "/cdbg_java_agent_internals.jar";
  LOG(INFO) << "Loading internals from " << internals_jar_path;
  JniLocalRef jstr_internals_path(JniToJavaString(internals_jar_path));

  JniLocalRef class_loader_obj_local_ref(jni()->NewObject(
      class_loader_cls.get(),
      constructor_method,
      jstr_internals_path.get()));

  class_loader_cls.ReleaseRef();

  if (!JniCheckNoException("NewObject(class loader)")) {
    return false;
  }

  if (class_loader_obj_local_ref == nullptr) {
    LOG(ERROR) << "New instance of InternalsClassLoader could not be created";
    return false;
  }

  class_loader_obj_ = jni()->NewGlobalRef(class_loader_obj_local_ref.get());

  return true;
}


bool JvmInternals::LoadClasses() {
  //
  // com.google.devtools.cdbg.debuglets.java.ClassPathLookup class
  //

  if (!class_path_lookup_.cls.LoadWithClassLoader(
        class_loader_obj_,
        kClassPathLookupClassName)) {
    return false;
  }

  class_path_lookup_.constructor =
      class_path_lookup_.cls.GetInstanceMethod(
          "<init>",
          "(Z[Ljava/lang/String;Ljava/lang/String;)V");
  if (class_path_lookup_.constructor == nullptr) {
    return false;
  }

  constexpr char resolve_source_location_signature[] =
    "(Ljava/lang/String;I)"
    "Lcom/google/devtools/cdbg/debuglets/java/ResolvedSourceLocation;";

  class_path_lookup_.resolve_source_location_method =
    class_path_lookup_.cls.GetInstanceMethod(
        "resolveSourceLocation",
        resolve_source_location_signature);
  if (class_path_lookup_.resolve_source_location_method == nullptr) {
    return false;
  }

  class_path_lookup_.find_classes_by_name_method =
    class_path_lookup_.cls.GetInstanceMethod(
        "findClassesByName",
        "(Ljava/lang/String;)[Ljava/lang/String;");
  if (class_path_lookup_.find_classes_by_name_method == nullptr) {
    return false;
  }

  class_path_lookup_.compute_debuggee_uniquifier_method =
    class_path_lookup_.cls.GetInstanceMethod(
        "computeDebuggeeUniquifier",
        "(Ljava/lang/String;)Ljava/lang/String;");
  if (class_path_lookup_.compute_debuggee_uniquifier_method == nullptr) {
    return false;
  }

  class_path_lookup_.read_application_resource_method =
    class_path_lookup_.cls.GetInstanceMethod(
        "readApplicationResource",
        "(Ljava/lang/String;)[Ljava/lang/String;");
  if (class_path_lookup_.read_application_resource_method == nullptr) {
    return false;
  }

  //
  // com.google.devtools.cdbg.debuglets.java.ResolvedSourceLocation
  //

  if (!resolved_source_location_.cls.LoadWithClassLoader(
        class_loader_obj_,
        kResolvedSourceLocationClassName)) {
    return false;
  }

  string get_error_message_signature;
  get_error_message_signature = "()";
  get_error_message_signature += kFormatMessageClassSignature;

  resolved_source_location_.get_error_message_method =
    resolved_source_location_.cls.GetInstanceMethod(
        "getErrorMessage",
        get_error_message_signature.c_str());
  if (resolved_source_location_.get_error_message_method == nullptr) {
    return false;
  }

  resolved_source_location_.get_class_signature_method =
    resolved_source_location_.cls.GetInstanceMethod(
        "getClassSignature",
        "()Ljava/lang/String;");
  if (resolved_source_location_.get_class_signature_method == nullptr) {
    return false;
  }

  resolved_source_location_.get_method_name_method =
    resolved_source_location_.cls.GetInstanceMethod(
        "getMethodName",
        "()Ljava/lang/String;");
  if (resolved_source_location_.get_method_name_method == nullptr) {
    return false;
  }

  resolved_source_location_.get_adjusted_line_number_method =
    resolved_source_location_.cls.GetInstanceMethod(
        "getAdjustedLineNumber",
        "()I");
  if (resolved_source_location_.get_adjusted_line_number_method == nullptr) {
    return false;
  }


  //
  // com.google.devtools.cdbg.debuglets.java.FormatMessage
  //

  if (!format_message_.cls.LoadWithClassLoader(
        class_loader_obj_,
        kFormatMessageClassName)) {
    return false;
  }

  format_message_.get_format_method =
    format_message_.cls.GetInstanceMethod(
        "getFormat",
        "()Ljava/lang/String;");
  if (format_message_.get_format_method == nullptr) {
    return false;
  }

  format_message_.get_parameters_method =
    format_message_.cls.GetInstanceMethod(
        "getParameters",
        "()[Ljava/lang/String;");
  if (format_message_.get_parameters_method == nullptr) {
    return false;
  }

  return true;
}


FormatMessageModel JvmInternals::ConvertFormatMessage(
    jobject obj_format_message) {
  if (obj_format_message == nullptr) {
    return INTERNAL_ERROR_MESSAGE;
  }

  JniLocalRef jobj;

  // FormatMessage.getFormat
  jobj.reset(jni()->CallObjectMethod(
      obj_format_message,
      format_message_.get_format_method));
  if (!JniCheckNoException("FormatMessage.getFormat")) {
    return INTERNAL_ERROR_MESSAGE;
  }

  string format = JniToNativeString(jobj.get());

  if (format.empty()) {
    LOG(ERROR) << "Empty error message format returned in FormatMessage";
    return INTERNAL_ERROR_MESSAGE;
  }

  // FormatMessage.getParameters
  jobj.reset(jni()->CallObjectMethod(
      obj_format_message,
      format_message_.get_parameters_method));
  if (!JniCheckNoException("FormatMessage.getParameters")) {
    return INTERNAL_ERROR_MESSAGE;
  }

  std::vector<string> parameters;
  if (jobj != nullptr) {
    const jsize size = jni()->GetArrayLength(static_cast<jarray>(jobj.get()));

    for (int i = 0; i < size; ++i) {
      JniLocalRef jstr(jni()->GetObjectArrayElement(
          static_cast<jobjectArray>(jobj.get()), i));

      parameters.push_back(JniToNativeString(jstr.get()));
    }
  }

  return { std::move(format), std::move(parameters) };
}

}  // namespace cdbg
}  // namespace devtools
