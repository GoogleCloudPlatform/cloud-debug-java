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

#include "nanojava_locals.h"

#include <cstdint>

namespace devtools {
namespace cdbg {
namespace nanojava {

NanoJavaLocals::NanoJavaLocals(
    NanoJavaInternalErrorProvider* internal_error_provider,
    int max_locals)
    : internal_error_provider_(internal_error_provider),
      max_locals_(max_locals),
      locals_(new Slot[max_locals]) {
}


NanoJavaLocals::~NanoJavaLocals() {
  delete[] locals_;
}


void NanoJavaLocals::SetLocalObject(int local_index, jobject ref) {
  if ((local_index < 0) || (local_index >= max_locals_)) {
    SET_INTERNAL_ERROR(
        "bad local variable index $0",
        std::to_string(local_index));
    return;
  }

  Slot& slot = locals_[local_index];
  FreeSlot(&slot);
  slot.type = Slot::Type::Object;
  slot.ref = JniNewLocalRef(ref).release();
}

void NanoJavaLocals::SetLocal(int local_index, Slot::Type type, int32_t value) {
  DCHECK(IsSingleSlotPrimitive(type));

  if ((local_index < 0) || (local_index >= max_locals_)) {
    SET_INTERNAL_ERROR(
        "bad local variable index $0",
        std::to_string(local_index));
    return;
  }

  Slot& slot = locals_[local_index];
  FreeSlot(&slot);
  slot.type = type;
  slot.primitive = value;
}

void NanoJavaLocals::SetLocal2(int local_index, Slot::Type type,
                               int64_t value) {
  DCHECK(IsDoubleSlotPrimitive(type));

  if ((local_index < 0) || (local_index + 1 >= max_locals_)) {
    SET_INTERNAL_ERROR(
        "bad local variable index $0",
        std::to_string(local_index));
    return;
  }

  Slot& slot1 = locals_[local_index];
  Slot& slot2 = locals_[local_index + 1];

  FreeSlot(&slot1);
  FreeSlot(&slot2);

  slot1.type = type;
  slot2.type = Slot::Type::Empty;

  slot1.primitive = static_cast<int32_t>(value);
  slot2.primitive = static_cast<int32_t>(value >> 32);
}

jobject NanoJavaLocals::GetLocalObject(int local_index) {
  if ((local_index < 0) || (local_index >= max_locals_)) {
    SET_INTERNAL_ERROR(
        "bad local variable index $0",
        std::to_string(local_index));
    return nullptr;
  }

  if (locals_[local_index].type != Slot::Type::Object) {
    SET_INTERNAL_ERROR(
        "local variable $0 type mismatch, expected: Object, actual: $1",
        std::to_string(local_index),
        GetSlotTypeName(locals_[local_index].type));
    return nullptr;
  }

  return locals_[local_index].ref;
}

int32_t NanoJavaLocals::GetLocal(int local_index, Slot::Type expected_type) {
  DCHECK(IsSingleSlotPrimitive(expected_type));

  if ((local_index < 0) || (local_index >= max_locals_)) {
    SET_INTERNAL_ERROR(
        "bad local variable index $0",
        std::to_string(local_index));
    return 0;
  }

  if (locals_[local_index].type != expected_type) {
    SET_INTERNAL_ERROR(
        "local variable $0 type mismatch, expected: $1, actual: $2",
        std::to_string(local_index),
        GetSlotTypeName(expected_type),
          GetSlotTypeName(locals_[local_index].type));
    return 0;
  }

  return locals_[local_index].primitive;
}

int64_t NanoJavaLocals::GetLocal2(int local_index, Slot::Type expected_type) {
  DCHECK(IsDoubleSlotPrimitive(expected_type));

  if ((local_index < 0) || (local_index + 1 >= max_locals_)) {
    SET_INTERNAL_ERROR(
        "bad local variable index $0",
        std::to_string(local_index));
    return 0;
  }

  if ((locals_[local_index].type != expected_type) ||
      (locals_[local_index + 1].type != Slot::Type::Empty)) {
    SET_INTERNAL_ERROR(
        "local variable $0 type mismatch, "
        "expected: { $1, void }, actual: { $2, $3 }",
        std::to_string(local_index),
        GetSlotTypeName(expected_type),
        GetSlotTypeName(locals_[local_index].type),
        GetSlotTypeName(locals_[local_index + 1].type));
    return 0;
  }

  return static_cast<uint64_t>(locals_[local_index + 1].primitive) << 32 |
         locals_[local_index].primitive;
}

}  // namespace nanojava
}  // namespace cdbg
}  // namespace devtools
