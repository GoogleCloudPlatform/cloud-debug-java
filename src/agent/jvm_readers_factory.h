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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_READERS_FACTORY_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_READERS_FACTORY_H_

#include <map>
#include <memory>

#include "common.h"
#include "instance_field_reader.h"
#include "jni_utils.h"
#include "jvm_evaluators.h"
#include "local_variable_reader.h"
#include "readers_factory.h"
#include "static_field_reader.h"

namespace devtools {
namespace cdbg {

// JVMTI based implementation of ReadersFactory. This class should
// only be used within a single JVMTI callback and should not be kept around
// longer than that. This class also makes no guarantees that Java method won't
// get unloaded during its lifetime. This is something that the caller should
// take care of.
class JvmReadersFactory : public ReadersFactory {
 public:
  JvmReadersFactory(
      JvmEvaluators* evaluators,
      jmethodID method,
      jlocation location);

  std::string GetEvaluationPointClassName() override;

  JniLocalRef FindClassByName(const std::string& class_name,
                              FormatMessageModel* error_message) override;

  bool IsAssignable(const std::string& from_signature,
                    const std::string& to_signature) override;

  std::unique_ptr<LocalVariableReader> CreateLocalVariableReader(
      const std::string& variable_name,
      FormatMessageModel* error_message) override;

  std::unique_ptr<LocalVariableReader> CreateLocalInstanceReader() override;

  std::unique_ptr<InstanceFieldReader> CreateInstanceFieldReader(
      const std::string& class_signature, const std::string& field_name,
      FormatMessageModel* error_message) override;

  std::unique_ptr<StaticFieldReader> CreateStaticFieldReader(
      const std::string& field_name,
      FormatMessageModel* error_message) override;

  std::unique_ptr<StaticFieldReader> CreateStaticFieldReader(
      const std::string& class_name, const std::string& field_name,
      FormatMessageModel* error_message) override;

  std::vector<ClassMetadataReader::Method> FindLocalInstanceMethods(
      const std::string& method_name) override;

  bool FindInstanceMethods(const std::string& class_signature,
                           const std::string& method_name,
                           std::vector<ClassMetadataReader::Method>* methods,
                           FormatMessageModel* error_message) override;

  std::vector<ClassMetadataReader::Method> FindStaticMethods(
      const std::string& method_name) override;

  bool FindStaticMethods(const std::string& class_name,
                         const std::string& method_name,
                         std::vector<ClassMetadataReader::Method>* methods,
                         FormatMessageModel* error_message) override;

  std::unique_ptr<ArrayReader> CreateArrayReader(
      const JSignature& array_signature) override;

 private:
  // Common code for the two public versions of "CreateStaticFieldReader".
  std::unique_ptr<StaticFieldReader> CreateStaticFieldReader(
      jclass cls, const std::string& field_name);

  // Common code for the public versions of "FindXXXMethods".
  std::vector<ClassMetadataReader::Method> FindClassMethods(
      jclass cls, bool is_static, const std::string& method_name);

 private:
  // Evaluation classes bundled together. Not owned by this class.
  JvmEvaluators* const evaluators_;

  // Method in which the expression is going to be evaluated.
  const jmethodID method_;

  // Location within the method in which expression is going to take its local
  // variables.
  const jlocation location_;

  DISALLOW_COPY_AND_ASSIGN(JvmReadersFactory);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_READERS_FACTORY_H_


