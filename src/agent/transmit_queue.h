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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_TRANSMIT_QUEUE_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_TRANSMIT_QUEUE_H_

#include <memory>
#include <queue>

#include "common.h"

namespace devtools {
namespace cdbg {

// Limit amount of breakpoint hit results that we accumulate. This is to
// prevent debuglet from taking all available memory if there is something
// wrong with the communication channel to the Hub.
constexpr int kMaxTransmitQueueSize = 100;

// Maximum number of times that the message is re-transmitted before it is
// assumed to be poisonous and discarded
constexpr int kMaxRetryAttempts = 10;

// Simple list of pending UpdateActiveBreakpoint messages.
//
// Since communication channel is not reliable, "TransmitQueue" supports
// retrying. Each message maintains retry count and if exceeded, the message is
// considered poisonous and discarded.
//
// The class is not thread safe since formatting and transmission always run
// in the same thread (main debugger thread).
template <typename TMessage>
class TransmitQueue {
 public:
  // Single item in a transmit queue.
  struct Item {
    // Formatted message ready to be transmitted.
    std::unique_ptr<TMessage> message;

    // Number of times the message was attempted to be sent.
    int attempts { 0 };
  };

  TransmitQueue() { }

  // Appends the formatted message to the end of the queue. "Enqueue" honors
  // the "kMaxPendingResults" limit and discards the breakpoint if threshold
  // is reached.
  bool enqueue(std::unique_ptr<TMessage> message) {
    std::unique_ptr<Item> new_item(new Item);
    new_item->message = std::move(message);

    return enqueue(std::move(new_item));
  }

  // Returns message that failed to be sent back to the end of the queue. The
  // function increments retry count and discards the message if the retry
  // count exceeds kMaxRetryAttempts.
  bool enqueue(std::unique_ptr<Item> item) {
    if (item->attempts >= kMaxRetryAttempts) {
      LOG(ERROR) << "Item retry count exceeded maximum, discarding...";
      return false;
    }

    if (queue_.size() >= kMaxTransmitQueueSize) {
      LOG(ERROR) << "Transmission queue is full, discarding new item...";
      return false;
    }

    item->attempts++;
    queue_.push(std::move(item));

    return true;
  }

  // Checks whether the transmission queue is empty.
  bool empty() const {
    return queue_.empty();
  }

  // Pops the next message ready for transmission. Returns nullptr if the queue
  // is empty.
  std::unique_ptr<Item> pop() {
    if (queue_.empty()) {
      return nullptr;
    }

    std::unique_ptr<Item> front = std::move(queue_.front());
    queue_.pop();

    return front;
  }

 private:
  // Items pending transmission.
  std::queue<std::unique_ptr<Item>> queue_;

  DISALLOW_COPY_AND_ASSIGN(TransmitQueue);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_TRANSMIT_QUEUE_H_

