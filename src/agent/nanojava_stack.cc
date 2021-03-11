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

#include "nanojava_stack.h"

#include <cstdint>

namespace devtools {
namespace cdbg {
namespace nanojava {

NanoJavaStack::NanoJavaStack(
    NanoJavaInternalErrorProvider* internal_error_provider,
    std::function<void()> fn_raise_null_pointer_exception,
    int max_stack)
    : internal_error_provider_(internal_error_provider),
      fn_raise_null_pointer_exception_(fn_raise_null_pointer_exception),
      max_stack_(max_stack),
      stack_(new Slot[max_stack]) {
}


NanoJavaStack::~NanoJavaStack() {
  delete[] stack_;
}


void NanoJavaStack::PushStackObject(jobject ref) {
  if (stack_pointer_ >= max_stack_) {
    DCHECK_EQ(stack_pointer_, max_stack_);
    SET_INTERNAL_ERROR("stack overflow");
    return;
  }

  stack_[stack_pointer_].type = Slot::Type::Object;
  stack_[stack_pointer_].ref = jni()->NewLocalRef(ref);

  ++stack_pointer_;
}

void NanoJavaStack::PushStack(Slot::Type type, int32_t value) {
  DCHECK(IsSingleSlotPrimitive(type));

  if (stack_pointer_ >= max_stack_) {
    DCHECK_EQ(stack_pointer_, max_stack_);
    SET_INTERNAL_ERROR("stack overflow");
    return;
  }

  stack_[stack_pointer_].type = type;
  stack_[stack_pointer_].primitive = value;

  ++stack_pointer_;
}

void NanoJavaStack::PushStack2(Slot::Type type, int64_t value) {
  DCHECK(IsDoubleSlotPrimitive(type));

  if (stack_pointer_ + 1 >= max_stack_) {
    SET_INTERNAL_ERROR("stack overflow");
    return;
  }

  stack_[stack_pointer_].type = type;
  stack_[stack_pointer_].primitive = static_cast<int32_t>(value);
  ++stack_pointer_;

  stack_[stack_pointer_].type = Slot::Type::Empty;
  stack_[stack_pointer_].primitive = static_cast<int32_t>(value >> 32);
  ++stack_pointer_;
}

void NanoJavaStack::PushStackAny(const JVariant& value) {
  switch (value.type()) {
    case JType::Void:
      return;

    case JType::Boolean: {
      jboolean boolean_value = false;
      value.get<jboolean>(&boolean_value);
      PushStack(Slot::Type::Int, boolean_value);
      return;
    }

    case JType::Byte: {
      jbyte byte_value = 0;
      value.get<jbyte>(&byte_value);
      PushStack(Slot::Type::Int, byte_value);
      return;
    }

    case JType::Char: {
      jchar char_value = 0;
      value.get<jchar>(&char_value);
      PushStack(Slot::Type::Int, char_value);
      return;
    }

    case JType::Short: {
      jshort short_value = 0;
      value.get<jshort>(&short_value);
      PushStack(Slot::Type::Int, short_value);
      return;
    }

    case JType::Int: {
      jint int_value = 0;
      value.get<jint>(&int_value);
      PushStack(Slot::Type::Int, int_value);
      return;
    }

    case JType::Float: {
      jfloat float_value = 0;
      value.get<jfloat>(&float_value);
      PushStack(Slot::Type::Float, as<int32_t>(float_value));
      return;
    }

    case JType::Long: {
      jlong long_value = 0;
      value.get<jlong>(&long_value);
      PushStack2(Slot::Type::Long, long_value);
      return;
    }

    case JType::Double: {
      jdouble double_value = 0;
      value.get<jdouble>(&double_value);
      PushStack2(Slot::Type::Double, as<int64_t>(double_value));
      return;
    }

    case JType::Object: {
      jobject ref = nullptr;
      value.get<jobject>(&ref);

      PushStackObject(ref);
      return;
    }
  }

  SET_INTERNAL_ERROR(
      "bad type $0",
      std::to_string(static_cast<int>(value.type())));
}


JniLocalRef NanoJavaStack::PopStackObject() {
  if (stack_pointer_ <= 0) {
    DCHECK_EQ(0, stack_pointer_);
    SET_INTERNAL_ERROR("stack underflow");
    return nullptr;
  }

  --stack_pointer_;

  Slot* slot = &stack_[stack_pointer_];

  if (slot->type != Slot::Type::Object) {
    SET_INTERNAL_ERROR(
        "stack slot type mismatch: actual = $0, expected = object",
        GetSlotTypeName(slot->type));
    return nullptr;
  }

  slot->type = Slot::Type::Empty;
  return JniLocalRef(slot->ref);
}


JniLocalRef NanoJavaStack::PopStackObjectNonNull() {
  JniLocalRef ref = PopStackObject();
  if (ref == nullptr) {
    fn_raise_null_pointer_exception_();
  }

  return ref;
}


JniLocalRef NanoJavaStack::PopStackObjectInstanceOf(jclass cls) {
  if (cls == nullptr) {
    SET_INTERNAL_ERROR("class object not available");
    return nullptr;
  }

  JniLocalRef ref = PopStackObjectNonNull();
  if (ref == nullptr) {
    return nullptr;
  }

  if (!jni()->IsInstanceOf(ref.get(), cls)) {
    SET_INTERNAL_ERROR(
        "object on stack ($0) is not an instance of $1",
        TypeNameFromJObjectSignature(GetObjectClassSignature(ref.get())),
        TypeNameFromJObjectSignature(GetClassSignature(cls)));
    return nullptr;
  }

  return ref;
}

int32_t NanoJavaStack::PopStack(Slot::Type expected_type) {
  DCHECK(IsSingleSlotPrimitive(expected_type));

  if (stack_pointer_ <= 0) {
    DCHECK_EQ(0, stack_pointer_);
    SET_INTERNAL_ERROR("stack underflow");
    return 0;
  }

  --stack_pointer_;

  Slot* slot = &stack_[stack_pointer_];

  if (slot->type != expected_type) {
    SET_INTERNAL_ERROR(
        "stack slot type mismatch: actual = $0, expected = $1",
        GetSlotTypeName(slot->type),
        GetSlotTypeName(expected_type));
    return false;
  }

  slot->type = Slot::Type::Empty;
  return slot->primitive;
}

int64_t NanoJavaStack::PopStack2(Slot::Type expected_type) {
  DCHECK(IsDoubleSlotPrimitive(expected_type));

  if (stack_pointer_ <= 1) {
    DCHECK_GE(stack_pointer_, 0);
    SET_INTERNAL_ERROR("stack underflow");
    return 0;
  }

  stack_pointer_ -= 2;

  Slot* slot1 = &stack_[stack_pointer_];
  const Slot* slot2 = &stack_[stack_pointer_ + 1];

  if ((slot1->type != expected_type) || (slot2->type != Slot::Type::Empty)) {
    SET_INTERNAL_ERROR(
        "stack slot type mismatch: actual = [$0, $1], expected = [$2, empty]",
        GetSlotTypeName(slot1->type),
        GetSlotTypeName(slot2->type),
        GetSlotTypeName(expected_type));
    return 0;
  }

  slot1->type = Slot::Type::Empty;

  return static_cast<uint64_t>(slot2->primitive) << 32 | slot1->primitive;
}

JVariant NanoJavaStack::PopStackAny(JType type) {
  switch (type) {
    case JType::Void:
      break;  // error.

    case JType::Boolean:
    case JType::Byte:
    case JType::Char:
    case JType::Short:
    case JType::Int: {
      const int32_t value = PopStack(Slot::Type::Int);

      switch (type) {
        case JType::Boolean:
          return JVariant::Boolean(value != 0);

        case JType::Byte:
          return JVariant::Byte(static_cast<jbyte>(value));

        case JType::Char:
          return JVariant::Char(static_cast<jchar>(value));

        case JType::Short:
          return JVariant::Short(static_cast<jshort>(value));

        case JType::Int:
          return JVariant::Int(value);

        default:
          DCHECK(false);
          return JVariant();  // error.
      }
    }

    case JType::Float:
      return JVariant::Float(as<float>(PopStack(Slot::Type::Float)));

    case JType::Long:
      return JVariant::Long(PopStack2(Slot::Type::Long));

    case JType::Double:
      return JVariant::Double(as<double>(PopStack2(Slot::Type::Double)));

    case JType::Object:
      return JVariant::LocalRef(PopStackObject());
  }

  SET_INTERNAL_ERROR(
      "bad type $0",
      std::to_string(static_cast<int>(type)));
  return JVariant();  // error.
}


jobject NanoJavaStack::PeekStackObject() {
  if (stack_pointer_ <= 0) {
    DCHECK_EQ(0, stack_pointer_);
    SET_INTERNAL_ERROR("stack underflow");
    return nullptr;
  }

  const Slot& slot = stack_[stack_pointer_ - 1];
  if (slot.type != Slot::Type::Object) {
    SET_INTERNAL_ERROR(
        "stack slot type mismatch: actual = $0, expected = object",
        GetSlotTypeName(slot.type));
    return nullptr;
  }

  return slot.ref;
}


void NanoJavaStack::StackDup() {
  if ((stack_pointer_ <= 0) || (stack_pointer_ >= max_stack_)) {
    SET_INTERNAL_ERROR("stack overflow or underflow");
    return;
  }

  const Slot& current_slot = stack_[stack_pointer_ - 1];
  Slot* new_slot = &stack_[stack_pointer_];

  Slot::Type type = current_slot.type;
  if (IsDoubleSlotPrimitive(type)) {
    SET_INTERNAL_ERROR("unexpected double slot primitive");
    return;
  }

  if (type == Slot::Type::Object) {
    new_slot->type = type;
    new_slot->ref = jni()->NewLocalRef(current_slot.ref);
  } else {
    *new_slot = current_slot;
  }

  ++stack_pointer_;
}


void NanoJavaStack::StackDup2() {
  if ((stack_pointer_ <= 1) || (stack_pointer_ + 1 >= max_stack_)) {
    SET_INTERNAL_ERROR("stack overflow or underflow");
    return;
  }

  for (int i = 0; i < 2; ++i, ++stack_pointer_) {
    const Slot& current_slot = stack_[stack_pointer_ - 2];
    Slot* new_slot = &stack_[stack_pointer_];

    if (current_slot.type == Slot::Type::Object) {
      new_slot->type = current_slot.type;
      new_slot->ref = jni()->NewLocalRef(current_slot.ref);
    } else {
      *new_slot = current_slot;
    }
  }
}


void NanoJavaStack::Swap(int pos1, int pos2) {
  if ((pos1 == pos2) ||
      (pos1 < 1) || (pos2 < 1) ||
      (pos1 > stack_pointer_) || (pos2 > stack_pointer_)) {
    SET_INTERNAL_ERROR(
        "bad arguments $0, $1",
        std::to_string(pos1),
        std::to_string(pos2));
    return;
  }

  std::swap(
      stack_[stack_pointer_ - pos1],
      stack_[stack_pointer_ - pos2]);
}


void NanoJavaStack::Discard() {
  if (stack_pointer_ <= 0) {
    DCHECK_EQ(0, stack_pointer_);
    SET_INTERNAL_ERROR("stack underflow");
    return;
  }

  FreeSlot(&stack_[stack_pointer_ - 1]);
  --stack_pointer_;
}

}  // namespace nanojava
}  // namespace cdbg
}  // namespace devtools
