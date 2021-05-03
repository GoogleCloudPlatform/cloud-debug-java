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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JNI_BRIDGE_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JNI_BRIDGE_H_

#include <functional>
#include <memory>

#include "nullable.h"
#include "bridge.h"
#include "common.h"
#include "debuggee_labels.h"
#include "jni_utils.h"
#include "mutex.h"
#include "transmit_queue.h"

namespace devtools {
namespace cdbg {

// Implementation of "Bridge" interface to communicate with Java class in
// the debugged process.
class JniBridge : public Bridge {
 public:
  // Routines to serialize and deserialize breakpoint between BreakpointModel
  // and the data format we send to Java code.
  typedef std::function<
      SerializedBreakpoint(const BreakpointModel&)
      > BreakpointSerializer;
  typedef std::function<
      std::unique_ptr<BreakpointModel>(const SerializedBreakpoint&)
      > BreakpointDeserializer;

  JniBridge(
      std::function<JniLocalRef()> hub_client_factory,
      BreakpointSerializer breakpoint_serializer,
      BreakpointDeserializer breakpoint_deserializer);

  ~JniBridge() override;

  bool Bind(ClassPathLookup* class_path_lookup) override;

  void Shutdown() override;

  bool RegisterDebuggee(bool* is_enabled,
                        const DebuggeeLabels& debuggee_labels) override;

  HangingGetResult ListActiveBreakpoints(
      std::vector<std::unique_ptr<BreakpointModel>>* breakpoints) override;

  void EnqueueBreakpointUpdate(
      std::unique_ptr<BreakpointModel> breakpoint) override;

  void TransmitBreakpointUpdates() override;

  bool HasPendingMessages() const override;

  bool RegisterBreakpointCanary(const std::string& breakpoint_id) override;

  bool ApproveBreakpointCanary(const std::string& breakpoint_id) override;

  bool IsEnabled(bool* is_enabled) override;

 private:
  // Callback to create an instance of Java class implementing
  // the com.google.devtools.cdbg.debuglets.java.HubClient interface.
  std::function<JniLocalRef()> hub_client_factory_;

  // Routines to serialize and deserialize breakpoint between BreakpointModel
  // and the data format we send to Java code.
  BreakpointSerializer breakpoint_serializer_;
  BreakpointDeserializer breakpoint_deserializer_;

  // Locks "transmit_queue_" for thread safety. Also used to prevent race
  // between "Bind" and "Shutdown".
  mutable absl::Mutex mu_;

  // Set to true after "Shutdown" has been called.
  bool shutdown_ = false;

  // JniHub class instance.
  JniGlobalRef jni_hub_;

  // Queue of breakpoint update messages pending transmission. Each item is
  // a breakpoint message serialized as ProtoBuf or JSON.
  TransmitQueue<SerializedBreakpoint> transmit_queue_;

  DISALLOW_COPY_AND_ASSIGN(JniBridge);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JNI_BRIDGE_H_
