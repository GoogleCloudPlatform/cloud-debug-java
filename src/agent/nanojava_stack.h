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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_NANOJAVA_STACK_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_NANOJAVA_STACK_H_

#include <algorithm>
#include <cstdint>

#include "common.h"
#include "jvariant.h"
#include "nanojava_internal_error_builder.h"
#include "nanojava_slot.h"

namespace devtools {
namespace cdbg {
namespace nanojava {

// Execution stack of an interpreted method.
// The stack requires:
// 1. Error provider: used to signal internal unrecoverable errors. The caller
//    must check error status before assuming successful operation.
// 2. Callback to raise NullPointerException: used in PopStackObjectNonNull.
// 3. Maximum stack size as specified in Java class file.
class NanoJavaStack {
 public:
  // Allocates space for the local variables. Long and double types take two
  // slots.
  NanoJavaStack(
      NanoJavaInternalErrorProvider* internal_error_provider,
      std::function<void()> fn_raise_null_pointer_exception,
      int max_stack);

  ~NanoJavaStack();

  // Pushes reference onto the operand stack. Allocates new local reference.
  // Sets error in case of stack overflow.
  void PushStackObject(jobject ref);

  // Pushes a primitive single stop value onto the stack. Sets error in case
  // of stack overflow.
  void PushStack(Slot::Type type, int32_t value);

  // Pushes a primitive double slot value (either Long or Double types) onto
  // the stack. Sets error in case of stack overflow.
  void PushStack2(Slot::Type type, int64_t value);

  // Pushes primitive or object value onto the stack. If "value" has Void type,
  // this function has no effect. Sets error in case of stack overflow.
  void PushStackAny(const JVariant& value);

  // Pops reference from the operand stack. The caller is responsible to
  // release the reference. Sets error in case of stack underflow.
  JniLocalRef PopStackObject();

  // Similar to PopStackObject, but sets "NullPointerException" if the
  // reference is null.
  JniLocalRef PopStackObjectNonNull();

  // Pops reference from the operand stack and verifies that the object
  // is instance of the specified class. The caller is responsible to
  // release the reference. Sets "NullPointerException" if popped reference
  // is null. Sets error in case of stack underflow or if the popped object
  // is not instance of "cls".
  JniLocalRef PopStackObjectInstanceOf(jclass cls);

  // Pops a primitive single slot value from the stack. Sets error and
  // returns 0 in case of stack underflow.
  int32_t PopStack(Slot::Type expected_type);

  // Pops a primitive double slot value (either Long or Double types) from the
  // stack. Sets error and returns 0 in case of stack underflow.
  int64_t PopStack2(Slot::Type expected_type);

  // Pops object, single primitive or double primitive from the stack depending
  // on "signature". Sets error and returns JType::Void on failure.
  JVariant PopStackAny(JType type);

  // Gets the reference to an object from the top of the stack. Sets error
  // and returns nullptr in case of stack underflow or if the top of the stack
  // is not an object reference.
  jobject PeekStackObject();

  // Duplicates object or primitive single slot value from on the stack.
  // Sets error in case of stack overflow or underflow.
  void StackDup();

  // Duplicates the top two slots on the execution stack. This can be
  // either two single slot entries or one double slot entry. Sets error in
  // case of stack overflow or underflow
  void StackDup2();

  // Swaps two stack slots. Sets error in case of stack underflow.
  void Swap(int pos1, int pos2);

  // Discards one stack slot. Sets error in case of stack underflow.
  void Discard();

 private:
  // Gets the interface to construct internal error messages.
  NanoJavaInternalErrorProvider* internal_error_provider() {
    return internal_error_provider_;
  }

 private:
  // Interface to construct internal error messages. Not owned by this class.
  NanoJavaInternalErrorProvider* const internal_error_provider_;

  // Callback function to raise NullPointerException.
  const std::function<void()> fn_raise_null_pointer_exception_;

  // Maximum stack size of the method.
  const int max_stack_;

  // Operand stack of the current method. Unlike x86, each method has
  // its own stack. This is because a method can return prematurely without
  // popping its stack.
  Slot* const stack_;

  // Index of the next free stack slot. When the stack is empty, this index
  // is 0. When the stack grows, the index is increased.
  int stack_pointer_ { 0 };

  DISALLOW_COPY_AND_ASSIGN(NanoJavaStack);
};

}  // namespace nanojava
}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_NANOJAVA_STACK_H_
