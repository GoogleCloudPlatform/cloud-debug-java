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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_NANOJAVA_SLOT_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_NANOJAVA_SLOT_H_

#include <cstdint>

#include "common.h"

namespace devtools {
namespace cdbg {
namespace nanojava {

// Single slot in the operation stack of the interpreter or set of local
// variables. 64 bit primitives (long and double) take two slots (as per Java
// specifications). Objects are always local references. Popped stack entries
// have their object references released with JNIEnv::DeleteLocalRef.
// Discarded stack entries (in case a method returns prematurely) are
// released through JNIEnv::PopLocalFrame.
struct Slot {
  // Java Virtual Machine has a different type sets for primitive types
  // and data in its execution stack. Boolean, byte, char and short don't
  // have a distinct representation on the execution stack. Instead these
  // types are cast to int. This is per specification of JVM.
  enum class Type {
    Empty,
    Int,
    Float,
    Long,
    Double,
    Object
  };

  Type type { Type::Empty };
  union {
    uint32_t primitive;
    jobject ref;
  };
};


// Represents a primitive type in a different storage class. Both must be equal
// in size. This is different from type casting. For example "(int)1.5f" is 1,
// but as<int, float>(1.5f) will be the binary representation of 1.5f.
template<typename To, typename From>
To as(From value) {
  static_assert(sizeof(From) == sizeof(To),
                "Source and Target must be of the same size");
  return *reinterpret_cast<To*>(&value);
}


// Returns true if the specified data type is not an object and occupies one
// stack slot.
inline bool IsSingleSlotPrimitive(Slot::Type type) {
  return (type == Slot::Type::Int) || (type == Slot::Type::Float);
}


// Returns true if the specified data type occupied two stack slots.
inline bool IsDoubleSlotPrimitive(Slot::Type type) {
  return (type == Slot::Type::Long) || (type == Slot::Type::Double);
}


// Releases the local reference if the slot has JType::Object type.
inline void FreeSlot(Slot* slot) {
  if ((slot->type == Slot::Type::Object) && (slot->ref != nullptr)) {
    jni()->DeleteLocalRef(slot->ref);
    slot->ref = nullptr;
  }
}


// Gets enum name for logging purposes.
inline const char* GetSlotTypeName(Slot::Type type) {
  switch (type) {
    case Slot::Type::Empty:
      return "empty";

    case Slot::Type::Int:
      return "int";

    case Slot::Type::Float:
      return "float";

    case Slot::Type::Long:
      return "long";

    case Slot::Type::Double:
      return "double";

    case Slot::Type::Object:
      return "object";
  }

  DCHECK(false) << "Invalid value of type: " << static_cast<int>(type);
  return "<error>";
}

}  // namespace nanojava
}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_NANOJAVA_SLOT_H_
