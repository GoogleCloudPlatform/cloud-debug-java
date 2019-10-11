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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_CLASS_PATH_LOOKUP_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_CLASS_PATH_LOOKUP_H_

#include <memory>
#include <set>

#include "common.h"
#include "jni_utils.h"
#include "jvariant.h"

namespace devtools {
namespace cdbg {

struct ResolvedSourceLocation;

// Proxy for ClassPathLookup class implemented in cdbg_java_agent_internals.jar.
class ClassPathLookup {
 public:
  virtual ~ClassPathLookup() { }

  // Searches for a statement in a method corresponding to the specified source
  // line in the available Java classes. If source location could not be
  // resolved, "location->error_message" will be initialized.
  //
  // The returned "ResolvedSourceLocation" may have a different line number
  // if "line_number" points to a multi-line statement. The function makes no
  // assumption about which classes have already been loaded and which
  // haven't. This code has zero impact on the running application.
  // Specifically no new application classes are being loaded.
  virtual void ResolveSourceLocation(const std::string& source_path,
                                     int line_number,
                                     ResolvedSourceLocation* location) = 0;

  // Gets the list of class signatures for the specified class name.
  // Examples:
  //   1. "com.prod.MyClass" --> [ "Lcom/prod/MyClass;" ]
  //   2. "MyClass" --> [ "Lcom/prod1/MyClass;", "Lcom/prod2/MyClass;" ]
  //   3. "My$Inner" --> [ "Lcom/prod/My$Inner;" ]
  virtual std::vector<std::string> FindClassesByName(
      const std::string& class_name) = 0;

  // Computes a hash code of all the binaries in the class path. Returns empty
  // string in case of an error.
  virtual std::string ComputeDebuggeeUniquifier(const std::string& iv) = 0;

  // Searches for the application resource files that match "resource_path",
  // reads them as UTF-8 encoded string and returns the string. If no matches
  // are found, returns an empty array.
  //
  // A resource with the same name may appear in multiple directories
  // referenced in class path or in multiple .jar files. While it is not too
  // interesting for .class resource files, this is an important scenario for
  // source context file that may show up in every .jar file.
  virtual std::set<std::string> ReadApplicationResource(
      const std::string& resource_path) = 0;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_CLASS_PATH_LOOKUP_H_
