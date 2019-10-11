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

#include "jni_user_id_provider.h"

#include "jni_proxy_useridprovider.h"

namespace devtools {
namespace cdbg {

JniUserIdProvider::JniUserIdProvider(std::function<JniLocalRef()> factory)
    : factory_(std::move(factory)) {
}


void JniUserIdProvider::Collect() {
  JniLocalRef provider = factory_();
  if (provider == nullptr) {
    LOG(WARNING) << "End user identity provider not available";
    return;
  }

  DCHECK(jni()->IsInstanceOf(
      provider.get(),
      jniproxy::UserIdProvider()->GetClass()));

  provider_ = JniNewGlobalRef(provider.get());
}

bool JniUserIdProvider::Format(std::string* kind, std::string* id) {
  if (provider_ == nullptr) {
    return false;  // User id not available.
  }

  auto rc = jniproxy::UserIdProvider()->format(provider_.get());
  if (rc.HasException()) {
    rc.LogException();
    return false;  // Failed to obtain user id.
  }

  std::vector<std::string> result = JniToNativeStringArray(rc.GetData().get());
  if (result.size() != 2) {
    return false;
  }


  *kind = result[0];
  *id = result[1];
  return true;
}

}  // namespace cdbg
}  // namespace devtools
