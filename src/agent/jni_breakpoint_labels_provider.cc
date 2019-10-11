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

#include "jni_breakpoint_labels_provider.h"

#include "jni_proxy_breakpointlabelsprovider.h"

namespace devtools {
namespace cdbg {

JniBreakpointLabelsProvider::JniBreakpointLabelsProvider(
    std::function<JniLocalRef()> factory)
    : factory_(factory) {
}


void JniBreakpointLabelsProvider::Collect() {
  JniLocalRef labels = factory_();
  if (labels == nullptr) {
    LOG(WARNING) << "Breakpoint labels provider not available";
    return;
  }

  DCHECK(jni()->IsInstanceOf(
      labels.get(),
      jniproxy::BreakpointLabelsProvider()->GetClass()));

  labels_ = JniNewGlobalRef(labels.get());
}

std::map<std::string, std::string> JniBreakpointLabelsProvider::Format() {
  if (labels_ == nullptr) {
    return {};  // Breakpoint labels not available.
  }

  auto rc = jniproxy::BreakpointLabelsProvider()->format(labels_.get());
  if (rc.HasException()) {
    rc.LogException();
    return {};  // Failed to obtain breakpoint labels.
  }

  std::vector<std::string> labels_array =
      JniToNativeStringArray(rc.GetData().get());

  // "labels_array" serializes map into a flat array. Every even entry is a key
  // and every odd entry is value.
  std::map<std::string, std::string> labels;
  for (int i = 0; i < labels_array.size() / 2; ++i) {
    labels[labels_array[i * 2]] = labels_array[i * 2 + 1];
  }

  return labels;
}

}  // namespace cdbg
}  // namespace devtools
