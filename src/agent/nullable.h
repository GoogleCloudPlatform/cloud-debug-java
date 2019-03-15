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

#ifndef DEVTOOLS_CDBG_COMMON_NULLABLE_H_
#define DEVTOOLS_CDBG_COMMON_NULLABLE_H_

#include "common.h"

namespace devtools {
namespace cdbg {

template <class T>
class Nullable {
 public:
  Nullable() : has_value_(false) {}

  // Copy constructor.
  Nullable(const Nullable<T>& other) : has_value_(other.has_value()) {
    if (other.has_value()) {
      value_ = other.value_;
    }
  }

  // Default move constructor.
  Nullable(Nullable<T>&& other) = default;

  // Implicit initialization to no value.
  Nullable(std::nullptr_t)  // NOLINT
      : has_value_(false) {}

  // Implicit initialization from the value of type T.
  Nullable(T value)  // NOLINT
      : has_value_(true), value_(std::move(value)) {}

  // Assignment of the value of type Nullable<T>.
  Nullable& operator= (const Nullable<T>& other) {
    has_value_ = other.has_value();
    if (has_value_) {
      value_ = other.value();
    }

    return *this;
  }

  // Default move assignment operator.
  Nullable& operator= (Nullable<T>&& value) = default;

  // Implicit assignment of the value of type T.
  Nullable& operator= (T value) {
    set_value(std::move(value));
    return *this;
  }

  // Explicitly sets the value of type T.
  void set_value(T value) {
    has_value_ = true;
    value_ = std::move(value);
  }

  // Reset back to no value.
  void clear() {
    has_value_ = false;
  }

  // Returns true if value is initialized, false otherwise.
  bool has_value() const {
    return has_value_;
  }

  // Explicitly returns stored value.
  const T& value() const {
    DCHECK(has_value());
    return value_;
  }

  bool operator== (const Nullable<T>& other) const {
    return (!has_value_ && !other.has_value_) ||
           (has_value_ && other.has_value_ && (value_ == other.value_));
  }

  bool operator!= (const Nullable<T>& other) const {
    return !(*this == other);
  }

 private:
  bool has_value_;
  T value_;

  // Intentionally copyable.
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_COMMON_NULLABLE_H_
