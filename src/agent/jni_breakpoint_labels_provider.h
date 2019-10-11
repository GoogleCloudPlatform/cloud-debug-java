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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JNI_BREAKPOINT_LABELS_PROVIDER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JNI_BREAKPOINT_LABELS_PROVIDER_H_

#include "breakpoint_labels_provider.h"
#include "common.h"
#include "jni_utils.h"

namespace devtools {
namespace cdbg {

// Invokes com.google.devtools.cdbg.BreakpointLabelsProvider to expose
// breakpoint labels to the debugger agent.
class JniBreakpointLabelsProvider : public BreakpointLabelsProvider {
 public:
  // The "factory" callback creates a Java object implementing the
  // com.google.devtools.cdbg.BreakpointLabelsProvider interface.
  explicit JniBreakpointLabelsProvider(
      std::function<JniLocalRef()> factory);

  void Collect() override;

  std::map<std::string, std::string> Format() override;

 private:
  // Callback that creates a Java object implementing the
  // com.google.devtools.cdbg.BreakpointLabelsProvider interface.
  std::function<JniLocalRef()> factory_;

  // Global reference to Java class implementing the
  // com.google.devtools.cdbg.BreakpointLabelsProvider interface. It collects
  // all the necessary data in constructor. Then we keep the object to until
  // "Format" is called.
  JniGlobalRef labels_;

  DISALLOW_COPY_AND_ASSIGN(JniBreakpointLabelsProvider);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JNI_BREAKPOINT_LABELS_PROVIDER_H_
