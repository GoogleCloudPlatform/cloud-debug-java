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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_DYNAMIC_LOGGER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_DYNAMIC_LOGGER_H_

#include "common.h"
#include "model.h"

namespace devtools {
namespace cdbg {

struct ResolvedSourceLocation;

// Writes dynamic log entries to application log.
class DynamicLogger {
 public:
  virtual ~DynamicLogger() { }

  // Returns true if dynamic logger initialization has been completed
  // successfully.
  virtual bool IsAvailable() const = 0;

  // Synchronously writes a log entry to application log. Ignores any failures.
  virtual void Log(BreakpointModel::LogLevel level,
                   const ResolvedSourceLocation& source_location,
                   const std::string& message) = 0;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_DYNAMIC_LOGGER_H_
