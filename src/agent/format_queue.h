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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_FORMAT_QUEUE_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_FORMAT_QUEUE_H_

#include <memory>
#include <queue>

#include "capture_data_collector.h"
#include "common.h"
#include "model.h"
#include "mutex.h"
#include "observable.h"

namespace devtools {
namespace cdbg {

// Limit amount of breakpoint hit results that we accumulate. This is to
// prevent debuglet from taking all available memory if there is something
// wrong with the communication channel to the Hub.
constexpr int kMaxFormatQueueSize = 100;

// Implements a queue of breakpoint results that are waiting for be formatted
// to the message that can be transmitted to the Hub service. The class is
// thread safe since breakpoints are captured and formatted on different
// threads.
class FormatQueue {
 public:
  // Event fired when a new breakpoint update is enqueued. This event is fired
  // in the same thread that enqueued the update. The subscriber to this event
  // should defer as much work as possible outside of the event callback.
  typedef Observable<> OnItemEnqueued;

  FormatQueue() { }
  ~FormatQueue();

  // Returns true if FormatQueue has no data, false otherwise.
  bool IsEmpty() const;

  // Removes everything from the queue. This is typically needed when JVM
  // goes down and we want to clean up all the resources.
  void RemoveAll();

  // Appends the completed breakpoint to the end of the queue. The "breakpoint"
  // parameter contains the definition of the breakpoint (without the results).
  // The "collector" captures call stack, local variables and objects on
  // breakpoint hit and can format the captured data into the protocol message.
  // "FormatQueue" takes ownership over "breakpoint" and "collector". "Enqueue"
  // honors the "kMaxPendingResults" limit and discards the breakpoint if
  // threshold is reached.
  // "jni" is used to provide JNI context to "OnItemEnqueued" event.
  void Enqueue(
      std::unique_ptr<BreakpointModel> breakpoint,
      std::unique_ptr<CaptureDataCollector> collector);

  // If the queue is empty, returns nullptr. Otherwise pops the first entry in
  // the queue, formats it (i.e. combines breakpoint definition with breakpoint
  // results and captures immutable Java objects) and returns it to the caller.
  std::unique_ptr<BreakpointModel> FormatAndPop();

  // Subscribes to receive OnItemEnqueued notifications.
  OnItemEnqueued::Cookie SubscribeOnItemEnqueuedEvents(
      OnItemEnqueued::Callback fn) {
    return on_item_enqueued_.Subscribe(fn);
  }

  // Unsubscribes from OnItemEnqueued notifications.
  void UnsubscribeOnItemEnqueuedEvents(OnItemEnqueued::Cookie cookie) {
    on_item_enqueued_.Unsubscribe(std::move(cookie));
  }

 private:
  // Single item in a queue.
  struct Item {
    // Breakpoint definition typically without results. "breakpoint" may
    // include hit results if collector is nullptr.
    std::unique_ptr<BreakpointModel> breakpoint;

    // Capture of call stack, local variables and objects on breakpoint hit.
    std::unique_ptr<CaptureDataCollector> collector;
  };

  // Locks access to the queue.
  mutable absl::Mutex mu_;

  // Breakpoint hit results that wait to be reported to the hub. The list is
  // used as queue. It is important to use linked list here, so that the pointer
  // to "Item" returned by "Top" remains valid while new elements are pushed
  // into the queue.
  std::list<Item> queue_;

  // Allows other objects to receive synchronous notifications each time
  // a new breakpoint update is enqueued.
  OnItemEnqueued on_item_enqueued_;

  DISALLOW_COPY_AND_ASSIGN(FormatQueue);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_FORMAT_QUEUE_H_
