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

#include "jni_bridge.h"

#include "jni_proxy_classloader.h"
#include "jni_proxy_hubclient.h"
#include "jni_proxy_hubclient_listactivebreakpointsresult.h"

namespace devtools {
namespace cdbg {

JniBridge::JniBridge(
    std::function<JniLocalRef()> hub_client_factory,
    BreakpointSerializer breakpoint_serializer,
    BreakpointDeserializer breakpoint_deserializer)
    : hub_client_factory_(hub_client_factory),
      breakpoint_serializer_(breakpoint_serializer),
      breakpoint_deserializer_(breakpoint_deserializer) {
}


JniBridge::~JniBridge() {
}


bool JniBridge::Bind(ClassPathLookup* class_path_lookup) {
  absl::MutexLock lock(&mu_);

  if (shutdown_) {
    LOG(ERROR) << "Bind not allowed after Shutdown";
    return false;
  }

  jni_hub_ = JniNewGlobalRef(hub_client_factory_().get());
  if (jni_hub_ == nullptr) {
    LOG(ERROR) << "Failed to instantiate HubClient Java class";
    return false;
  }

  return true;
}


void JniBridge::EnqueueBreakpointUpdate(
      std::unique_ptr<BreakpointModel> breakpoint) {
  std::unique_ptr<SerializedBreakpoint> serialized_breakpoint(
      new SerializedBreakpoint(breakpoint_serializer_(*breakpoint)));

  {
    absl::MutexLock lock(&mu_);
    transmit_queue_.enqueue(std::move(serialized_breakpoint));
  }
}

bool JniBridge::RegisterDebuggee(bool* is_enabled,
                                 const DebuggeeLabels& debuggee_labels) {
  *is_enabled = false;

  JniLocalRef java_labels = debuggee_labels.Get();

  if (!java_labels) {
    LOG(ERROR) << "Failed to create the Debuggee labels Java map.";
    return false;
  }

  auto rc = jniproxy::HubClient()->registerDebuggee(jni_hub_.get(),
                                                    java_labels.get());

  if (rc.HasException()) {
    // The Java code logs important errors.
    return false;  // Registration failed (registerDebuggee threw exception).
  }

  *is_enabled = rc.GetData();

  return true;
}

Bridge::HangingGetResult JniBridge::ListActiveBreakpoints(
    std::vector<std::unique_ptr<BreakpointModel>>* breakpoints) {
  breakpoints->clear();

  ExceptionOr<JniLocalRef> rc =
      jniproxy::HubClient()->listActiveBreakpoints(jni_hub_.get());
  if (rc.HasException()) {
    return HangingGetResult::FAIL;
  }

  ExceptionOr<jboolean> timeout =
      jniproxy::HubClient_ListActiveBreakpointsResult()->getIsTimeout(
          rc.GetData().get());
  if (timeout.HasException()) {
    return HangingGetResult::FAIL;
  }

  if (timeout.GetData()) {
    return HangingGetResult::TIMEOUT;
  }

  ExceptionOr<std::string> format =
      jniproxy::HubClient_ListActiveBreakpointsResult()->getFormat(
          rc.GetData().get());
  if (format.HasException()) {
    return HangingGetResult::FAIL;
  }

  ExceptionOr<JniLocalRef> blobs =
      jniproxy::HubClient_ListActiveBreakpointsResult()->getActiveBreakpoints(
          rc.GetData().get());
  if (blobs.HasException()) {
    return HangingGetResult::FAIL;
  }

  auto array = static_cast<jobjectArray>(blobs.GetData().get());

  const jsize size = jni()->GetArrayLength(array);
  breakpoints->reserve(size);

  SerializedBreakpoint serialized_breakpoint;
  serialized_breakpoint.format = format.GetData();

  for (int i = 0; i < size; ++i) {
    // If we fail to deserialize a breakpoint, we just skip it and move on.
    // There is no point in failing everything. Any errors encountered here
    // can't be surfaced to a user.
    serialized_breakpoint.data = JniToNativeBlob(
        JniLocalRef(jni()->GetObjectArrayElement(array, i)).get());

    std::unique_ptr<BreakpointModel> model =
        breakpoint_deserializer_(serialized_breakpoint);
    if (model == nullptr) {
      LOG(ERROR) << "Breakpoint could not be deserialized";
      continue;
    }

    if (model->id.empty()) {
      LOG(ERROR) << "Missing ID in breakpoint definition";
      continue;
    }

    breakpoints->push_back(std::move(model));
  }

  return HangingGetResult::SUCCESS;
}


void JniBridge::TransmitBreakpointUpdates() {
  mu_.Lock();

  std::unique_ptr<TransmitQueue<SerializedBreakpoint>::Item> item;
  while ((item = transmit_queue_.pop()) != nullptr) {
    mu_.Unlock();

    ExceptionOr<std::nullptr_t> no_retry =
        jniproxy::HubClient()->transmitBreakpointUpdate(
            jni_hub_.get(),
            item->message->format,
            item->message->id,
            item->message->data);

    mu_.Lock();

    // If the failure occurred due to an application  error, do not re-queue
    // the offending message, discard it, and continue with the next message.
    // Otherwise, put the offending message back to the end of the
    // queue and exit. Don't continue for two reasons:
    // 1. If there was some kind of timeout, the failed transmission already
    //    took too much time, so don't make it worse by trying to send more
    //    messages that are likely to fail for the same reason.
    // 2. Prevent infinite loop when a message can't be sent over and over
    //    again (it won't be inifinite because a message will be eventually
    //    discarded as poisonous, but it will still take a lot of time).
    if (!no_retry.HasException())
      continue;

    transmit_queue_.enqueue(std::move(item));
    break;
  }

  mu_.Unlock();
}


bool JniBridge::HasPendingMessages() const {
  absl::MutexLock lock(&mu_);
  return !transmit_queue_.empty();
}

bool JniBridge::RegisterBreakpointCanary(const std::string& breakpoint_id) {
  auto rc = jniproxy::HubClient()->registerBreakpointCanary(
      jni_hub_.get(),
      breakpoint_id);
  if (rc.HasException()) {
    rc.LogException();
    return false;
  }

  return true;
}

bool JniBridge::ApproveBreakpointCanary(const std::string& breakpoint_id) {
  auto rc = jniproxy::HubClient()->approveBreakpointCanary(
      jni_hub_.get(),
      breakpoint_id);
  if (rc.HasException()) {
    rc.LogException();
    return false;
  }

  return true;
}

bool JniBridge::IsEnabled(bool* is_enabled) {
  auto rc = jniproxy::HubClient()->isEnabled(jni_hub_.get());
  if (rc.HasException()) {
    *is_enabled = false;
    return false;
  }

  *is_enabled = rc.GetData();
  return true;
}


void JniBridge::Shutdown() {
  absl::MutexLock lock(&mu_);

  if (jni_hub_ != nullptr) {
    jniproxy::HubClient()->shutdown(jni_hub_.get());
  }

  shutdown_ = true;
}

}  // namespace cdbg
}  // namespace devtools
