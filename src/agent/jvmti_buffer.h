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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVMTI_BUFFER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVMTI_BUFFER_H_

#include "common.h"

namespace devtools {
namespace cdbg {

// Smart pointer class to automatically release JVMTI buffer when the
// execution leaves the current scope.
template <typename T>
class JvmtiBuffer {
 public:
  JvmtiBuffer() : ptr_(nullptr) { }

  ~JvmtiBuffer() {
    if (ptr_ != nullptr) {
      jvmti()->Deallocate(reinterpret_cast<unsigned char*>(ptr_));
    }
  }

  const T* get() const { return ptr_; }

  T** ref() {
    DCHECK(ptr_ == nullptr);  // Uninitialized buffer expected
    return &ptr_;
  }

 private:
  // Pointer to JVMTI allocated memory or nullptr.
  T* ptr_;

  DISALLOW_COPY_AND_ASSIGN(JvmtiBuffer);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JVMTI_BUFFER_H_

