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

// Ensure that Python.h is included before any other header in Python debuglet.
#include "common.h"

#include "leaky_bucket.h"

#include <algorithm>
#include <limits>

namespace devtools {
namespace cdbg {

static int64 NowInNanoseconds() {
  timespec time;
  clock_gettime(CLOCK_MONOTONIC, &time);
  return 1000000000LL * time.tv_sec + time.tv_nsec;
}


LeakyBucket::LeakyBucket(int64 capacity, int64 fill_rate)
    : capacity_(capacity),
      fractional_tokens_(0.0),
      fill_rate_(fill_rate),
      fill_time_ns_(NowInNanoseconds()) {
  tokens_ = capacity;
}


bool LeakyBucket::RequestTokensSlow(int64 requested_tokens) {
  // Getting the time outside the lock is significantly faster (reduces
  // contention, etc.).
  const int64 current_time_ns = NowInNanoseconds();

  std::lock_guard<std::mutex> lock(mu_);

  const int64 cur_tokens = AtomicLoadTokens();
  if (cur_tokens >= 0) {
    return true;
  }

  const int64 available_tokens =
      RefillBucket(requested_tokens + cur_tokens, current_time_ns);
  if (available_tokens >= 0) {
    return true;
  }

  // Since we were unable to satisfy the request, we need to restore the
  // requested tokens.
  AtomicIncrementTokens(requested_tokens);

  return false;
}


int64 LeakyBucket::RefillBucket(
    int64 available_tokens,
    int64 current_time_ns) {
  if (current_time_ns <= fill_time_ns_) {
    // We check to see if the bucket has been refilled after we checked the
    // current time but before we grabbed mu_. If it has there's nothing to do.
    return AtomicLoadTokens();
  }

  const int64 elapsed_ns = current_time_ns - fill_time_ns_;
  fill_time_ns_ = current_time_ns;

  // Calculate the number of tokens we can add. Note elapsed is in ns while
  // fill_rate_ is in tokens per second, hence the scaling factor.
  // We can get a negative amount of tokens by calling TakeTokens. Make sure we
  // don't add more than the capacity of leaky bucket.
  fractional_tokens_ +=
      std::min(elapsed_ns * (fill_rate_ / 1e9), static_cast<double>(capacity_));
  const int64 ideal_tokens_to_add = fractional_tokens_;

  const int64 max_tokens_to_add = capacity_ - available_tokens;
  int64 real_tokens_to_add;
  if (max_tokens_to_add < ideal_tokens_to_add) {
    fractional_tokens_ = 0.0;
    real_tokens_to_add = max_tokens_to_add;
  } else {
    real_tokens_to_add = ideal_tokens_to_add;
    fractional_tokens_ -= real_tokens_to_add;
  }

  return AtomicIncrementTokens(real_tokens_to_add);
}


void LeakyBucket::TakeTokens(int64 tokens) {
  const int64 remaining = AtomicIncrementTokens(-tokens);

  if (remaining < 0) {
    // (Try to) refill the bucket. If we don't do this, we could just
    // keep decreasing forever without refilling. We need to be
    // refilling at least as frequently as every capacity_ /
    // fill_rate_ seconds. Otherwise, we waste tokens.
    const int64 current_time_ns = NowInNanoseconds();

    std::lock_guard<std::mutex> lock(mu_);
    RefillBucket(remaining, current_time_ns);
  }
}

}  // namespace cdbg
}  // namespace devtools
