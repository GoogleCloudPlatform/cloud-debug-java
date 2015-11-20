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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JNI_METHOD_CALLER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JNI_METHOD_CALLER_H_

#include "class_metadata_reader.h"
#include "common.h"
#include "jni_utils.h"
#include "jvariant.h"
#include "method_call_result.h"

namespace devtools {
namespace cdbg {

// Helper class to actuall call methods through JNI given the signature
// and the set of arguments.
//
// There are 3 ways a method can be called:
//   1. As a static method call.
//   2. As a virtual method call.
//   3. As a non-virtual method call (equivalent to C++ syntax of "A::f()").
//
// This class is not doing any checks on validity of the arguments. If
// some arguments are missing or of a wrong type, JVM process will crash.
//
// The "Bind" function must only be called once. After that this class is
// thread safe.
class JniMethodCaller {
 public:
  JniMethodCaller() {}

  // Loads the method. Returns false if the method could not be found.
  bool Bind(
      jclass cls,
      const ClassMetadataReader::Method& metadata);

  // Invokes the method. Returns one of the following:
  //   1. Return value from the method (which can be void).
  //   2. Java exception if the method threw an exception.
  //   3. Error message if internal error occurred.
  // See method_caller.h for details about "nonvirtual".
  MethodCallResult Call(
      bool nonvirtual,
      jobject source,
      const std::vector<JVariant>& arguments);

 private:
  JVariant CallStatic(jvalue* arguments);
  JVariant CallNonVirtual(jobject source, jvalue* arguments);
  JVariant CallVirtual(jobject source, jvalue* arguments);

 private:
  // Method metadata.
  ClassMetadataReader::Method metadata_;

  // Parsed method signature.
  JMethodSignature method_signature_;

  // Target class for method invocation. Keeping the reference alive here
  // ensures that the method ("method_id_") doesn't go away.
  JavaClass cls_;

  // Method to be invoked.
  jmethodID method_id_ { nullptr };

  DISALLOW_COPY_AND_ASSIGN(JniMethodCaller);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JNI_METHOD_CALLER_H_
