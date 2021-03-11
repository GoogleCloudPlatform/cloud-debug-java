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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JSONCPP_UTIL_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JSONCPP_UTIL_H_

#include <cstdint>
#include <memory>

#include "common.h"
#include "json/json.h"

namespace devtools {
namespace cdbg {

// Gets the value of JSON string element. Returns empty string if the attribute
// does not exist or if it is not a string type.
std::string JsonCppGetString(const Json::Value& value, const char* name);

// Gets the value of JSON boolean element. Returns "def" if the attribute
// does not exist or if it is not a boolean type.
bool JsonCppGetBool(const Json::Value& value, const char* name, bool def);

// Gets the value of JSON integer element. Returns "def" if the attribute
// does not exist or if it is not an integer type.
int32_t JsonCppGetInt(const Json::Value& value, const char* name, int def);

}  // namespace cdbg
}  // namespace devtools


#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JSONCPP_UTIL_H_
