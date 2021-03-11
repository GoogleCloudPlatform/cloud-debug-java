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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_NANOJAVA_INTERPRETER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_NANOJAVA_INTERPRETER_H_

#include <cstdint>

#include "class_file.h"
#include "common.h"
#include "jvariant.h"
#include "method_call_result.h"
#include "model.h"
#include "nanojava_internal_error_builder.h"
#include "nanojava_locals.h"
#include "nanojava_slot.h"
#include "nanojava_stack.h"

namespace devtools {
namespace cdbg {
namespace nanojava {

// Executes bytecode of a method. This interpreter that runs inside JVM
// is much slower than JVM, but it has much more control over the executed
// Java code. For example it can detect long loops and block changes to
// existing objects.
//
// Nested method calls use new instance of "NanoJavaInterpreter" for each call.
//
// This class is not thread safe. Only one method can be executed at a time.
//
class NanoJavaInterpreter : public NanoJavaInternalErrorProvider {
 public:
  // "NanoJavaInterpreter" executes Java bytecode much like regular JVM. It does
  // not enforce any restrictions or safety rules. For example implementation of
  // "NanoJavaInterpreter" doesn't prevent the code from hanging in an infinite
  // loop. Instead it defines a series of callbacks through "Supervisor"
  // interface. These callbacks can be used to restrict what the bytecode can
  // do.
  class Supervisor {
   public:
    virtual ~Supervisor() {}

    // Transitive call of a method. The interpreter does not assert valid types.
    // The "nonvirtual" parameter has no effect for static method calls. For
    // instance method calls "nonvirtual" chooses between virtual and
    // non-virtual calls. If "nonvirtual" is false, the derived method will be
    // used. If "nonvirtual" is true, the selected method will be called even if
    // overloaded by a derived class.
    virtual MethodCallResult InvokeNested(
        bool nonvirtual,
        const ConstantPool::MethodRef& method,
        jobject source,
        std::vector<JVariant> arguments) = 0;

    // Indicates that one more instruction is about to be executed. Returns
    // error code if subsequent execution should be blocked. Returns nullptr
    // if execution can proceed.
    virtual std::unique_ptr<FormatMessageModel> IsNextInstructionAllowed() = 0;

    // Indicates that a new object has been allocated by the interpreter.
    // This can be either a regular object or an array.
    virtual void NewObjectAllocated(jobject obj) = 0;

    // Called just before a new array is allocated. Returns error code if the
    // operation should be blocked. Returns nullptr to proceed.
    virtual std::unique_ptr<FormatMessageModel> IsNewArrayAllowed(
        int32_t count) = 0;

    // Called before execution of opcodes that change array objects. Returns
    // error code if the operation should be blocked. Returns nullptr if the
    // execution can proceed.
    virtual std::unique_ptr<FormatMessageModel> IsArrayModifyAllowed(
        jobject array) = 0;

    // Called before execution of instruction to change a field. The "target"
    // parameter is ignored for static fields ("is_static" = true). Returns
    // error code if the operation should be blocked. Returns nullptr if the
    // execution can proceed.
    virtual std::unique_ptr<FormatMessageModel> IsFieldModifyAllowed(
        jobject target,
        const ConstantPool::FieldRef& field) = 0;
  };

  // Current state of the interpreter used for troubleshooting purposes.
  class DiagState {
   public:
    DiagState(
        const NanoJavaInterpreter* interpreter,
        const NanoJavaInterpreter* parent_frame);

    // Gets index of the currently executing instruction.
    int ip() const { return interpreter_->ip_; }

    // Gets the pointer to the interpreted method that invoke this method. Used
    // to reconstruct the interpreter call stack for debugging purposes.
    const NanoJavaInterpreter* parent_frame() const { return parent_frame_; }

    // Gets the name of the associated method (for troubleshooting).
    std::string method_name() const;

    // Format call stack of the interpreted methods.
    std::string FormatCallStack() const;

   private:
    // Instance of "NanoJavaInterpreter" that owns this class.
    const NanoJavaInterpreter* const interpreter_;

    // Pointer to the interpreted method that invoke this method. Used to
    // reconstruct the interpreter call stack for debugging purposes.
    const NanoJavaInterpreter* const parent_frame_;

    DISALLOW_COPY_AND_ASSIGN(DiagState);
  };

  // Class constructor. "supervisor", "method", "parent_frame", "instance" and
  // "arguments" are not owned by this class. Their lifetime must exceed
  // lifetime of this class. "parent_frame" might be nullptr if this is a top
  // level caller.
  NanoJavaInterpreter(
      Supervisor* supervisor,
      ClassFile::Method* method,
      const NanoJavaInterpreter* parent_frame,
      jobject instance,
      const std::vector<JVariant>& arguments);

  ~NanoJavaInterpreter() override;

  // Runs the interpreter through the method bytecode.
  MethodCallResult Execute();

  // Returns true if some kind of internal error has previously occurred.
  bool IsError() const {
    return (result_ != nullptr) &&
           (result_->result_type() == MethodCallResult::Type::Error);
  }

  // Returns true if some kind of internal error has previously occurred or
  // if there is a pending thrown exception.
  bool IsErrorOrException() const {
    return (result_ != nullptr) &&
           (result_->result_type() != MethodCallResult::Type::Success);
  }

  // Sets result of the method. This will stop the execution.
  void SetResult(MethodCallResult result) override;

  // Gets the current state of the interpreter for troubleshooting purposes.
  const DiagState& diag_state() const { return diag_state_; }

  // Gets the name of the associated method (for troubleshooting).
  std::string method_name() const override { return diag_state_.method_name(); }

  // Format call stack of the interpreted methods.
  std::string FormatCallStack() const override {
    return diag_state_.FormatCallStack();
  }

  // Counts the stack depth of the execution.
  int GetStackDepth() const;

 private:
  // Sets method arguments as local variables.
  void InitializeLocals();

  // Initializes local variables and run through the main loop of the
  // interpreter for the current method.
  void ExecuteInternal();

  // Interprets a single instruction at "ip_" offset. Returns offset of
  // the next instruction to run.
  int ExecuteSingleInstruction();

  // Gets the Java class object corresponding to the specified JVMTI signature.
  // In case of failure or Java exception, "LoadClass" should call "SetResult"
  // and return nullptr. "LoadClass" must never return nullptr without
  // setting error or exception.
  jclass LoadClass(ClassIndexer::Type* class_reference);

  // Sets result of a method to a new exception object.
  template <typename T>
  void RaiseException(T* proxy_class);

  // Sets result of a method to a new instance of NullPointerException object.
  void RaiseNullPointerException();

  // Pops array object and checks that we are allowed to write into it (this
  // decision is made by "Supervisor"). Sets error and returns nullptr if
  // top of the stack is null, an object of a different type or the array
  // is not allowed to be modified.
  JniLocalRef PopModifiablePrimitiveArray(const char* array_signature);

  // Called before execution of any instruction. Returns false to stop
  // execution of the current method (setting method result to error
  // in this case).
  bool CheckNextInstructionAllowed();

  // Called just before a new array is allocated. Returns false if the
  // operation should be blocked and sets method result to error.
  bool CheckNewArrayAllowed(int32_t count);

  // Called before execution of opcodes that change array objects. Returns
  // false if the operation should be blocked and sets method result to
  // error in this case.
  bool CheckArrayModifyAllowed(jobject array);

  // Verifies that the field was actually found when the class was loaded. Sets
  // error if the field is not available. Assumes that the field is not
  // available because the class wasn't found.
  void CheckFieldFound(const ConstantPool::FieldRef& field);

  // Called before execution of opcodes that change a field (either instance
  // or static). Returns false if the operation should be blocked and sets
  // method result to error in this case.
  bool CheckFieldModifyAllowed(
      jobject target,
      const ConstantPool::FieldRef& field);

  // Check pending Java exception through JNI interface and sets result
  // accordingly. Returns false in case of exception.
  bool CheckJavaException();

  // Sets "instruction not supported" error message.
  void SetOpcodeNotSupportedError(std::string opcode);

  // If there is a pending exception, scans try-catch blocks and looks for
  // the exception handler. If exception handler is found, this function will
  // set "ip_" to the handler block, clear the pending exception and return
  // true. Returns false if no action was taken.
  bool DispatchExceptionHandler();

  // Gets the interface to construct internal error messages.
  NanoJavaInternalErrorProvider* internal_error_provider() { return this; }

  // Implements XRETURN instructions.
  void ReturnOperation(JType return_opcode_type);

  // Single slot binary operation on two values from the top of the stack.
  void PrimitiveBinaryOperation(Slot::Type type,
                                int32_t (NanoJavaInterpreter::*fn)(int32_t,
                                                                   int32_t)) {
    int32_t n2 = stack_.PopStack(type);
    int32_t n1 = stack_.PopStack(type);
    stack_.PushStack(type, (this->*fn)(n2, n1));
  }

  // Double slot binary operation on two values from the top of the stack.
  void PrimitiveBinaryOperation2(Slot::Type type,
                                 int64_t (NanoJavaInterpreter::*fn)(int64_t,
                                                                    int64_t)) {
    int64_t n2 = stack_.PopStack2(type);
    int64_t n1 = stack_.PopStack2(type);
    stack_.PushStack2(type, (this->*fn)(n2, n1));
  }

  // Implements IF_ICMPXX instructions.
  template <class Compare>
  bool IfICmpOperation() {
    int32_t n2 = stack_.PopStack(Slot::Type::Int);
    int32_t n1 = stack_.PopStack(Slot::Type::Int);
    return Compare()(n1, n2);
  }

  // Implements LDC instruction.
  void LdcOperation(int constant_pool_index);

  // Implements INVOKExxx instructions.
  void InvokeOperation(uint8_t opcode, const ConstantPool::MethodRef& operand);

  // Implements NEWARRAY instruction.
  void NewArrayOperation(int array_type);

  // Implements GETSTATIC instruction.
  void GetStaticFieldOperation(const ConstantPool::FieldRef& operand);

  // Implements GETFIELD instruction.
  void GetInstanceFieldOperation(const ConstantPool::FieldRef& operand);

  // Implements PUTFIELD instruction.
  void SetInstanceFieldOperation(const ConstantPool::FieldRef& operand);

  //
  // Implementation of binary operations.
  //
  // Notes:
  // 1. These functions have to be members of this class because
  //    "PrimitiveBinaryOperation" function gets a class function pointer.
  // 2. These functions are templated to implement all the 4 operation types
  //    in a single method (int, long, float, double).
  //

  template <typename TPrimitive, typename TSlot>
  TSlot add(TSlot n1, TSlot n2) {
    return as<TSlot>(as<TPrimitive>(n2) + as<TPrimitive>(n1));
  }

  template <typename TPrimitive, typename TSlot>
  TSlot sub(TSlot n1, TSlot n2) {
    return as<TSlot>(as<TPrimitive>(n2) - as<TPrimitive>(n1));
  }

  template <typename TPrimitive, typename TSlot>
  TSlot mul(TSlot n1, TSlot n2) {
    return as<TSlot>(as<TPrimitive>(n2) * as<TPrimitive>(n1));
  }

  template <typename TSlot>
  TSlot div_int(TSlot n1, TSlot n2);

  template <typename TPrimitive, typename TSlot>
  TSlot div_float(TSlot n1, TSlot n2) {
    return as<TSlot>(as<TPrimitive>(n2) / as<TPrimitive>(n1));
  }

  template <typename TSlot>
  TSlot modulo_int(TSlot n1, TSlot n2);

  template <typename TPrimitive, typename TSlot>
  TSlot modulo_float(TSlot n1, TSlot n2);

  template <typename TSlot>
  TSlot and_int(TSlot n1, TSlot n2) {
    return n2 & n1;
  }

  template <typename TSlot>
  TSlot or_int(TSlot n1, TSlot n2) {
    return n2 | n1;
  }

  template <typename TSlot>
  TSlot xor_int(TSlot n1, TSlot n2) {
    return n2 ^ n1;
  }

  int32_t ishl(int32_t n1, int32_t n2) { return n2 << (n1 & 0x1F); }

  int32_t ishr(int32_t n1, int32_t n2) { return n2 >> (n1 & 0x1F); }

  int32_t iushr(int32_t n1, int32_t n2) {
    return as<int32_t>(static_cast<uint32_t>(n2) >> (n1 & 0x1F));
  }

 private:
  // Controls method execution and exposes some aspects of the environment
  // to the interpreter. Not owned by this class.
  Supervisor* const supervisor_;

  // Interpreted method. Not owned by this class.
  ClassFile::Method* method_;

  // Exposes interpreter state for troubleshooting purposes.
  DiagState diag_state_;

  // Object instance used for instance method calls. Ignored for static calls.
  const jobject instance_;

  // Method call arguments (not including "this").
  const std::vector<JVariant>& arguments_;

  // Execution stack of the interpreted method.
  NanoJavaStack stack_;

  // Local variables of the current method.
  NanoJavaLocals locals_;

  // Index of the next instruction to execute.
  int ip_ { 0 };

  // Current execution status. Once the method has completed, exception thrown
  // or error occurred, this variable will be set.
  std::unique_ptr<MethodCallResult> result_;

  DISALLOW_COPY_AND_ASSIGN(NanoJavaInterpreter);
};

}  // namespace nanojava
}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_NANOJAVA_INTERPRETER_H_
