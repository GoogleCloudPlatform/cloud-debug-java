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

#ifndef DEVTOOLS_CDBG_COMMON_LEAKY_BUCKET_H_
#define DEVTOOLS_CDBG_COMMON_LEAKY_BUCKET_H_

#include <atomic>
#include <cstdint>
#include <mutex>  // NOLINT

#include "common.h"

namespace devtools {
namespace cdbg {

// Implements a bucket that fills tokens at a constant rate up to a maximum
// capacity. This class is thread-safe.
//
class LeakyBucket {
 public:
  // "capacity":  The max number of tokens the bucket can hold at any point.
  // "fill_rate": The rate which the bucket fills in tokens per second.
  LeakyBucket(int64_t capacity, int64_t fill_rate);

  ~LeakyBucket() {}

  // Requests tokens from the bucket. If the bucket does not contain enough
  // tokens, returns false, and no tokens are issued. Requesting more
  // tokens than the "capacity_" will always fail, and CHECKs in debug mode.
  //
  // The LeakyBucket has at most "capacity_" tokens. You can use this to control
  // your bursts, subject to some limitations. An example of the control that
  // the capacity provides: imagine that you have no traffic, and therefore no
  // tokens are being acquired. Suddenly, infinite demand arrives.
  // At most "capacity_" tokens will be granted immediately. Subsequent
  // requests will only be admitted based on the fill rate.
  inline bool RequestTokens(int64_t requested_tokens);

  // Takes tokens from bucket, possibly sending the number of tokens in the
  // bucket negative.
  void TakeTokens(int64_t tokens);

 private:
  // The slow path of RequestTokens. Grabs a lock and may refill tokens_
  // using the fill rate and time passed since last fill.
  bool RequestTokensSlow(int64_t requested_tokens);

  // Refills the bucket with newly added tokens since last update and returns
  // the current amount of tokens in the bucket. 'available_tokens' indicates
  // the number of tokens in the bucket before refilling. 'current_time_ns'
  // indicates the current time in nanoseconds.
  int64_t RefillBucket(int64_t available_tokens, int64_t current_time_ns);

  // Atomically increment "tokens_".
  inline int64_t AtomicIncrementTokens(int64_t increment) {
    return tokens_.fetch_add(increment, std::memory_order_relaxed) + increment;
  }

  // Atomically load the value of "tokens_".
  inline int64_t AtomicLoadTokens() const {
    return tokens_.load(std::memory_order_relaxed);
  }

 private:
  // Protects fill_time_ns_ and fractional_tokens_.
  std::mutex mu_;

  // Current number of tokens in the bucket. Tokens is guarded by "mu_"
  // only if we're planning to increment it. This is to prevent "tokens_"
  // from ever exceeding "capacity_". See RequestTokens in the leaky_bucket.cc
  // file.
  //
  // Tokens can be momentarily negative, either via TakeTokens or
  // during a normal RequestTokens that was not satisfied.
  std::atomic<int64_t> tokens_;

  // Capacity of the bucket.
  const int64_t capacity_;

  // Although the main token count is an integer we also track fractional tokens
  // for increased precision.
  double fractional_tokens_;

  // Fill rate in tokens per second.
  const int64_t fill_rate_;

  // Time in nanoseconds of the last refill.
  int64_t fill_time_ns_;

  DISALLOW_COPY_AND_ASSIGN(LeakyBucket);
};

// Inline fast-path.
inline bool LeakyBucket::RequestTokens(int64_t requested_tokens) {
  if (requested_tokens > capacity_) {
    return false;
  }

  // Try and grab some tokens. remaining is how many tokens are
  // left after subtracting out requested tokens.
  int64_t remaining = AtomicIncrementTokens(-requested_tokens);
  if (remaining >= 0) {
    // We had at least as much as we needed.
    return true;
  }

  return RequestTokensSlow(requested_tokens);
}

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_COMMON_LEAKY_BUCKET_H_
