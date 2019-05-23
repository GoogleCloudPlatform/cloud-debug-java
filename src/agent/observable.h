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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_OBSERVABLE_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_OBSERVABLE_H_

#include <functional>
#include <list>
#include <memory>

#include "common.h"
#include "mutex.h"

namespace devtools {
namespace cdbg {

// Implements multicast event with variable number of arguments.
// This class is thread safe.
template <class... Args>
class Observable {
 public:
  typedef std::function<void(Args...)> Callback;
  typedef typename std::list<Callback> Handlers;

  // Use unique_ptr here to ensure the cookie is not kept around after call
  // to "Unsubscribe".
  typedef std::unique_ptr<typename Handlers::iterator> Cookie;

  Observable() { }

  ~Observable() {
    DCHECK(handlers_.empty());
  }

  // Subscribes to the event. Returns cookie used in "RemoveHandler" to
  // unsubscribe. The callback handler must not call to Subscribe or Unsubscribe
  // because it will cause deadlock.
  Cookie Subscribe(std::function<void(Args...)> fn) {
    absl::MutexLock lock(&mu_);

    auto it = handlers_.insert(handlers_.end(), std::move(fn));
    return Cookie(new typename Handlers::iterator(it));
  }

  // Removes subscription to the event. It is the responsibility of the caller
  // to make sure no event is being sent in another thread. This class does not
  // guarantee that no event will be delivered after "Unsubscribe" returns.
  void Unsubscribe(Cookie cookie) {
    if (cookie == nullptr) {
      return;
    }

    absl::MutexLock lock(&mu_);

    handlers_.erase(*cookie);
  }

  // Invokes all the subscribed handlers.
  void Fire(Args... args) const {
    // Clone the list of callbacks to prevent deadlock if the callback causes
    // the event to be fired recursively (from either this thread or another
    // thread). A more efficient solution would be to iterate over read/write
    // lock, but since this class is only used for low frequency events, the
    // performance impact of this solution is negligible.
    std::list<Callback> handlers_copy;
    {
      absl::MutexLock lock(&mu_);
      handlers_copy = handlers_;
    }

    for (auto& fn : handlers_copy) {
      fn(args...);
    }
  }

 private:
  // Locks access to "handlers_" list in this class.
  mutable absl::Mutex mu_;

  // List of subscribers to the event.
  Handlers handlers_;

  DISALLOW_COPY_AND_ASSIGN(Observable);
};


}  // namespace cdbg
}  // namespace devtools


#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_OBSERVABLE_H_
