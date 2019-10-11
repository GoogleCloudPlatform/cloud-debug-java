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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_MODEL_JSON_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_MODEL_JSON_H_

#include <memory>
#include <vector>

#include "common.h"
#include "model.h"

namespace Json {
class Value;
}

namespace devtools {
namespace cdbg {

//
// Serialize BreakpointModel structure to JSON format.
//

SerializedBreakpoint BreakpointToJson(const BreakpointModel& model);

SerializedBreakpoint BreakpointToPrettyJson(const BreakpointModel& model);

//
// Deserialize BreakpointModel structure from JSON format.
//

std::unique_ptr<BreakpointModel> BreakpointFromJson(
    const SerializedBreakpoint& serialized_breakpoint);

std::unique_ptr<BreakpointModel> BreakpointFromJsonString(
    const std::string& json_string);

std::unique_ptr<BreakpointModel> BreakpointFromJsonValue(
    const Json::Value& json_value);

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_MODEL_JSON_H_


