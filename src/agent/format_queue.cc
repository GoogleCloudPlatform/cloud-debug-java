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

#include "format_queue.h"

#include "statistician.h"

namespace devtools {
namespace cdbg {

FormatQueue::~FormatQueue() {
  if (!queue_.empty()) {
    LOG(WARNING) << "Pending breakpoint hit reports are abandoned";
  }
}


bool FormatQueue::IsEmpty() const {
  absl::MutexLock lock(&mu_);
  return queue_.empty();
}

void FormatQueue::RemoveAll() {
  absl::MutexLock lock(&mu_);

  for (Item& item : queue_) {
    if (item.collector != nullptr) {
      item.collector->ReleaseRefs();
    }
  }

  queue_.clear();
}


void FormatQueue::Enqueue(
    std::unique_ptr<BreakpointModel> breakpoint,
    std::unique_ptr<CaptureDataCollector> collector) {
  {
    Item item;
    item.breakpoint = std::move(breakpoint);
    item.collector = std::move(collector);

    if (item.breakpoint == nullptr) {
      DCHECK(item.breakpoint != nullptr);
      return;
    }

    absl::MutexLock lock(&mu_);

    // Replace pending non-final updates and ignore repeated final updates.
    for (Item& existing_item : queue_) {
      if (existing_item.breakpoint->id == item.breakpoint->id) {
        if (!existing_item.breakpoint->is_final_state) {
          std::swap(existing_item, item);
        }
        return;
      }
    }

    if (queue_.size() < kMaxFormatQueueSize) {
      queue_.push_back(std::move(item));
    }
  }

  on_item_enqueued_.Fire();
}


std::unique_ptr<BreakpointModel> FormatQueue::FormatAndPop() {
  ScopedStat ss(statFormattingTime);
  absl::MutexLock lock(&mu_);

  if (queue_.empty()) {
    return nullptr;
  }

  // TODO: refactor to avoid keeping mutex locked during "Format".

  Item& front = queue_.front();
  std::unique_ptr<BreakpointModel> breakpoint = std::move(front.breakpoint);

  if (front.collector != nullptr) {
    front.collector->Format(breakpoint.get());
    front.collector->ReleaseRefs();
  }

  queue_.pop_front();

  // Copy "expressions" to "evaluated_expressions". The size of "expressions"
  // and "evaluated_expressions" is expected to be the same if breakpoint was
  // evaluated. Otherwise "evaluated_expressions" will be empty. Use "std::min"
  // just to be on the safe side.
  const int expressions_count =
      std::min(
          breakpoint->expressions.size(),
          breakpoint->evaluated_expressions.size());
  for (int i = 0; i < expressions_count; ++i) {
    breakpoint->evaluated_expressions[i]->name = breakpoint->expressions[i];
  }

  return breakpoint;
}


}  // namespace cdbg
}  // namespace devtools


