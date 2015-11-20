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

#include "config.h"

namespace devtools {
namespace cdbg {

static bool MatchMethodRule(
    const Config::Method& rule,
    const string& method_name,
    const string& method_signature) {
  // Empty method name means "match all".
  if (!rule.name.empty() &&
      (rule.name != method_name)) {
    return false;
  }

  // Empty method signature means "match all".
  if (!rule.signature.empty() &&
      (rule.signature != method_signature)) {
    return false;
  }

  return true;
}


Config::Config() {
}


Config::~Config() {
}


const Config::Method& Config::GetMethodRule(
    const string& method_cls_signature,
    const string& object_cls_signature,
    const string& method_name,
    const string& method_signature) const {
  std::map<string, std::vector<Config::Method>>::const_iterator it;

  it = classes_.find(object_cls_signature);
  if (it != classes_.end()) {
    for (const Config::Method& rule : it->second) {
      if (MatchMethodRule(rule, method_name, method_signature)) {
        return rule;
      }
    }
  }

  // Compare class that defined the method with the class of the object.
  if (method_cls_signature != object_cls_signature) {
    it = classes_.find(method_cls_signature);
    if (it != classes_.end()) {
      for (const Config::Method& rule : it->second) {
        if (rule.applies_to_derived_classes &&
            MatchMethodRule(rule, method_name, method_signature)) {
          return rule;
        }
      }
    }
  }

  return default_rule_;
}


Config::Builder::Builder() {
}


Config::Builder& Config::Builder::SetClassConfig(
      const string& class_signature,
      std::vector<Config::Method> rules) {
  config_->classes_[class_signature] = std::move(rules);
  return *this;
}


Config::Builder& Config::Builder::AddMethodRule(
    const string& class_signature,
    Config::Method rule) {
  auto& methods = config_->classes_[class_signature];
  methods.insert(methods.begin(), std::move(rule));

  return *this;
}


Config::Builder& Config::Builder::SetDefaultMethodRule(Config::Method rule) {
  config_->default_rule_ = std::move(rule);
  return *this;
}


Config::Builder& Config::Builder::SetQuotas(
    const Config::Method::CallQuota& expression_method_call_quota,
    const Config::Method::CallQuota& pretty_printers_method_call_quota,
    const Config::Method::CallQuota& dynamic_log_method_call_quota) {
  config_->expression_method_call_quota_ = expression_method_call_quota;
  config_->pretty_printers_method_call_quota_ =
      pretty_printers_method_call_quota;
  config_->dynamic_log_method_call_quota_ = dynamic_log_method_call_quota;

  return *this;
}

}  // namespace cdbg
}  // namespace devtools

