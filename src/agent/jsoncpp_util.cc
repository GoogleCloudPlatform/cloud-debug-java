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

#include "jsoncpp_util.h"

#include <cstdint>

namespace devtools {
namespace cdbg {

std::string JsonCppGetString(const Json::Value& value, const char* name) {
  Json::Value attr = value.get(name, Json::Value());
  if (!attr.isString()) {
    LOG_IF(WARNING, attr.type() != Json::nullValue)
        << "Invalid type of JSON attribute " << name << ": " << attr.type();
    return std::string();
  }

  return attr.asString();
}

bool JsonCppGetBool(const Json::Value& value, const char* name, bool def) {
  Json::Value attr = value.get(name, Json::Value());
  if (!attr.isBool()) {
    LOG_IF(WARNING, attr.type() != Json::nullValue)
        << "Invalid type of JSON attribute " << name << ": " << attr.type();
    return def;
  }

  return attr.asBool();
}

int32_t JsonCppGetInt(const Json::Value& value, const char* name, int def) {
  Json::Value attr = value.get(name, Json::Value());
  if (!attr.isInt()) {
    LOG_IF(WARNING, attr.type() != Json::nullValue)
        << "Invalid type of JSON attribute " << name << ": " << attr.type();
    return def;
  }

  return attr.asInt();
}

}  // namespace cdbg
}  // namespace devtools
