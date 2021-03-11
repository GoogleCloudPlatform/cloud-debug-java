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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_NANOJAVA_LOCALS_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_NANOJAVA_LOCALS_H_

#include <cstdint>

#include "common.h"
#include "jvariant.h"
#include "nanojava_internal_error_builder.h"
#include "nanojava_slot.h"

namespace devtools {
namespace cdbg {
namespace nanojava {

// Stores and manages local variables of an interpreted method. When methods of
// this class fail (for example due to invalid index of local varible), they
// set an internal error through "NanoJavaInternalErrorProvider" interface. The
// caller must verify error wasn't set before assuming the operation succeeded.
class NanoJavaLocals {
 public:
  // Allocates space for the local variables. Long and double types take two
  // slots.
  NanoJavaLocals(
      NanoJavaInternalErrorProvider* internal_error_provider,
      int max_locals);

  ~NanoJavaLocals();

  // Sets local variable to reference the specified object (which can be null).
  // Allocates a new local reference. Sets error if the local variable index
  // is beyond the locals size.
  void SetLocalObject(int local_index, jobject ref);

  // Set local variable to a single slot primitive value. Sets error if the
  // local variable index is beyond the locals size.
  void SetLocal(int local_index, Slot::Type type, int32_t value);

  // Set local variable to a double slot primitive value.
  void SetLocal2(int local_index, Slot::Type type, int64_t value);

  // Reads local variable of JType::Object type. Does not allocate a new local
  // reference. Sets error if variable index is invalid or if the local
  // variable does not contain an object reference.
  jobject GetLocalObject(int local_index);

  // Reads single slot primitive local variable. Sets error if variable index
  // is invalid.
  int32_t GetLocal(int local_index, Slot::Type expected_type);

  // Reads double slot primitive local variable. Sets error if variable index
  // is invalid.
  int64_t GetLocal2(int local_index, Slot::Type expected_type);

 private:
  // Gets the interface to construct internal error messages.
  NanoJavaInternalErrorProvider* internal_error_provider() {
    return internal_error_provider_;
  }

 private:
  // Interface to construct internal error messages. Not owned by this class.
  NanoJavaInternalErrorProvider* const internal_error_provider_;

  // Maximum number of local variables for the method. Arguments are counted
  // in. Long and double types take two slots of a local variable.
  const int max_locals_;

  // Local variables of the method.
  Slot* const locals_;

  DISALLOW_COPY_AND_ASSIGN(NanoJavaLocals);
};

}  // namespace nanojava
}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_NANOJAVA_LOCALS_H_
