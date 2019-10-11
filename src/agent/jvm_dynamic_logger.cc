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

#include "jvm_dynamic_logger.h"

#include "jni_proxy_dynamicloghelper.h"
#include "jni_proxy_jul_logger.h"
#include "resolved_source_location.h"
#include "type_util.h"

namespace devtools {
namespace cdbg {

void JvmDynamicLogger::Initialize() {
  logger_ = nullptr;

  level_.info = JniNewGlobalRef(
      jniproxy::DynamicLogHelper()->getInfoLevel()
      .Release(ExceptionAction::LOG_AND_IGNORE)
      .get());
  level_.warning = JniNewGlobalRef(
      jniproxy::DynamicLogHelper()->getWarningLevel()
      .Release(ExceptionAction::LOG_AND_IGNORE)
      .get());
  level_.severe = JniNewGlobalRef(
      jniproxy::DynamicLogHelper()->getSevereLevel()
      .Release(ExceptionAction::LOG_AND_IGNORE)
      .get());

  logger_ = JniNewGlobalRef(
      jniproxy::DynamicLogHelper()->getLogger()
      .Release(ExceptionAction::LOG_AND_IGNORE)
      .get());
  if (logger_ == nullptr) {
    return;
  }

  // The application may define to filter out INFO or WARNING logs by default,
  // but we don't want this to apply to dynamic logging.
  jniproxy::Logger()->setLevel(logger_.get(), level_.info.get());
}


bool JvmDynamicLogger::IsAvailable() const {
  return logger_ != nullptr;
}

void JvmDynamicLogger::Log(BreakpointModel::LogLevel level,
                           const ResolvedSourceLocation& source_location,
                           const std::string& message) {
  if (!IsAvailable()) {
    LOG(WARNING) << "Dynamic logger not available";
    return;
  }

  const std::string source_class =
      TypeNameFromJObjectSignature(source_location.class_signature);

  jobject level_obj = nullptr;
  switch (level) {
    case BreakpointModel::LogLevel::INFO:
      level_obj = level_.info.get();
      break;

    case BreakpointModel::LogLevel::WARNING:
      level_obj = level_.warning.get();
      break;

    case BreakpointModel::LogLevel::ERROR:
      level_obj = level_.severe.get();
      break;
  }

  jniproxy::Logger()->logp(
      logger_.get(),
      level_obj,
      source_class,
      source_location.method_name,
      message);
}

}  // namespace cdbg
}  // namespace devtools
