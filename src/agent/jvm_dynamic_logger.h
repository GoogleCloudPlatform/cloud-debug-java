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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_DYNAMIC_LOGGER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_DYNAMIC_LOGGER_H_

#include "common.h"
#include "dynamic_logger.h"
#include "jni_utils.h"

namespace devtools {
namespace cdbg {

class JvmDynamicLogger : public DynamicLogger {
 public:
  // Loads the relevant Java classes and creates the shared "Logger" instance.
  void Initialize();

  bool IsAvailable() const override;

  void Log(BreakpointModel::LogLevel level,
           const ResolvedSourceLocation& source_location,
           const std::string& message) override;

 private:
  // Instance of "java.util.logging.Logger" class.
  JniGlobalRef logger_;

  // "java.util.logging.Level" class.
  struct {
    // Global reference to "Level.INFO" static field.
    JniGlobalRef info;

    // Global reference to "Level.WARNING" static field.
    JniGlobalRef warning;

    // Global reference to "Level.SEVERE" static field.
    JniGlobalRef severe;
  } level_;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVM_DYNAMIC_LOGGER_H_
