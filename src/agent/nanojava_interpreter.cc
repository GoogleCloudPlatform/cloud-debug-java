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

#include "nanojava_interpreter.h"

#include <cmath>
#include <cstdint>
#include <limits>

#include "jni_proxy_arithmeticexception.h"
#include "jni_proxy_classcastexception.h"
#include "jni_proxy_negativearraysizeexception.h"
#include "jni_proxy_nullpointerexception.h"
#include "messages.h"

namespace devtools {
namespace cdbg {
namespace nanojava {

// Number of local variable slots to allocate beyond what the method declares.
// These local variables are used for interim object references.
static constexpr int kExtraLocalVariables = 32;

NanoJavaInterpreter::NanoJavaInterpreter(
    Supervisor* supervisor,
    ClassFile::Method* method,
    const NanoJavaInterpreter* parent_frame,
    jobject instance,
    const std::vector<JVariant>& arguments)
    : supervisor_(supervisor),
      method_(method),
      diag_state_(this, parent_frame),
      instance_(instance),
      arguments_(arguments),
      stack_(
          this,
          std::bind(&NanoJavaInterpreter::RaiseNullPointerException, this),
          method->GetMaxStack()),
      locals_(this, method->GetMaxLocals()) {
  DCHECK(method_->IsStatic() == (instance_ == nullptr));
}


NanoJavaInterpreter::~NanoJavaInterpreter() {
}

// Runs the interpreter through the method bytecode.
MethodCallResult NanoJavaInterpreter::Execute() {
  if ((method_->method_modifiers() & JVM_ACC_NATIVE) != 0) {
    return MethodCallResult::Error({ NativeMethodNotSafe, { method_name() } });
  }

  // Calculate maximum number of local references that we might need for
  // this method. Assume the worst case when all slots are used for
  // references. We also allocate a fixed number of local references for
  // the interpreter code internal use.
  const int capacity =
      method_->GetMaxStack() + method_->GetMaxLocals() + kExtraLocalVariables;
  if (jni()->PushLocalFrame(capacity) != 0) {
    jni()->ExceptionClear();
    return INTERNAL_ERROR_RESULT(
        "no space for $0 local variables",
        std::to_string(capacity));
  }

  result_ = nullptr;
  ExecuteInternal();

  DCHECK(result_ != nullptr);

  jni()->PopLocalFrame(nullptr);  // We don't need result.

  return std::move(*result_);
}


// Initializes locals in the current frame.
void NanoJavaInterpreter::InitializeLocals() {
  int local_index = 0;

  if (!method_->IsStatic()) {
    if (instance_ == nullptr) {
      RaiseNullPointerException();
      return;
    }

    locals_.SetLocalObject(0, instance_);
    ++local_index;
  }

  for (const JVariant& argument : arguments_) {
    const jvalue value = argument.get_jvalue();
    switch (argument.type()) {
      // Reference.
      case JType::Object:
        locals_.SetLocalObject(local_index, value.l);
        ++local_index;
        break;

      // One slot primitive.
      case JType::Boolean:
        locals_.SetLocal(local_index, Slot::Type::Int, value.z);
        ++local_index;
        break;

      case JType::Byte:
        locals_.SetLocal(local_index, Slot::Type::Int, value.b);
        ++local_index;
        break;

      case JType::Char:
        locals_.SetLocal(local_index, Slot::Type::Int, value.c);
        ++local_index;
        break;

      case JType::Short:
        locals_.SetLocal(local_index, Slot::Type::Int, value.s);
        ++local_index;
        break;

      case JType::Int:
        locals_.SetLocal(local_index, Slot::Type::Int, value.i);
        ++local_index;
        break;

      case JType::Float:
        locals_.SetLocal(local_index, Slot::Type::Float, value.i);
        ++local_index;
        break;

      // Two slot primitive.
      case JType::Long:
        locals_.SetLocal2(local_index, Slot::Type::Long, value.j);
        local_index += 2;
        break;

      case JType::Double:
        locals_.SetLocal2(local_index, Slot::Type::Double,
                          as<int64_t>(value.d));
        local_index += 2;
        break;

      default:
        SET_INTERNAL_ERROR(
            "bad argument type $0",
            std::to_string(static_cast<int>(argument.type())));
        break;
    }
  }
}


void NanoJavaInterpreter::ExecuteInternal() {
  InitializeLocals();
  if (result_ != nullptr) {
    return;  // "InitializeLocals" failed.
  }

  ip_ = 0;
  while (true) {
    if (!CheckNextInstructionAllowed()) {
      return;
    }

    const int next_ip = ExecuteSingleInstruction();

    if (result_ == nullptr) {
      ip_ = next_ip;
      continue;
    }

    if (DispatchExceptionHandler()) {
      DCHECK(result_ == nullptr);

      // The call to "CatchPendingException" was successful. It reset "result_"
      // back to nullptr and updated "ip_" to the catch/finally handler offset.
      continue;
    }

    // One of these happened:
    // 1. Method completed successfully.
    // 2. Method threw an exception that didn't have a catch block.
    // 3. Error occurred during method execution.
    return;
  }
}


int NanoJavaInterpreter::ExecuteSingleInstruction() {
  Nullable<ClassFile::Instruction> nullable_instruction =
      method_->GetInstruction(ip_);
  if (!nullable_instruction.has_value()) {
    SET_INTERNAL_ERROR(
        "failed to read instruction at offset $0",
        std::to_string(ip_));
    return -1;
  }

  const ClassFile::Instruction& instruction = nullable_instruction.value();
  int next_ip = instruction.next_instruction_offset;

  const uint8_t opcode = instruction.opcode;
  switch (opcode) {
    case JVM_OPC_nop:
      break;

    case JVM_OPC_aconst_null:
      stack_.PushStackObject(nullptr);
      break;

    case JVM_OPC_bipush:
    case JVM_OPC_sipush:
      stack_.PushStack(Slot::Type::Int, instruction.int_operand);
      break;

    case JVM_OPC_iconst_m1:
    case JVM_OPC_iconst_0:
    case JVM_OPC_iconst_1:
    case JVM_OPC_iconst_2:
    case JVM_OPC_iconst_3:
    case JVM_OPC_iconst_4:
    case JVM_OPC_iconst_5:
      stack_.PushStack(
          Slot::Type::Int,
          instruction.opcode - JVM_OPC_iconst_0);
      break;

    case JVM_OPC_lconst_0:
    case JVM_OPC_lconst_1:
      stack_.PushStack2(
          Slot::Type::Long,
          instruction.opcode - JVM_OPC_lconst_0);
      break;

    case JVM_OPC_fconst_0:
    case JVM_OPC_fconst_1:
    case JVM_OPC_fconst_2: {
      const float value = instruction.opcode - JVM_OPC_fconst_0;
      stack_.PushStack(Slot::Type::Float, as<int32_t>(value));
      break;
    }

    case JVM_OPC_dconst_0:
    case JVM_OPC_dconst_1: {
      const double value = instruction.opcode - JVM_OPC_dconst_0;
      stack_.PushStack2(Slot::Type::Double, as<int64_t>(value));
      break;
    }

    case JVM_OPC_iload:
      stack_.PushStack(
          Slot::Type::Int,
          locals_.GetLocal(instruction.int_operand, Slot::Type::Int));
      break;

    case JVM_OPC_fload:
      stack_.PushStack(
          Slot::Type::Float,
          locals_.GetLocal(instruction.int_operand, Slot::Type::Float));
      break;

    case JVM_OPC_lload:
      stack_.PushStack2(
          Slot::Type::Long,
          locals_.GetLocal2(instruction.int_operand, Slot::Type::Long));
      break;

    case JVM_OPC_dload:
      stack_.PushStack2(
          Slot::Type::Double,
          locals_.GetLocal2(instruction.int_operand, Slot::Type::Double));
      break;

    case JVM_OPC_aload:
      stack_.PushStackObject(
          locals_.GetLocalObject(instruction.int_operand));
      break;

    case JVM_OPC_istore:
      locals_.SetLocal(
          instruction.int_operand,
          Slot::Type::Int,
          stack_.PopStack(Slot::Type::Int));
      break;

    case JVM_OPC_fstore:
      locals_.SetLocal(
          instruction.int_operand,
          Slot::Type::Float,
          stack_.PopStack(Slot::Type::Float));
      break;

    case JVM_OPC_lstore:
      locals_.SetLocal2(
          instruction.int_operand,
          Slot::Type::Long,
          stack_.PopStack2(Slot::Type::Long));
      break;

    case JVM_OPC_dstore:
      locals_.SetLocal2(
          instruction.int_operand,
          Slot::Type::Double,
          stack_.PopStack2(Slot::Type::Double));
      break;

    case JVM_OPC_astore:
      locals_.SetLocalObject(
          instruction.int_operand,
          stack_.PopStackObject().get());
      break;

    case JVM_OPC_bastore: {
      const int32_t value = stack_.PopStack(Slot::Type::Int);
      const int32_t index = stack_.PopStack(Slot::Type::Int);
      JniLocalRef ref = stack_.PopStackObjectNonNull();
      if (IsErrorOrException()) {
        break;
      }

      if (!CheckArrayModifyAllowed(ref.get())) {
        break;
      }

      const std::string& signature = GetObjectClassSignature(ref.get());
      if (signature == "[Z") {  // Boolean array.
        jni()->SetBooleanArrayRegion(
            static_cast<jbooleanArray>(ref.get()),
            index,
            1,
            reinterpret_cast<const jboolean*>(&value));
      } else if (signature == "[B") {  // Byte array.
        jni()->SetByteArrayRegion(
            static_cast<jbyteArray>(ref.get()),
            index,
            1,
            reinterpret_cast<const jbyte*>(&value));
      } else {
        SET_INTERNAL_ERROR("$0 is not a boolean or byte array", signature);
        break;
      }

      CheckJavaException();

      break;
    }

    case JVM_OPC_castore: {
      const jchar value = stack_.PopStack(Slot::Type::Int);
      const int32_t index = stack_.PopStack(Slot::Type::Int);
      JniLocalRef ref = PopModifiablePrimitiveArray("[C");
      if (IsErrorOrException()) {
        break;
      }

      jni()->SetCharArrayRegion(
          static_cast<jcharArray>(ref.get()),
          index,
          1,
          &value);
      CheckJavaException();
      break;
    }

    case JVM_OPC_sastore: {
      const jshort value = stack_.PopStack(Slot::Type::Int);
      const int32_t index = stack_.PopStack(Slot::Type::Int);
      JniLocalRef ref = PopModifiablePrimitiveArray("[S");
      if (IsErrorOrException()) {
        break;
      }

      jni()->SetShortArrayRegion(
          static_cast<jshortArray>(ref.get()),
          index,
          1,
          &value);
      CheckJavaException();
      break;
    }

    case JVM_OPC_iastore: {
      const jint value = stack_.PopStack(Slot::Type::Int);
      const int32_t index = stack_.PopStack(Slot::Type::Int);
      JniLocalRef ref = PopModifiablePrimitiveArray("[I");
      if (IsErrorOrException()) {
        break;
      }

      jni()->SetIntArrayRegion(
          static_cast<jintArray>(ref.get()),
          index,
          1,
          &value);
      CheckJavaException();
      break;
    }

    case JVM_OPC_lastore: {
      const jlong value = stack_.PopStack2(Slot::Type::Long);
      const int32_t index = stack_.PopStack(Slot::Type::Int);
      JniLocalRef ref = PopModifiablePrimitiveArray("[J");
      if (IsErrorOrException()) {
        break;
      }

      jni()->SetLongArrayRegion(
          static_cast<jlongArray>(ref.get()),
          index,
          1,
          &value);
      CheckJavaException();
      break;
    }

    case JVM_OPC_fastore: {
      const jfloat value = as<float>(stack_.PopStack(Slot::Type::Float));
      const int32_t index = stack_.PopStack(Slot::Type::Int);
      JniLocalRef ref = PopModifiablePrimitiveArray("[F");
      if (IsErrorOrException()) {
        break;
      }

      jni()->SetFloatArrayRegion(
          static_cast<jfloatArray>(ref.get()),
          index,
          1,
          &value);
      CheckJavaException();
      break;
    }

    case JVM_OPC_dastore: {
      const jdouble value = as<double>(stack_.PopStack2(Slot::Type::Double));
      const int32_t index = stack_.PopStack(Slot::Type::Int);
      JniLocalRef ref = PopModifiablePrimitiveArray("[D");
      if (IsErrorOrException()) {
        break;
      }

      jni()->SetDoubleArrayRegion(
          static_cast<jdoubleArray>(ref.get()),
          index,
          1,
          &value);
      CheckJavaException();
      break;
    }

    case JVM_OPC_aastore: {
      JniLocalRef value = stack_.PopStackObject();
      const int32_t index = stack_.PopStack(Slot::Type::Int);
      JniLocalRef ref = stack_.PopStackObjectNonNull();
      if (IsErrorOrException()) {
        break;
      }

      if (!CheckArrayModifyAllowed(ref.get())) {
        break;
      }

      const std::string& signature = GetObjectClassSignature(ref.get());
      if ((signature.size() > 2) && (signature.front() == '[')) {
        // JVM verifies that the array element is of the right type.
        jni()->SetObjectArrayElement(
            static_cast<jobjectArray>(ref.get()),
            index,
            value.get());
        CheckJavaException();
      } else {
        SET_INTERNAL_ERROR("$0 is not an object array", signature);
      }

      break;
    }

    case JVM_OPC_baload: {
      const int32_t index = stack_.PopStack(Slot::Type::Int);
      JniLocalRef ref = stack_.PopStackObjectNonNull();
      if (IsErrorOrException()) {
        break;
      }

      const std::string& signature = GetObjectClassSignature(ref.get());
      if (signature == "[Z") {  // Boolean array.
        jboolean value = 0;
        jni()->GetBooleanArrayRegion(
            static_cast<jbooleanArray>(ref.get()),
            index,
            1,
            &value);
        if (!CheckJavaException()) {
          break;
        }

        stack_.PushStack(Slot::Type::Int, value);
      } else if (signature == "[B") {  // Byte array.
        jbyte value = 0;
        jni()->GetByteArrayRegion(
            static_cast<jbyteArray>(ref.get()),
            index,
            1,
            &value);
        if (!CheckJavaException()) {
          break;
        }

        stack_.PushStack(Slot::Type::Int, value);
      } else {
        SET_INTERNAL_ERROR("$0 is not a boolean or byte array", signature);
      }

      break;
    }

    case JVM_OPC_caload: {
      const int32_t index = stack_.PopStack(Slot::Type::Int);
      JniLocalRef ref = stack_.PopStackObjectNonNull();
      if (IsErrorOrException()) {
        break;
      }

      const std::string& signature = GetObjectClassSignature(ref.get());
      if (signature == "[C") {
        jchar value = 0;
        jni()->GetCharArrayRegion(
            static_cast<jcharArray>(ref.get()),
            index,
            1,
            &value);
        if (!CheckJavaException()) {
          break;
        }

        stack_.PushStack(Slot::Type::Int, value);
      } else {
        SET_INTERNAL_ERROR("$0 is not a char array", signature);
      }

      break;
    }

    case JVM_OPC_saload: {
      const int32_t index = stack_.PopStack(Slot::Type::Int);
      JniLocalRef ref = stack_.PopStackObjectNonNull();
      if (IsErrorOrException()) {
        break;
      }

      const std::string& signature = GetObjectClassSignature(ref.get());
      if (signature == "[S") {
        jshort value = 0;
        jni()->GetShortArrayRegion(
            static_cast<jshortArray>(ref.get()),
            index,
            1,
            &value);
        if (!CheckJavaException()) {
          break;
        }

        stack_.PushStack(Slot::Type::Int, value);
      } else {
        SET_INTERNAL_ERROR("$0 is not a short array", signature);
      }

      break;
    }

    case JVM_OPC_iaload: {
      const int32_t index = stack_.PopStack(Slot::Type::Int);
      JniLocalRef ref = stack_.PopStackObjectNonNull();
      if (IsErrorOrException()) {
        break;
      }

      const std::string& signature = GetObjectClassSignature(ref.get());
      if (signature == "[I") {
        jint value = 0;
        jni()->GetIntArrayRegion(
            static_cast<jintArray>(ref.get()),
            index,
            1,
            &value);
        if (!CheckJavaException()) {
          break;
        }

        stack_.PushStack(Slot::Type::Int, value);
      } else {
        SET_INTERNAL_ERROR("$0 is not an int array", signature);
      }

      break;
    }

    case JVM_OPC_laload: {
      const int32_t index = stack_.PopStack(Slot::Type::Int);
      JniLocalRef ref = stack_.PopStackObjectNonNull();
      if (IsErrorOrException()) {
        break;
      }

      const std::string& signature = GetObjectClassSignature(ref.get());
      if (signature == "[J") {
        jlong value = 0;
        jni()->GetLongArrayRegion(
            static_cast<jlongArray>(ref.get()),
            index,
            1,
            &value);
        if (!CheckJavaException()) {
          break;
        }

        stack_.PushStack2(Slot::Type::Long, value);
      } else {
        SET_INTERNAL_ERROR("$0 is not a long array", signature);
      }

      break;
    }

    case JVM_OPC_faload: {
      const int32_t index = stack_.PopStack(Slot::Type::Int);
      JniLocalRef ref = stack_.PopStackObjectNonNull();
      if (IsErrorOrException()) {
        break;
      }

      const std::string& signature = GetObjectClassSignature(ref.get());
      if (signature == "[F") {
        jfloat value = 0;
        jni()->GetFloatArrayRegion(
            static_cast<jfloatArray>(ref.get()),
            index,
            1,
            &value);
        if (!CheckJavaException()) {
          break;
        }

        stack_.PushStack(Slot::Type::Float, as<int32_t>(value));
      } else {
        SET_INTERNAL_ERROR("$0 is not a float array", signature);
      }

      break;
    }

    case JVM_OPC_daload: {
      const int32_t index = stack_.PopStack(Slot::Type::Int);
      JniLocalRef ref = stack_.PopStackObjectNonNull();
      if (IsErrorOrException()) {
        break;
      }

      const std::string& signature = GetObjectClassSignature(ref.get());
      if (signature == "[D") {
        jdouble value = 0;
        jni()->GetDoubleArrayRegion(
            static_cast<jdoubleArray>(ref.get()),
            index,
            1,
            &value);
        if (!CheckJavaException()) {
          break;
        }

        stack_.PushStack2(Slot::Type::Double, as<int64_t>(value));
      } else {
        SET_INTERNAL_ERROR("$0 is not a double array", signature);
      }

      break;
    }

    case JVM_OPC_aaload: {
      const int32_t index = stack_.PopStack(Slot::Type::Int);
      JniLocalRef ref = stack_.PopStackObjectNonNull();
      if (IsErrorOrException()) {
        break;
      }

      const std::string& signature = GetObjectClassSignature(ref.get());
      if ((signature.size() > 2) && (signature.front() == '[')) {
        JniLocalRef value(jni()->GetObjectArrayElement(
            static_cast<jobjectArray>(ref.get()),
            index));
        if (!CheckJavaException()) {
          break;
        }

        stack_.PushStackObject(value.get());
      } else {
        SET_INTERNAL_ERROR("$0 is not an object array", signature);
      }

      break;
    }

    case JVM_OPC_iadd:
      PrimitiveBinaryOperation(Slot::Type::Int,
                               &NanoJavaInterpreter::add<int32_t>);
      break;

    case JVM_OPC_isub:
      PrimitiveBinaryOperation(Slot::Type::Int,
                               &NanoJavaInterpreter::sub<int32_t>);
      break;

    case JVM_OPC_imul:
      PrimitiveBinaryOperation(Slot::Type::Int,
                               &NanoJavaInterpreter::mul<int32_t>);
      break;

    case JVM_OPC_idiv:
      PrimitiveBinaryOperation(
          Slot::Type::Int,
          &NanoJavaInterpreter::div_int);
      break;

    case JVM_OPC_irem:
      PrimitiveBinaryOperation(
          Slot::Type::Int,
          &NanoJavaInterpreter::modulo_int);
      break;

    case JVM_OPC_ishl:
      PrimitiveBinaryOperation(
          Slot::Type::Int,
          &NanoJavaInterpreter::ishl);
      break;

    case JVM_OPC_ishr:
      PrimitiveBinaryOperation(
          Slot::Type::Int,
          &NanoJavaInterpreter::ishr);
      break;

    case JVM_OPC_iushr:
      PrimitiveBinaryOperation(
          Slot::Type::Int,
          &NanoJavaInterpreter::iushr);
      break;

    case JVM_OPC_iand:
      PrimitiveBinaryOperation(
          Slot::Type::Int,
          &NanoJavaInterpreter::and_int);
      break;

    case JVM_OPC_ior:
      PrimitiveBinaryOperation(
          Slot::Type::Int,
          &NanoJavaInterpreter::or_int);
      break;

    case JVM_OPC_ixor:
      PrimitiveBinaryOperation(
          Slot::Type::Int,
          &NanoJavaInterpreter::xor_int);
      break;

    case JVM_OPC_fadd:
      PrimitiveBinaryOperation(
          Slot::Type::Float,
          &NanoJavaInterpreter::add<float>);
      break;

    case JVM_OPC_fsub:
      PrimitiveBinaryOperation(
          Slot::Type::Float,
          &NanoJavaInterpreter::sub<float>);
      break;

    case JVM_OPC_fmul:
      PrimitiveBinaryOperation(
          Slot::Type::Float,
          &NanoJavaInterpreter::mul<float>);
      break;

    case JVM_OPC_fdiv:
      PrimitiveBinaryOperation(
          Slot::Type::Float,
          &NanoJavaInterpreter::div_float<float>);
      break;

    case JVM_OPC_frem:
      PrimitiveBinaryOperation(
          Slot::Type::Float,
          &NanoJavaInterpreter::modulo_float<float>);
      break;

    case JVM_OPC_fcmpl: {
      float n1 = as<float>(stack_.PopStack(Slot::Type::Float));
      float n2 = as<float>(stack_.PopStack(Slot::Type::Float));
      stack_.PushStack(
          Slot::Type::Int,
          (std::isnan(n1) || std::isnan(n2)) ? -1 : ((n2 > n1) - (n2 < n1)));
      break;
    }

    case JVM_OPC_fcmpg: {
      float n1 = as<float>(stack_.PopStack(Slot::Type::Float));
      float n2 = as<float>(stack_.PopStack(Slot::Type::Float));
      stack_.PushStack(
          Slot::Type::Int,
          (std::isnan(n1) || std::isnan(n2)) ? 1 : ((n2 > n1) - (n2 < n1)));
      break;
    }

    case JVM_OPC_ladd:
      PrimitiveBinaryOperation2(Slot::Type::Long,
                                &NanoJavaInterpreter::add<int64_t>);
      break;

    case JVM_OPC_lsub:
      PrimitiveBinaryOperation2(Slot::Type::Long,
                                &NanoJavaInterpreter::sub<int64_t>);
      break;

    case JVM_OPC_lmul:
      PrimitiveBinaryOperation2(Slot::Type::Long,
                                &NanoJavaInterpreter::mul<int64_t>);
      break;

    case JVM_OPC_ldiv:
      PrimitiveBinaryOperation2(
          Slot::Type::Long,
          &NanoJavaInterpreter::div_int);
      break;

    case JVM_OPC_lrem:
      PrimitiveBinaryOperation2(
          Slot::Type::Long,
          &NanoJavaInterpreter::modulo_int);
      break;

    case JVM_OPC_lshl: {
      int32_t n1 = stack_.PopStack(Slot::Type::Int) & 0x3F;
      int64_t n2 = stack_.PopStack2(Slot::Type::Long);
      stack_.PushStack2(Slot::Type::Long, n2 << n1);
      break;
    }

    case JVM_OPC_lshr: {
      int32_t n1 = stack_.PopStack(Slot::Type::Int) & 0x3F;
      int64_t n2 = stack_.PopStack2(Slot::Type::Long);
      stack_.PushStack2(Slot::Type::Long, n2 >> n1);
      break;
    }

    case JVM_OPC_lushr: {
      int32_t n1 = stack_.PopStack(Slot::Type::Int) & 0x3F;
      uint64_t n2 = static_cast<uint64_t>(stack_.PopStack2(Slot::Type::Long));
      stack_.PushStack2(Slot::Type::Long, as<int64_t>(n2 >> n1));
      break;
    }

    case JVM_OPC_land:
      PrimitiveBinaryOperation2(
          Slot::Type::Long,
          &NanoJavaInterpreter::and_int);
      break;

    case JVM_OPC_lor:
      PrimitiveBinaryOperation2(
          Slot::Type::Long,
          &NanoJavaInterpreter::or_int);
      break;

    case JVM_OPC_lxor:
      PrimitiveBinaryOperation2(
          Slot::Type::Long,
          &NanoJavaInterpreter::xor_int);
      break;

    case JVM_OPC_lcmp: {
      int64_t n1 = stack_.PopStack2(Slot::Type::Long);
      int64_t n2 = stack_.PopStack2(Slot::Type::Long);
      stack_.PushStack(Slot::Type::Int, (n2 > n1) - (n2 < n1));
      break;
    }

    case JVM_OPC_dadd:
      PrimitiveBinaryOperation2(
          Slot::Type::Double,
          &NanoJavaInterpreter::add<double>);
      break;

    case JVM_OPC_dsub:
      PrimitiveBinaryOperation2(
          Slot::Type::Double,
          &NanoJavaInterpreter::sub<double>);
      break;

    case JVM_OPC_dmul:
      PrimitiveBinaryOperation2(
          Slot::Type::Double,
          &NanoJavaInterpreter::mul<double>);
      break;

    case JVM_OPC_ddiv:
      PrimitiveBinaryOperation2(
          Slot::Type::Double,
          &NanoJavaInterpreter::div_float<double>);
      break;

    case JVM_OPC_drem:
      PrimitiveBinaryOperation2(
          Slot::Type::Double,
          &NanoJavaInterpreter::modulo_float<double>);
      break;

    case JVM_OPC_ineg:
      stack_.PushStack(
          Slot::Type::Int,
          -stack_.PopStack(Slot::Type::Int));
      break;

    case JVM_OPC_lneg:
      stack_.PushStack2(
          Slot::Type::Long,
          -stack_.PopStack2(Slot::Type::Long));
      break;

    case JVM_OPC_fneg:
      stack_.PushStack(
          Slot::Type::Float,
          as<int32_t>(-as<float>(stack_.PopStack(Slot::Type::Float))));
      break;

    case JVM_OPC_dneg:
      stack_.PushStack2(
          Slot::Type::Double,
          as<int64_t>(-as<double>(stack_.PopStack2(Slot::Type::Double))));
      break;

    case JVM_OPC_dcmpl: {
      double n1 = as<double>(stack_.PopStack2(Slot::Type::Double));
      double n2 = as<double>(stack_.PopStack2(Slot::Type::Double));
      stack_.PushStack(
          Slot::Type::Int,
          (std::isnan(n1) || std::isnan(n2)) ? -1 : ((n2 > n1) - (n2 < n1)));
      break;
    }

    case JVM_OPC_dcmpg: {
      double n1 = as<double>(stack_.PopStack2(Slot::Type::Double));
      double n2 = as<double>(stack_.PopStack2(Slot::Type::Double));
      stack_.PushStack(
          Slot::Type::Int,
          (std::isnan(n1) || std::isnan(n2)) ? 1 : ((n2 > n1) - (n2 < n1)));
      break;
    }

    case JVM_OPC_ireturn:
      ReturnOperation(JType::Int);
      break;

    case JVM_OPC_freturn:
      ReturnOperation(JType::Float);
      break;

    case JVM_OPC_lreturn:
      ReturnOperation(JType::Long);
      break;

    case JVM_OPC_dreturn:
      ReturnOperation(JType::Double);
      break;

    case JVM_OPC_areturn:
      ReturnOperation(JType::Object);
      break;

    case JVM_OPC_return:
      ReturnOperation(JType::Void);
      break;

    case JVM_OPC_ldc:
      LdcOperation(instruction.int_operand);
      break;

    case JVM_OPC_dup:
      stack_.StackDup();
      break;

    case JVM_OPC_dup_x1:
      stack_.StackDup();
      stack_.Swap(2, 3);
      break;

    case JVM_OPC_dup_x2:
      stack_.StackDup();
      stack_.Swap(2, 4);
      break;

    case JVM_OPC_dup2:
      stack_.StackDup2();
      break;

    case JVM_OPC_dup2_x1:
      stack_.StackDup2();
      stack_.Swap(3, 5);
      stack_.Swap(4, 5);
      break;

    case JVM_OPC_dup2_x2:
      stack_.StackDup2();
      stack_.Swap(3, 5);
      stack_.Swap(4, 6);
      break;

    case JVM_OPC_pop:
      stack_.Discard();
      break;

    case JVM_OPC_pop2:
      stack_.Discard();
      stack_.Discard();
      break;

    case JVM_OPC_swap:
      stack_.Swap(1, 2);
      break;

    case JVM_OPC_i2l:
      stack_.PushStack2(Slot::Type::Long, stack_.PopStack(Slot::Type::Int));
      break;

    case JVM_OPC_i2f:
      stack_.PushStack(
          Slot::Type::Float,
          as<int32_t>(static_cast<float>(stack_.PopStack(Slot::Type::Int))));
      break;

    case JVM_OPC_i2d:
      stack_.PushStack2(
          Slot::Type::Double,
          as<int64_t>(static_cast<double>(stack_.PopStack(Slot::Type::Int))));
      break;

    case JVM_OPC_l2i:
      stack_.PushStack(
          Slot::Type::Int,
          static_cast<int32_t>(stack_.PopStack2(Slot::Type::Long)));
      break;

    case JVM_OPC_l2f:
      stack_.PushStack(
          Slot::Type::Float,
          as<int32_t>(static_cast<float>(stack_.PopStack2(Slot::Type::Long))));
      break;

    case JVM_OPC_l2d:
      stack_.PushStack2(
          Slot::Type::Double,
          as<int64_t>(static_cast<double>(stack_.PopStack2(Slot::Type::Long))));
      break;

    case JVM_OPC_f2i:
      stack_.PushStack(
          Slot::Type::Int,
          static_cast<int32_t>(as<float>(stack_.PopStack(Slot::Type::Float))));
      break;

    case JVM_OPC_f2l:
      stack_.PushStack2(
          Slot::Type::Long,
          static_cast<int64_t>(as<float>(stack_.PopStack(Slot::Type::Float))));
      break;

    case JVM_OPC_f2d:
      stack_.PushStack2(Slot::Type::Double,
                        as<int64_t>(static_cast<double>(
                            as<float>(stack_.PopStack(Slot::Type::Float)))));
      break;

    case JVM_OPC_d2i:
      stack_.PushStack(Slot::Type::Int,
                       static_cast<int32_t>(
                           as<double>(stack_.PopStack2(Slot::Type::Double))));
      break;

    case JVM_OPC_d2l:
      stack_.PushStack2(Slot::Type::Long,
                        static_cast<int64_t>(
                            as<double>(stack_.PopStack2(Slot::Type::Double))));
      break;

    case JVM_OPC_d2f:
      stack_.PushStack(Slot::Type::Float,
                       as<int32_t>(static_cast<float>(
                           as<double>(stack_.PopStack2(Slot::Type::Double)))));
      break;

    case JVM_OPC_i2b:
      stack_.PushStack(
          Slot::Type::Int,
          static_cast<jbyte>(stack_.PopStack(Slot::Type::Int)));
      break;

    case JVM_OPC_i2c:
      stack_.PushStack(
          Slot::Type::Int,
          static_cast<jchar>(stack_.PopStack(Slot::Type::Int)));
      break;

    case JVM_OPC_i2s:
      stack_.PushStack(
          Slot::Type::Int,
          static_cast<jshort>(stack_.PopStack(Slot::Type::Int)));
      break;

    case JVM_OPC_invokevirtual:
    case JVM_OPC_invokespecial:
    case JVM_OPC_invokestatic:
    case JVM_OPC_invokeinterface:
      InvokeOperation(instruction.opcode, *instruction.method_operand);
      break;

    case JVM_OPC_new: {
      jclass cls = LoadClass(instruction.type_operand->type.get());
      if (cls == nullptr) {  // Error occurred
        DCHECK(IsError());
        break;
      }

      JniLocalRef ref(jni()->AllocObject(cls));
      if (!CheckJavaException()) {
        break;
      }

      supervisor_->NewObjectAllocated(ref.get());

      stack_.PushStackObject(ref.get());
      break;
    }

    case JVM_OPC_newarray:
      NewArrayOperation(instruction.int_operand);
      break;

    case JVM_OPC_anewarray: {
      int32_t count = stack_.PopStack(Slot::Type::Int);
      if (count < 0) {
        RaiseException(jniproxy::NegativeArraySizeException());
        break;
      }

      if (!CheckNewArrayAllowed(count)) {
        break;
      }

      jclass cls = LoadClass(instruction.type_operand->type.get());
      if (cls == nullptr) {
        DCHECK(IsError());
        break;
      }

      JniLocalRef ref(jni()->NewObjectArray(count, cls, nullptr));
      if (!CheckJavaException()) {
        break;
      }

      if (ref == nullptr) {
        SET_INTERNAL_ERROR(
            "failed to allocate new object array, length = $0",
            std::to_string(count));
        break;
      }

      supervisor_->NewObjectAllocated(ref.get());

      stack_.PushStackObject(ref.get());
      break;
    }

    case JVM_OPC_instanceof: {
      JniLocalRef obj = stack_.PopStackObject();
      if (obj == nullptr) {
        stack_.PushStack(Slot::Type::Int, 0);
        break;
      }

      jclass cls = LoadClass(instruction.type_operand->type.get());
      if (cls == nullptr) {
        DCHECK(IsError());
        break;
      }

      stack_.PushStack(Slot::Type::Int, jni()->IsInstanceOf(obj.get(), cls));
      break;
    }

    case JVM_OPC_checkcast: {
      jobject obj = stack_.PeekStackObject();
      if (obj == nullptr) {
        break;
      }

      jclass cls = LoadClass(instruction.type_operand->type.get());
      if (cls == nullptr) {
        DCHECK(IsError());
        break;
      }

      if (!jni()->IsInstanceOf(obj, cls)) {
        RaiseException(jniproxy::ClassCastException());
      }
      break;
    }

    case JVM_OPC_arraylength: {
      JniLocalRef ref = stack_.PopStackObjectNonNull();
      if (ref == nullptr) {
        break;
      }

      const std::string& signature = GetObjectClassSignature(ref.get());
      if (!IsArrayObjectSignature(signature)) {
        SET_INTERNAL_ERROR("$0 is not an array type", signature);
        break;
      }

      stack_.PushStack(
          Slot::Type::Int,
          jni()->GetArrayLength(static_cast<jarray>(ref.get())));
      break;
    }

    case JVM_OPC_iinc: {
      const int32_t n = locals_.GetLocal(instruction.iinc_operand.local_index,
                                         Slot::Type::Int);
      locals_.SetLocal(
          instruction.iinc_operand.local_index,
          Slot::Type::Int,
          n + instruction.iinc_operand.increment);
      break;
    }

    case JVM_OPC_ifeq:
      if (stack_.PopStack(Slot::Type::Int) == 0) {
        next_ip = instruction.offset + instruction.int_operand;
      }
      break;

    case JVM_OPC_ifne:
      if (stack_.PopStack(Slot::Type::Int) != 0) {
        next_ip = instruction.offset + instruction.int_operand;
      }
      break;

    case JVM_OPC_iflt:
      if (stack_.PopStack(Slot::Type::Int) < 0) {
        next_ip = instruction.offset + instruction.int_operand;
      }
      break;

    case JVM_OPC_ifle:
      if (stack_.PopStack(Slot::Type::Int) <= 0) {
        next_ip = instruction.offset + instruction.int_operand;
      }
      break;

    case JVM_OPC_ifgt:
      if (stack_.PopStack(Slot::Type::Int) > 0) {
        next_ip = instruction.offset + instruction.int_operand;
      }
      break;

    case JVM_OPC_ifge:
      if (stack_.PopStack(Slot::Type::Int) >= 0) {
        next_ip = instruction.offset + instruction.int_operand;
      }
      break;

    case JVM_OPC_if_icmpeq:
      if (IfICmpOperation<std::equal_to<int32_t>>()) {
        next_ip = instruction.offset + instruction.int_operand;
      }
      break;

    case JVM_OPC_if_icmpne:
      if (IfICmpOperation<std::not_equal_to<int32_t>>()) {
        next_ip = instruction.offset + instruction.int_operand;
      }
      break;

    case JVM_OPC_if_icmplt:
      if (IfICmpOperation<std::less<int32_t>>()) {
        next_ip = instruction.offset + instruction.int_operand;
      }
      break;

    case JVM_OPC_if_icmple:
      if (IfICmpOperation<std::less_equal<int32_t>>()) {
        next_ip = instruction.offset + instruction.int_operand;
      }
      break;

    case JVM_OPC_if_icmpgt:
      if (IfICmpOperation<std::greater<int32_t>>()) {
        next_ip = instruction.offset + instruction.int_operand;
      }
      break;

    case JVM_OPC_if_icmpge:
      if (IfICmpOperation<std::greater_equal<int32_t>>()) {
        next_ip = instruction.offset + instruction.int_operand;
      }
      break;

    case JVM_OPC_if_acmpeq: {
      JniLocalRef ref2 = stack_.PopStackObject();
      JniLocalRef ref1 = stack_.PopStackObject();
      if (jni()->IsSameObject(ref1.get(), ref2.get())) {
        next_ip = instruction.offset + instruction.int_operand;
      }

      break;
    }

    case JVM_OPC_if_acmpne: {
      JniLocalRef ref2 = stack_.PopStackObject();
      JniLocalRef ref1 = stack_.PopStackObject();
      if (!jni()->IsSameObject(ref1.get(), ref2.get())) {
        next_ip = instruction.offset + instruction.int_operand;
      }

      break;
    }

    case JVM_OPC_ifnull:
      if (stack_.PopStackObject() == nullptr) {
        next_ip = instruction.offset + instruction.int_operand;
      }
      break;

    case JVM_OPC_ifnonnull:
      if (stack_.PopStackObject() != nullptr) {
        next_ip = instruction.offset + instruction.int_operand;
      }
      break;

    case JVM_OPC_goto:
      next_ip = instruction.offset + instruction.int_operand;
      break;

    case JVM_OPC_getstatic:
      CheckFieldFound(*instruction.field_operand);
      if (!IsError()) {
        GetStaticFieldOperation(*instruction.field_operand);
      }
      break;

    case JVM_OPC_putstatic:
      CheckFieldFound(*instruction.field_operand);
      if (!IsError()) {
        // The interpreter does not support PUTSTATIC instructions at all. We
        // first call "CheckFieldModifyAllowed" to let the supervisor fail the
        // execution with a nice error message. If it doesn't we fall back to
        // the default "opcode not supported" error message.
        CheckFieldModifyAllowed(nullptr, *instruction.field_operand);
        SetOpcodeNotSupportedError("PUTSTATIC");
      }
      break;

    case JVM_OPC_getfield:
      CheckFieldFound(*instruction.field_operand);
      if (!IsError()) {
        GetInstanceFieldOperation(*instruction.field_operand);
      }
      break;

    case JVM_OPC_putfield:
      CheckFieldFound(*instruction.field_operand);
      if (!IsError()) {
        SetInstanceFieldOperation(*instruction.field_operand);
      }
      break;

    case JVM_OPC_athrow: {
      JniLocalRef exception = stack_.PopStackObjectNonNull();
      if (exception != nullptr) {
        SetResult(MethodCallResult::JavaException(exception.get()));
      }
      break;
    }

    // Locks pose a special threat for safe caller. It can just take a long
    // time, it can deadlock or it can cause deadlock. Unfortunately Java
    // doesn't have a simple way to wait with a small timeout on monitors.
    // The solution that NanoJava implements is to ignore all monitor related
    // opcodes in the interpreted code. Since we only read data, having no
    // locks will do no damage. The implication is that we may get incorrect
    // data when reading complex data structures like sets.
    case JVM_OPC_monitorenter:
    case JVM_OPC_monitorexit:
      stack_.PopStackObjectNonNull();
      break;

    case JVM_OPC_multianewarray:
      SetOpcodeNotSupportedError("MULTIANEWARRAY");
      break;

    case JVM_OPC_invokedynamic:
      SetOpcodeNotSupportedError("INVOKEDYNAMIC");
      break;

    case JVM_OPC_jsr:
      SetOpcodeNotSupportedError("JSR");
      break;

    case JVM_OPC_ret:
      SetOpcodeNotSupportedError("RET");
      break;

    case JVM_OPC_tableswitch: {
      const int32_t index = stack_.PopStack(Slot::Type::Int) -
                            instruction.table_switch_operand.low;

      if ((index >= 0) &&
          (index < instruction.table_switch_operand.table.size())) {
        ClassFile::TableSwitchTable table =
            instruction.table_switch_operand.table;
        next_ip = instruction.offset + table.offset(index);
        if (table.is_error()) {
          SET_INTERNAL_ERROR("Bad tableswitch table");
          break;
        }
      } else {
        next_ip = instruction.offset +
                  instruction.table_switch_operand.default_handler_offset;
      }

      break;
    }

    case JVM_OPC_lookupswitch: {
      const int32_t key = stack_.PopStack(Slot::Type::Int);

      next_ip = instruction.offset +
                instruction.lookup_switch_operand.default_handler_offset;
      ClassFile::LookupSwitchTable table =
          instruction.lookup_switch_operand.table;
      for (int i = 0; i < table.size(); ++i) {
        if (table.value(i) == key) {
          next_ip = instruction.offset + table.offset(i);
          break;
        }
      }

      if (table.is_error()) {
        SET_INTERNAL_ERROR("Bad lookupswitch table");
        break;
      }

      break;
    }

    default:
      SetOpcodeNotSupportedError(std::to_string(instruction.opcode));
      break;
  }  // End of switch statement.

  return next_ip;
}  // NOLINT(readability/fn_size)


jclass NanoJavaInterpreter::LoadClass(
    ClassIndexer::Type* class_reference) {
  jclass cls = class_reference->FindClass();
  if (cls != nullptr) {
    return cls;
  }

  DLOG(INFO) << "Class " << class_reference->GetSignature()
             << " not loaded, call stack:\n" << FormatCallStack();

  FormatMessageModel error_message {
      ClassNotLoaded,
      {
        TypeNameFromSignature(
            { JType::Object, class_reference->GetSignature() }),
        class_reference->GetSignature()
      }
  };

  SetResult(MethodCallResult::Error(error_message));
  return nullptr;
}

void NanoJavaInterpreter::SetOpcodeNotSupportedError(std::string opcode) {
  SetResult(MethodCallResult::Error(FormatMessageModel
      { OpcodeNotSupported, { method_name(), std::move(opcode) } }));
}

bool NanoJavaInterpreter::DispatchExceptionHandler() {
  if ((result_ == nullptr) ||
      (result_->result_type() != MethodCallResult::Type::JavaException)) {
    return false;
  }

  jobject exception = result_->exception();
  if (exception == nullptr) {
    SET_INTERNAL_ERROR("no pending exception");
    return false;
  }

  // Loop through the exception table. The first row to match is the exception
  // handler to branch to. Each row in exception table has a code range and
  // optional type. The row matches if the current instruction pointer is
  // within the code range and the thrown exception is an instance of the
  // type in the exception table.
  for (int i = 0; i < method_->GetExceptionTableSize(); ++i) {
    Nullable<ClassFile::TryCatchBlock> nullable_block =
        method_->GetTryCatchBlock(i);
    if (!nullable_block.has_value()) {
      SET_INTERNAL_ERROR("Failed to read trycatch block $0", std::to_string(i));
      return false;
    }

    const ClassFile::TryCatchBlock& block = nullable_block.value();

    if ((ip_ < block.begin_offset) || (ip_ >= block.end_offset)) {
      continue;
    }

    // The exception was thrown from the location matching current try...catch
    // block range. If this is a "finally" block, we are done. Otherwise we
    // need to check if the thrown exception is is an instance of "block.type"
    // class.
    if (block.type != nullptr) {
      jclass cls = LoadClass(block.type->type.get());
      if (cls == nullptr) {
        return false;
      }

      if (!jni()->IsInstanceOf(exception, cls)) {
        continue;
      }
    }

    stack_.PushStackObject(exception);
    if (IsError()) {  // "PushStackObject" failed (stack overflow).
      return false;
    }

    ip_ = block.handler_offset;
    result_ = nullptr;
    return true;  // Exception caught.
  }

  return false;
}


void NanoJavaInterpreter::SetResult(MethodCallResult result) {
  // Preserve first error that occurred.
  if (IsError()) {
    return;
  }

  result_.reset(new MethodCallResult(std::move(result)));
}


// This template method is only used within this module, so we don't have to
// define it in a header file.
template <typename T>
void NanoJavaInterpreter::RaiseException(T* proxy_class) {
  ExceptionOr<JniLocalRef> ref = proxy_class->NewObject();
  if (ref.HasException()) {
    // We failed to allocate the new exception object. This is highly unlikely
    // to ever happen, and it's most likely caused by out of memory condition.
    // Use the exception object thrown when allocating a new exception class
    // instead.
    ref.LogException();
    SetResult(MethodCallResult::JavaException(ref.GetException()));
    return;
  }

  supervisor_->NewObjectAllocated(ref.GetData().get());
  SetResult(MethodCallResult::JavaException(ref.GetData().get()));
}


void NanoJavaInterpreter::RaiseNullPointerException() {
  RaiseException(jniproxy::NullPointerException());
}


JniLocalRef NanoJavaInterpreter::PopModifiablePrimitiveArray(
    const char* array_signature) {
  JniLocalRef ref = stack_.PopStackObjectNonNull();
  if (IsErrorOrException()) {
    return nullptr;
  }

  if (!CheckArrayModifyAllowed(ref.get())) {
    return nullptr;
  }

  const std::string& signature = GetObjectClassSignature(ref.get());
  if (signature != array_signature) {
    SET_INTERNAL_ERROR(
        "$0 is not a primitive array $1",
        signature,
        array_signature);
  }

  return ref;
}


bool NanoJavaInterpreter::CheckNextInstructionAllowed() {
  std::unique_ptr<FormatMessageModel> error_message =
      supervisor_->IsNextInstructionAllowed();
  if (error_message != nullptr) {
    SetResult(MethodCallResult::Error(std::move(*error_message)));
    return false;
  }

  return true;
}

bool NanoJavaInterpreter::CheckNewArrayAllowed(int32_t count) {
  std::unique_ptr<FormatMessageModel> error_message =
      supervisor_->IsNewArrayAllowed(count);
  if (error_message != nullptr) {
    SetResult(MethodCallResult::Error(std::move(*error_message)));
    return false;
  }

  return true;
}

bool NanoJavaInterpreter::CheckArrayModifyAllowed(jobject array) {
  std::unique_ptr<FormatMessageModel> error_message =
      supervisor_->IsArrayModifyAllowed(array);
  if (error_message != nullptr) {
    SetResult(MethodCallResult::Error(std::move(*error_message)));
    return false;
  }

  return true;
}


bool NanoJavaInterpreter::CheckFieldModifyAllowed(
    jobject target,
    const ConstantPool::FieldRef& field) {
  std::unique_ptr<FormatMessageModel> error_message =
      supervisor_->IsFieldModifyAllowed(target, field);
  if (error_message != nullptr) {
    SetResult(MethodCallResult::Error(std::move(*error_message)));
    return false;
  }

  return true;
}


bool NanoJavaInterpreter::CheckJavaException() {
  if (!jni()->ExceptionCheck()) {
    return true;  // No pending exception.
  }

  SetResult(MethodCallResult::PendingJniException());
  return false;
}


void NanoJavaInterpreter::LdcOperation(int constant_pool_index) {
  ConstantPool* constant_pool = method_->class_file()->constant_pool();

  const uint8_t type = constant_pool->GetType(constant_pool_index);
  switch (type) {
    case JVM_CONSTANT_Integer: {
      Nullable<jint> value = constant_pool->GetInteger(constant_pool_index);
      if (!value.has_value()) {
        SET_INTERNAL_ERROR(
            "integer value not available in constant pool item $0",
            std::to_string(constant_pool_index));
        return;
      }

      stack_.PushStack(Slot::Type::Int, value.value());
      return;
    }

    case JVM_CONSTANT_Float: {
      Nullable<jfloat> value = constant_pool->GetFloat(constant_pool_index);
      if (!value.has_value()) {
        SET_INTERNAL_ERROR(
            "float value not available in constant pool item $0",
            std::to_string(constant_pool_index));
        return;
      }

      stack_.PushStack(Slot::Type::Float, as<int32_t>(value.value()));
      return;
    }

    case JVM_CONSTANT_Long: {
      Nullable<jlong> value = constant_pool->GetLong(constant_pool_index);
      if (value == nullptr) {
        SET_INTERNAL_ERROR(
            "long value not available in constant pool item $0",
            std::to_string(constant_pool_index));
        return;
      }

      stack_.PushStack2(Slot::Type::Long, value.value());
      return;
    }

    case JVM_CONSTANT_Double: {
      Nullable<jdouble> value = constant_pool->GetDouble(constant_pool_index);
      if (value == nullptr) {
        SET_INTERNAL_ERROR(
            "double value not available in constant pool item $0",
            std::to_string(constant_pool_index));
        return;
      }

      stack_.PushStack2(Slot::Type::Double, as<int64_t>(value.value()));
      return;
    }

    case JVM_CONSTANT_String: {
      const ConstantPool::StringRef* value =
          constant_pool->GetString(constant_pool_index);
      if (value == nullptr) {
        SET_INTERNAL_ERROR(
            "string value not available in constant pool item $0",
            std::to_string(constant_pool_index));
        return;
      }

      stack_.PushStackObject(value->str.get());
      return;
    }

    case JVM_CONSTANT_Class: {
      const ConstantPool::ClassRef* value =
          constant_pool->GetClass(constant_pool_index);
      if (value == nullptr) {
        SET_INTERNAL_ERROR(
            "class value not available in constant pool item $0",
            std::to_string(constant_pool_index));
        return;
      }

      jclass cls = LoadClass(value->type.get());
      if (cls == nullptr) {
        return;
      }

      stack_.PushStackObject(cls);
      return;
    }
  }

  SET_INTERNAL_ERROR(
      "unsupported constant pool item $0 for LDC instruction",
      std::to_string(static_cast<int>(type)));
}

void NanoJavaInterpreter::InvokeOperation(
    uint8_t opcode, const ConstantPool::MethodRef& operand) {
  if (!operand.is_found) {
    SetResult(MethodCallResult::Error({
        ClassNotLoaded,
        {
          TypeNameFromJObjectSignature(operand.owner->type->GetSignature()),
          operand.owner->type->GetSignature()
        }
    }));

    return;
  }

  std::vector<JVariant> arguments(operand.method_signature.arguments.size());
  auto it_arguments = arguments.rbegin();
  for (auto it = operand.method_signature.arguments.rbegin();
       it != operand.method_signature.arguments.rend();
       ++it, ++it_arguments) {
    *it_arguments = stack_.PopStackAny(it->type);
  }

  JniLocalRef instance;
  if (opcode != JVM_OPC_invokestatic) {
    instance = stack_.PopStackObject();
  }

  if (IsError()) {
    return;
  }

  MethodCallResult rc = supervisor_->InvokeNested(
      opcode == JVM_OPC_invokespecial,
      operand,
      instance.get(),
      arguments);

  if (rc.result_type() != MethodCallResult::Type::Success) {
    SetResult(rc);
  } else {
    stack_.PushStackAny(rc.return_value());
  }
}

void NanoJavaInterpreter::CheckFieldFound(const ConstantPool::FieldRef& field) {
  if (!field.is_found) {
    SetResult(MethodCallResult::Error({
        ClassNotLoaded,
        {
          TypeNameFromJObjectSignature(field.owner->type->GetSignature()),
          field.owner->type->GetSignature()
        }
    }));
  }
}


void NanoJavaInterpreter::GetStaticFieldOperation(
    const ConstantPool::FieldRef& operand) {
  DCHECK(operand.is_static.value());

  const jclass cls = static_cast<jclass>(operand.owner_cls.get());
  const jfieldID field_id = operand.field_id;

  switch (operand.field_type->GetType()) {
    case JType::Void:
      SET_INTERNAL_ERROR("void field type unexpected");
      return;

    case JType::Boolean:
      stack_.PushStack(
          Slot::Type::Int,
          static_cast<jint>(jni()->GetStaticBooleanField(cls, field_id)));
      return;

    case JType::Byte:
      stack_.PushStack(
          Slot::Type::Int,
          static_cast<jint>(jni()->GetStaticByteField(cls, field_id)));
      return;

    case JType::Char:
      stack_.PushStack(
          Slot::Type::Int,
          static_cast<jint>(jni()->GetStaticCharField(cls, field_id)));
      return;

    case JType::Short:
      stack_.PushStack(
          Slot::Type::Int,
          static_cast<jint>(jni()->GetStaticShortField(cls, field_id)));
      return;

    case JType::Int:
      stack_.PushStack(
          Slot::Type::Int,
          jni()->GetStaticIntField(cls, field_id));
      return;

    case JType::Long:
      stack_.PushStack2(
          Slot::Type::Long,
          jni()->GetStaticLongField(cls, field_id));
      return;

    case JType::Float:
      stack_.PushStack(Slot::Type::Float,
                       as<int32_t>(jni()->GetStaticFloatField(cls, field_id)));
      return;

    case JType::Double:
      stack_.PushStack2(
          Slot::Type::Double,
          as<int64_t>(jni()->GetStaticDoubleField(cls, field_id)));
      return;

    case JType::Object: {
      JniLocalRef ref(jni()->GetStaticObjectField(cls, field_id));
      stack_.PushStackObject(ref.get());
      return;
    }
  }

  SET_INTERNAL_ERROR(
      "bad type $0",
      std::to_string(static_cast<int>(operand.field_type->GetType())));
}


void NanoJavaInterpreter::GetInstanceFieldOperation(
    const ConstantPool::FieldRef& operand) {
  DCHECK(!operand.is_static.value());

  JniLocalRef instance = stack_.PopStackObjectInstanceOf(
      static_cast<jclass>(operand.owner_cls.get()));
  if (IsErrorOrException()) {
    return;
  }

  const jfieldID field_id = operand.field_id;

  switch (operand.field_type->GetType()) {
    case JType::Void:
      SET_INTERNAL_ERROR("void field type unexpected");
      return;

    case JType::Boolean:
      stack_.PushStack(
          Slot::Type::Int,
          static_cast<jint>(jni()->GetBooleanField(instance.get(), field_id)));
      return;

    case JType::Byte:
      stack_.PushStack(
          Slot::Type::Int,
          static_cast<jint>(jni()->GetByteField(instance.get(), field_id)));
      return;

    case JType::Char:
      stack_.PushStack(
          Slot::Type::Int,
          static_cast<jint>(jni()->GetCharField(instance.get(), field_id)));
      return;

    case JType::Short:
      stack_.PushStack(
          Slot::Type::Int,
          static_cast<jint>(jni()->GetShortField(instance.get(), field_id)));
      return;

    case JType::Int:
      stack_.PushStack(
          Slot::Type::Int,
          jni()->GetIntField(instance.get(), field_id));
      return;

    case JType::Long:
      stack_.PushStack2(
          Slot::Type::Long,
          jni()->GetLongField(instance.get(), field_id));
      return;

    case JType::Float:
      stack_.PushStack(Slot::Type::Float, as<int32_t>(jni()->GetFloatField(
                                              instance.get(), field_id)));
      return;

    case JType::Double:
      stack_.PushStack2(Slot::Type::Double, as<int64_t>(jni()->GetDoubleField(
                                                instance.get(), field_id)));
      return;

    case JType::Object: {
      JniLocalRef ref(jni()->GetObjectField(instance.get(), field_id));
      stack_.PushStackObject(ref.get());
      return;
    }
  }

  SET_INTERNAL_ERROR(
      "bad type $0",
      std::to_string(static_cast<int>(operand.field_type->GetType())));
}


void NanoJavaInterpreter::SetInstanceFieldOperation(
    const ConstantPool::FieldRef& operand) {
  DCHECK(!operand.is_static.value());

  const jclass cls = static_cast<jclass>(operand.owner_cls.get());
  const jfieldID field_id = operand.field_id;

  switch (operand.field_type->GetType()) {
    case JType::Void:
      SET_INTERNAL_ERROR("void field type unexpected");
      return;

    case JType::Boolean: {
      jboolean value = static_cast<jboolean>(stack_.PopStack(Slot::Type::Int));
      JniLocalRef instance = stack_.PopStackObjectInstanceOf(cls);
      if (!IsErrorOrException() &&
          CheckFieldModifyAllowed(instance.get(), operand)) {
        jni()->SetBooleanField(instance.get(), field_id, value);
      }
      return;
    }

    case JType::Byte: {
      jbyte value = static_cast<jbyte>(stack_.PopStack(Slot::Type::Int));
      JniLocalRef instance = stack_.PopStackObjectInstanceOf(cls);
      if (!IsErrorOrException() &&
          CheckFieldModifyAllowed(instance.get(), operand)) {
        jni()->SetByteField(instance.get(), field_id, value);
      }
      return;
    }

    case JType::Char: {
      jchar value = static_cast<jchar>(stack_.PopStack(Slot::Type::Int));
      JniLocalRef instance = stack_.PopStackObjectInstanceOf(cls);
      if (!IsErrorOrException() &&
          CheckFieldModifyAllowed(instance.get(), operand)) {
        jni()->SetCharField(instance.get(), field_id, value);
      }
      return;
    }

    case JType::Short: {
      jshort value = static_cast<jshort>(stack_.PopStack(Slot::Type::Int));
      JniLocalRef instance = stack_.PopStackObjectInstanceOf(cls);
      if (!IsErrorOrException() &&
          CheckFieldModifyAllowed(instance.get(), operand)) {
        jni()->SetShortField(instance.get(), field_id, value);
      }
      return;
    }

    case JType::Int: {
      jint value = stack_.PopStack(Slot::Type::Int);
      JniLocalRef instance = stack_.PopStackObjectInstanceOf(cls);
      if (!IsErrorOrException() &&
          CheckFieldModifyAllowed(instance.get(), operand)) {
        jni()->SetIntField(instance.get(), field_id, value);
      }
      return;
    }

    case JType::Long: {
      jlong value = stack_.PopStack2(Slot::Type::Long);
      JniLocalRef instance = stack_.PopStackObjectInstanceOf(cls);
      if (!IsErrorOrException() &&
          CheckFieldModifyAllowed(instance.get(), operand)) {
        jni()->SetLongField(instance.get(), field_id, value);
      }
      return;
    }

    case JType::Float: {
      jfloat value = as<float>(stack_.PopStack(Slot::Type::Float));
      JniLocalRef instance = stack_.PopStackObjectInstanceOf(cls);
      if (!IsErrorOrException() &&
          CheckFieldModifyAllowed(instance.get(), operand)) {
        jni()->SetFloatField(instance.get(), field_id, value);
      }
      return;
    }

    case JType::Double: {
      jdouble value = as<double>(stack_.PopStack2(Slot::Type::Double));
      JniLocalRef instance = stack_.PopStackObjectInstanceOf(cls);
      if (!IsErrorOrException() &&
          CheckFieldModifyAllowed(instance.get(), operand)) {
        jni()->SetDoubleField(instance.get(), field_id, value);
      }
      return;
    }

    case JType::Object: {
      JniLocalRef value = stack_.PopStackObject();
      if (value != nullptr) {
        jclass field_cls = operand.field_type->FindClass();
        if (field_cls == nullptr) {
          SET_INTERNAL_ERROR(
              "Field class not found: $0",
              operand.field_type->GetSignature());
          return;
        }

        if (!jni()->IsInstanceOf(value.get(), field_cls)) {
          SET_INTERNAL_ERROR(
              "new value ($0) is not an instance of $1",
              TypeNameFromSignature(
                  { JType::Object, GetObjectClassSignature(value.get()) }),
              TypeNameFromSignature(
                  { JType::Object, GetClassSignature(field_cls) }));
          return;
        }
      }

      JniLocalRef instance = stack_.PopStackObjectInstanceOf(cls);
      if (IsErrorOrException() ||
          !CheckFieldModifyAllowed(instance.get(), operand)) {
        return;
      }

      jni()->SetObjectField(instance.get(), field_id, value.get());
      return;
    }
  }

  SET_INTERNAL_ERROR(
      "bad type $0",
      std::to_string(static_cast<int>(operand.field_type->GetType())));
}


void NanoJavaInterpreter::NewArrayOperation(int array_type) {
  int32_t count = stack_.PopStack(Slot::Type::Int);
  if (count < 0) {
    RaiseException(jniproxy::NegativeArraySizeException());
    return;
  }

  if (!CheckNewArrayAllowed(count)) {
    return;
  }

  JniLocalRef ref;
  switch (array_type) {
    case JVM_T_BOOLEAN:
      ref.reset(jni()->NewBooleanArray(count));
      break;

    case JVM_T_BYTE:
      ref.reset(jni()->NewByteArray(count));
      break;

    case JVM_T_CHAR:
      ref.reset(jni()->NewCharArray(count));
      break;

    case JVM_T_SHORT:
      ref.reset(jni()->NewShortArray(count));
      break;

    case JVM_T_INT:
      ref.reset(jni()->NewIntArray(count));
      break;

    case JVM_T_LONG:
      ref.reset(jni()->NewLongArray(count));
      break;

    case JVM_T_FLOAT:
      ref.reset(jni()->NewFloatArray(count));
      break;

    case JVM_T_DOUBLE:
      ref.reset(jni()->NewDoubleArray(count));
      break;

    default:
      SET_INTERNAL_ERROR(
          "invalid primitive array type $0",
          std::to_string(array_type));
      return;
  }

  if (ref == nullptr) {
    SET_INTERNAL_ERROR(
        "failed to allocate primitive array, type = $0, length = $1",
        std::to_string(array_type),
        std::to_string(count));
    return;
  }

  supervisor_->NewObjectAllocated(ref.get());

  stack_.PushStackObject(ref.get());
}


void NanoJavaInterpreter::ReturnOperation(JType return_opcode_type) {
  std::shared_ptr<ClassIndexer::Type> return_type = method_->return_type();
  JType expected_opcode_type = return_type->GetType();
  if ((expected_opcode_type == JType::Boolean) ||
      (expected_opcode_type == JType::Byte) ||
      (expected_opcode_type == JType::Char) ||
      (expected_opcode_type == JType::Short)) {
    expected_opcode_type = JType::Int;
  }

  if (return_opcode_type != expected_opcode_type) {
    SET_INTERNAL_ERROR(
        "bad return type $0 (expected $1)",
        TypeNameFromSignature({ return_opcode_type }),
        TypeNameFromSignature({ expected_opcode_type }));
    return;
  }

  if (expected_opcode_type == JType::Void) {
    SetResult(MethodCallResult::Success(JVariant()));
    return;
  }

  if (expected_opcode_type != JType::Object) {
    SetResult(MethodCallResult::Success(
        stack_.PopStackAny(return_type->GetType())));
    return;
  }

  jclass return_cls = return_type->FindClass();
  if (return_cls == nullptr) {
    LOG(ERROR) << "Return class type not found: "
               << return_type->GetSignature();
    return;
  }

  JniLocalRef return_value = stack_.PopStackObject();
  if (return_value != nullptr) {
    if (!jni()->IsInstanceOf(return_value.get(), return_cls)) {
      std::string actual = GetObjectClassSignature(return_value.get());
      std::string expected = GetClassSignature(return_cls);
      SET_INTERNAL_ERROR(
          "returned object ($0) is not an instance of $1",
          TypeNameFromSignature({ JType::Object, actual }),
          TypeNameFromSignature({ JType::Object, expected }));
      return;
    }
  }

  SetResult(MethodCallResult::Success(
      JVariant::LocalRef(std::move(return_value))));
}


int NanoJavaInterpreter::GetStackDepth() const {
  int depth = 0;
  const NanoJavaInterpreter* frame = this;
  while (frame != nullptr) {
    ++depth;
    frame = frame->diag_state_.parent_frame();
  }

  return depth;
}


// This template function is only used in this .cc file, so its implementation
// doesn't have to be in .h file.
template <typename TSlot>
TSlot NanoJavaInterpreter::div_int(TSlot n1, TSlot n2) {
  if ((n2 == std::numeric_limits<TSlot>::min()) &&
      (n1 == -1)) {
    // If the dividend is the negative integer of largest possible magnitude
    // for the int type, and the divisor is -1, then overflow occurs, and the
    // result is equal to the dividend. Despite the overflow, no exception is
    // thrown in this case.
    return n2;
  }

  if (n1 == 0) {
    RaiseException(jniproxy::ArithmeticException());
    return 0;
  }

  return n2 / n1;
}


// This template function is only used in this .cc file, so its implementation
// doesn't have to be in .h file.
template <typename TSlot>
TSlot NanoJavaInterpreter::modulo_int(TSlot n1, TSlot n2) {
  if ((n2 == std::numeric_limits<TSlot>::min()) && (n1 == -1)) {
    // If the dividend is the negative integer of largest possible magnitude
    // for the int type, and the divisor is -1, then overflow occurs, and the
    // result is equal to the dividend. Despite the overflow, no exception is
    // thrown in this case.
    return n2;
  }

  if (n1 == 0) {
    RaiseException(jniproxy::ArithmeticException());
    return 0;
  }

  return n2 % n1;
}


// This template function is only used in this .cc file, so its implementation
// doesn't have to be in .h file.
template <typename TPrimitive, typename TSlot>
TSlot NanoJavaInterpreter::modulo_float(TSlot n1, TSlot n2) {
  return as<TSlot>(std::fmod(as<TPrimitive>(n2), as<TPrimitive>(n1)));
}


NanoJavaInterpreter::DiagState::DiagState(
    const NanoJavaInterpreter* interpreter,
    const NanoJavaInterpreter* parent_frame)
    : interpreter_(interpreter),
      parent_frame_(parent_frame) {
}

std::string NanoJavaInterpreter::DiagState::method_name() const {
  std::string name;

  name += TypeNameFromSignature(
      interpreter_->method_->class_file()->class_signature());
  name += '.';
  name += interpreter_->method_->name().str();

  return name;
}

std::string NanoJavaInterpreter::DiagState::FormatCallStack() const {
  std::string s;
  const NanoJavaInterpreter* frame = interpreter_;
  while (frame != nullptr) {
    if (!s.empty()) {
      s += '\n';
    }

    s += frame->method_name();
    s += '@';
    s += std::to_string(frame->ip_);

    frame = frame->diag_state_.parent_frame_;
  }

  return s;
}

}  // namespace nanojava
}  // namespace cdbg
}  // namespace devtools
