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

#include "class_file.h"

#include <functional>
#include <sstream>

#include "gmock/gmock.h"

#include "jni_proxy_classfiletextifier.h"
#include "jni_proxy_classpathlookup.h"
#include "jni_proxy_jasmin_main.h"
#include "jni_proxy_string.h"
#include "jasmin_utils.h"
#include "jvariant.h"
#include "jvm_class_indexer.h"
#include "jvmti_buffer.h"
#include "type_util.h"
#include "re2/re2.h"

using testing::Test;
using testing::TestWithParam;
using testing::Values;

namespace devtools {
namespace cdbg {

static std::string ResolveLabel(int offset,
                                std::map<int, std::string>* labels) {
  auto it = labels->find(offset);
  if (it == labels->end()) {
    std::string label = "L" + std::to_string(labels->size());
    labels->insert(std::make_pair(offset, label));
    return label;
  }

  return it->second;
}

static std::string ResolveIntOperandLabel(
    const ClassFile::Instruction& instruction,
    std::map<int, std::string>* labels) {
  return ResolveLabel(instruction.offset + instruction.int_operand, labels);
}

static std::string PrintFieldRef(const ConstantPool::FieldRef* field_ref,
                                 bool expect_static) {
  EXPECT_TRUE(field_ref->is_found);

  EXPECT_TRUE(field_ref->is_static.has_value());
  EXPECT_EQ(expect_static, field_ref->is_static.value());

  JvmtiBuffer<char> name_buffer;
  JvmtiBuffer<char> signature_buffer;
  jvmtiError err = jvmti()->GetFieldName(
      static_cast<jclass>(field_ref->owner_cls.get()),
      field_ref->field_id,
      name_buffer.ref(),
      signature_buffer.ref(),
      nullptr);
  EXPECT_EQ(JVMTI_ERROR_NONE, err);

  EXPECT_EQ(signature_buffer.get(), field_ref->field_type->GetSignature());

  std::ostringstream ss;
  ss << field_ref->owner->internal_name.str()
     << '.'
     << name_buffer.get()
     << " : "
     << signature_buffer.get();
  return ss.str();
}

static std::string SignatureString(const JSignature& signature) {
  switch (signature.type) {
    case JType::Void:
      return "V";
    case JType::Boolean:
      return "Z";
    case JType::Byte:
      return "B";
    case JType::Char:
      return "C";
    case JType::Short:
      return "S";
    case JType::Int:
      return "I";
    case JType::Long:
      return "J";
    case JType::Float:
      return "F";
    case JType::Double:
      return "D";
    case JType::Object:
      return signature.object_signature;
  }
}

// The PrintMethodRef is supposed to generate a string containing the name and
// signature of the method being invoked. The signature will contain the types
// of passed in arguments of the invocation site instead of the declaration
// site.
static std::string PrintMethodRef(const ConstantPool::MethodRef* method_ref,
                                  bool expect_static) {
  EXPECT_TRUE(method_ref->is_found);

  EXPECT_TRUE(method_ref->metadata.has_value());
  EXPECT_EQ(expect_static, method_ref->metadata.value().is_static());

  JvmtiBuffer<char> name_buffer;
  jvmtiError err = jvmti()->GetMethodName(method_ref->method_id,
                                          name_buffer.ref(), nullptr, nullptr);
  EXPECT_EQ(JVMTI_ERROR_NONE, err);

  jboolean is_interface;
  err = jvmti()->IsInterface(static_cast<jclass>(method_ref->owner_cls.get()),
                             &is_interface);
  EXPECT_EQ(JVMTI_ERROR_NONE, err);

  std::ostringstream ss;
  ss << method_ref->owner->internal_name.str() << '.' << name_buffer.get()
     << " (";
  for (const auto& arg : method_ref->method_signature.arguments) {
    ss << SignatureString(arg);
  }
  ss << ')' << SignatureString(method_ref->method_signature.return_type);

  if (is_interface) {
    ss << " (itf)";
  }
  return ss.str();
}

template <typename T>
static std::string PrintPrimitiveLdc(ConstantPool* constant_pool,
                                     Nullable<T> (ConstantPool::*fn)(int index),
                                     int index) {
  Nullable<T> constant = (constant_pool->*fn)(index);
  if (constant == nullptr) {
    return "LDC <error>";
  }

  ExceptionOr<std::string> str = jniproxy::String()->valueOf(constant.value());
  EXPECT_FALSE(str.HasException());
  return "LDC " + str.Release(ExceptionAction::LOG_AND_IGNORE);
}

template <typename T>
static std::string PrintComplexLdc(
    ConstantPool* constant_pool, const T* (ConstantPool::*fn)(int index),
    int index, std::function<std::string(const T&)> formatter) {
  const T* item = (constant_pool->*fn)(index);
  if (item == nullptr) {
    return "LDC <error>";
  }

  return "LDC " + formatter(*item);
}

static std::string PrintString(jstring str) {
  std::ostringstream ss;

  ss << '"';

  jsize len = jni()->GetStringLength(str);
  const jchar* chars = jni()->GetStringChars(str, nullptr);

  for (int i = 0; i < len; ++i) {
    jchar ch = chars[i];
    switch (ch) {
      case '\n':
        ss << "\\n";
        break;

      case '\r':
        ss << "\\r";
        break;

      case '\\':
        ss << "\\\\";
        break;

      case '"':
        ss << "\\\"";
        break;

      default:
        // TODO(vlif): encode non-ASCII characters like ASM does.
        ss << static_cast<char>(ch);
        break;
    }
  }

  jni()->ReleaseStringChars(str, chars);

  ss << '"';

  return ss.str();
}

// Prints disassembly of a single instruction.
static std::string PrintInstruction(ConstantPool* constant_pool,
                                    const ClassFile::Instruction& instruction,
                                    std::map<int, std::string>* labels) {
  switch (instruction.opcode) {
    case JVM_OPC_nop:
      return "NOP";

    case JVM_OPC_aconst_null:
      return "ACONST_NULL";

    case JVM_OPC_bipush:
      return "BIPUSH " + std::to_string(instruction.int_operand);
    case JVM_OPC_sipush:
      return "SIPUSH " + std::to_string(instruction.int_operand);

    case JVM_OPC_iconst_m1:
      return "ICONST_M1";

    case JVM_OPC_iconst_0:
      return "ICONST_0";

    case JVM_OPC_iconst_1:
      return "ICONST_1";

    case JVM_OPC_iconst_2:
      return "ICONST_2";

    case JVM_OPC_iconst_3:
      return "ICONST_3";

    case JVM_OPC_iconst_4:
      return "ICONST_4";

    case JVM_OPC_iconst_5:
      return "ICONST_5";

    case JVM_OPC_lconst_0:
      return "LCONST_0";

    case JVM_OPC_lconst_1:
      return "LCONST_1";

    case JVM_OPC_fconst_0:
      return "FCONST_0";

    case JVM_OPC_fconst_1:
      return "FCONST_1";
    case JVM_OPC_fconst_2:
      return "FCONST_2";

    case JVM_OPC_dconst_0:
      return "DCONST_0";
    case JVM_OPC_dconst_1:
      return "DCONST_1";

    case JVM_OPC_iload:
      return "ILOAD " + std::to_string(instruction.int_operand);

    case JVM_OPC_fload:
      return "FLOAD " + std::to_string(instruction.int_operand);

    case JVM_OPC_lload:
      return "LLOAD " + std::to_string(instruction.int_operand);

    case JVM_OPC_dload:
      return "DLOAD " + std::to_string(instruction.int_operand);

    case JVM_OPC_aload:
      return "ALOAD " + std::to_string(instruction.int_operand);

    case JVM_OPC_istore:
      return "ISTORE " + std::to_string(instruction.int_operand);

    case JVM_OPC_fstore:
      return "FSTORE " + std::to_string(instruction.int_operand);

    case JVM_OPC_lstore:
      return "LSTORE " + std::to_string(instruction.int_operand);

    case JVM_OPC_dstore:
      return "DSTORE " + std::to_string(instruction.int_operand);

    case JVM_OPC_astore:
      return "ASTORE " + std::to_string(instruction.int_operand);

    case JVM_OPC_bastore:
      return "BASTORE";

    case JVM_OPC_castore:
      return "CASTORE";

    case JVM_OPC_sastore:
      return "SASTORE";

    case JVM_OPC_iastore:
      return "IASTORE";

    case JVM_OPC_lastore:
      return "LASTORE";

    case JVM_OPC_fastore:
      return "FASTORE";

    case JVM_OPC_dastore:
      return "DASTORE";

    case JVM_OPC_aastore:
      return "AASTORE";

    case JVM_OPC_baload:
      return "BALOAD";

    case JVM_OPC_caload:
      return "CALOAD";

    case JVM_OPC_saload:
      return "SALOAD";

    case JVM_OPC_iaload:
      return "IALOAD";

    case JVM_OPC_laload:
      return "LALOAD";

    case JVM_OPC_faload:
      return "FALOAD";

    case JVM_OPC_daload:
      return "DALOAD";

    case JVM_OPC_aaload:
      return "AALOAD";

    case JVM_OPC_iadd:
      return "IADD";

    case JVM_OPC_isub:
      return "ISUB";

    case JVM_OPC_imul:
      return "IMUL";

    case JVM_OPC_idiv:
      return "IDIV";

    case JVM_OPC_irem:
      return "IREM";

    case JVM_OPC_ishl:
      return "ISHL";

    case JVM_OPC_ishr:
      return "ISHR";

    case JVM_OPC_iushr:
      return "IUSHR";

    case JVM_OPC_iand:
      return "IAND";

    case JVM_OPC_ior:
      return "IOR";

    case JVM_OPC_ixor:
      return "IXOR";

    case JVM_OPC_fadd:
      return "FADD";

    case JVM_OPC_fsub:
      return "FSUB";

    case JVM_OPC_fmul:
      return "FMUL";

    case JVM_OPC_fdiv:
      return "FDIV";

    case JVM_OPC_frem:
      return "FREM";

    case JVM_OPC_fcmpl:
      return "FCMPL";

    case JVM_OPC_fcmpg:
      return "FCMPG";

    case JVM_OPC_ladd:
      return "LADD";

    case JVM_OPC_lsub:
      return "LSUB";

    case JVM_OPC_lmul:
      return "LMUL";

    case JVM_OPC_ldiv:
      return "LDIV";

    case JVM_OPC_lrem:
      return "LREM";

    case JVM_OPC_lshl:
      return "LSHL";

    case JVM_OPC_lshr:
      return "LSHR";

    case JVM_OPC_lushr:
      return "LUSHR";

    case JVM_OPC_land:
      return "LAND";

    case JVM_OPC_lor:
      return "LOR";

    case JVM_OPC_lxor:
      return "LXOR";

    case JVM_OPC_lcmp:
      return "LCMP";

    case JVM_OPC_dadd:
      return "DADD";

    case JVM_OPC_dsub:
      return "DSUB";

    case JVM_OPC_dmul:
      return "DMUL";

    case JVM_OPC_ddiv:
      return "DDIV";

    case JVM_OPC_drem:
      return "DREM";

    case JVM_OPC_ineg:
      return "INEG";

    case JVM_OPC_lneg:
      return "LNEG";

    case JVM_OPC_fneg:
      return "FNEG";

    case JVM_OPC_dneg:
      return "DNEG";

    case JVM_OPC_dcmpl:
      return "DCMPL";

    case JVM_OPC_dcmpg:
      return "DCMPG";

    case JVM_OPC_ireturn:
      return "IRETURN";

    case JVM_OPC_freturn:
      return "FRETURN";

    case JVM_OPC_lreturn:
      return "LRETURN";

    case JVM_OPC_dreturn:
      return "DRETURN";

    case JVM_OPC_areturn:
      return "ARETURN";

    case JVM_OPC_return:
      return "RETURN";

    case JVM_OPC_ldc: {
      int index = instruction.int_operand;
      int type = constant_pool->GetType(index);
      switch (type) {
        case JVM_CONSTANT_Integer:
          return PrintPrimitiveLdc(
              constant_pool,
              &ConstantPool::GetInteger,
              index);

        case JVM_CONSTANT_Float:
          return PrintPrimitiveLdc(
              constant_pool,
              &ConstantPool::GetFloat,
              index);

        case JVM_CONSTANT_Long:
          return PrintPrimitiveLdc(
              constant_pool,
              &ConstantPool::GetLong,
              index);

        case JVM_CONSTANT_Double:
          return PrintPrimitiveLdc(
              constant_pool,
              &ConstantPool::GetDouble,
              index);

        case JVM_CONSTANT_Class:
          return PrintComplexLdc<ConstantPool::ClassRef>(
              constant_pool,
              &ConstantPool::GetClass,
              index,
              [] (const ConstantPool::ClassRef& item) {
                EXPECT_EQ(GetClassSignature(item.type->FindClass()),
                          item.type->GetSignature());
                return item.type->GetSignature() + ".class";
              });

        case JVM_CONSTANT_String:
          return PrintComplexLdc<ConstantPool::StringRef>(
              constant_pool,
              &ConstantPool::GetString,
              index,
              [] (const ConstantPool::StringRef& item) {
                return PrintString(static_cast<jstring>(item.str.get()));
              });

        default:
          return "LDC <unsupported>";
      }
    }

    case JVM_OPC_dup:
      return "DUP";

    case JVM_OPC_dup_x1:
      return "DUP_X1";

    case JVM_OPC_dup_x2:
      return "DUP_X2";

    case JVM_OPC_dup2:
      return "DUP2";

    case JVM_OPC_dup2_x1:
      return "DUP2_X1";

    case JVM_OPC_dup2_x2:
      return "DUP2_X2";

    case JVM_OPC_pop:
      return "POP";

    case JVM_OPC_pop2:
      return "POP2";

    case JVM_OPC_swap:
      return "SWAP";

    case JVM_OPC_i2l:
      return "I2L";

    case JVM_OPC_i2f:
      return "I2F";

    case JVM_OPC_i2d:
      return "I2D";

    case JVM_OPC_l2i:
      return "L2I";

    case JVM_OPC_l2f:
      return "L2F";

    case JVM_OPC_l2d:
      return "L2D";

    case JVM_OPC_f2i:
      return "F2I";

    case JVM_OPC_f2l:
      return "F2L";

    case JVM_OPC_f2d:
      return "F2D";

    case JVM_OPC_d2i:
      return "D2I";

    case JVM_OPC_d2l:
      return "D2L";

    case JVM_OPC_d2f:
      return "D2F";

    case JVM_OPC_i2b:
      return "I2B";

    case JVM_OPC_i2c:
      return "I2C";

    case JVM_OPC_i2s:
      return "I2S";

    case JVM_OPC_invokevirtual:
      return "INVOKEVIRTUAL " +
             PrintMethodRef(instruction.method_operand, false);

    case JVM_OPC_invokespecial:
      return "INVOKESPECIAL " +
             PrintMethodRef(instruction.method_operand, false);

    case JVM_OPC_invokestatic:
      return "INVOKESTATIC " +
             PrintMethodRef(instruction.method_operand, true);

    case JVM_OPC_invokeinterface:
      return "INVOKEINTERFACE " +
             PrintMethodRef(instruction.method_operand, false);

    case JVM_OPC_new:
      return "NEW " + instruction.type_operand->internal_name.str();

    case JVM_OPC_newarray:
      switch (instruction.int_operand) {
        case JVM_T_BOOLEAN:
          return "NEWARRAY T_BOOLEAN";

        case JVM_T_BYTE:
          return "NEWARRAY T_BYTE";

        case JVM_T_CHAR:
          return "NEWARRAY T_CHAR";

        case JVM_T_SHORT:
          return "NEWARRAY T_SHORT";

        case JVM_T_INT:
          return "NEWARRAY T_INT";

        case JVM_T_LONG:
          return "NEWARRAY T_LONG";

        case JVM_T_FLOAT:
          return "NEWARRAY T_FLOAT";

        case JVM_T_DOUBLE:
          return "NEWARRAY T_DOUBLE";

        default:
          return "NEWARRAY type " + std::to_string(instruction.int_operand);
      }

    case JVM_OPC_anewarray:
      return "ANEWARRAY " + instruction.type_operand->internal_name.str();

    case JVM_OPC_instanceof:
      return "INSTANCEOF " + instruction.type_operand->internal_name.str();

    case JVM_OPC_checkcast:
      return "CHECKCAST " + instruction.type_operand->internal_name.str();

    case JVM_OPC_arraylength:
      return "ARRAYLENGTH";

    case JVM_OPC_iinc:
      return "IINC " +
             std::to_string(instruction.iinc_operand.local_index) +
             " " +
             std::to_string(instruction.iinc_operand.increment);

    case JVM_OPC_ifeq:
      return "IFEQ " + ResolveIntOperandLabel(instruction, labels);

    case JVM_OPC_ifne:
      return "IFNE " + ResolveIntOperandLabel(instruction, labels);

    case JVM_OPC_iflt:
      return "IFLT " + ResolveIntOperandLabel(instruction, labels);

    case JVM_OPC_ifle:
      return "IFLE " + ResolveIntOperandLabel(instruction, labels);

    case JVM_OPC_ifgt:
      return "IFGT " + ResolveIntOperandLabel(instruction, labels);

    case JVM_OPC_ifge:
      return "IFGE " + ResolveIntOperandLabel(instruction, labels);

    case JVM_OPC_if_icmpeq:
      return "IF_ICMPEQ " + ResolveIntOperandLabel(instruction, labels);

    case JVM_OPC_if_icmpne:
      return "IF_ICMPNE " + ResolveIntOperandLabel(instruction, labels);

    case JVM_OPC_if_icmplt:
      return "IF_ICMPLT " + ResolveIntOperandLabel(instruction, labels);

    case JVM_OPC_if_icmple:
      return "IF_ICMPLE " + ResolveIntOperandLabel(instruction, labels);

    case JVM_OPC_if_icmpgt:
      return "IF_ICMPGT " + ResolveIntOperandLabel(instruction, labels);

    case JVM_OPC_if_icmpge:
      return "IF_ICMPGE " + ResolveIntOperandLabel(instruction, labels);

    case JVM_OPC_if_acmpeq:
      return "IF_ACMPEQ " + ResolveIntOperandLabel(instruction, labels);

    case JVM_OPC_if_acmpne:
      return "IF_ACMPNE " + ResolveIntOperandLabel(instruction, labels);

    case JVM_OPC_ifnull:
      return "IFNULL " + ResolveIntOperandLabel(instruction, labels);

    case JVM_OPC_ifnonnull:
      return "IFNONNULL " + ResolveIntOperandLabel(instruction, labels);

    case JVM_OPC_goto:
      return "GOTO " + ResolveIntOperandLabel(instruction, labels);

    case JVM_OPC_getstatic:
      return "GETSTATIC " + PrintFieldRef(instruction.field_operand, true);

    case JVM_OPC_putstatic:
      return "PUTSTATIC " + PrintFieldRef(instruction.field_operand, true);

    case JVM_OPC_getfield:
      return "GETFIELD " + PrintFieldRef(instruction.field_operand, false);

    case JVM_OPC_putfield:
      return "PUTFIELD " + PrintFieldRef(instruction.field_operand, false);

    case JVM_OPC_athrow:
      return "ATHROW";

    case JVM_OPC_monitorenter:
      return "MONITORENTER";

    case JVM_OPC_monitorexit:
      return "MONITOREXIT";

    case JVM_OPC_multianewarray:
      return "MULTIANEWARRAY";

    case JVM_OPC_invokedynamic:
      return "INVOKEDYNAMIC";

    case JVM_OPC_jsr:
      return "JSR";

    case JVM_OPC_ret:
      return "RET";

    case JVM_OPC_tableswitch: {
      std::ostringstream ss;
      ss << "TABLESWITCH";
      const auto& operand = instruction.table_switch_operand;
      ClassFile::TableSwitchTable table = operand.table;
      for (int i = 0; i < table.size(); ++i) {
        ss << std::endl
           << "      " << operand.low + i << ": "
           << ResolveLabel(instruction.offset + table.offset(i), labels);
      }

      int default_offset = instruction.offset + operand.default_handler_offset;
      ss << std::endl
         << "      default: " << ResolveLabel(default_offset, labels);

      if (table.is_error()) {
        return "<error>";
      }

      return ss.str();
    }

    case JVM_OPC_lookupswitch: {
      std::ostringstream ss;
      ss << "LOOKUPSWITCH";
      const auto& operand = instruction.lookup_switch_operand;
      ClassFile::LookupSwitchTable table = operand.table;
      for (int i = 0; i < table.size(); ++i) {
        ss << std::endl
           << "      " << table.value(i) << ": "
           << ResolveLabel(instruction.offset + table.offset(i), labels);
      }

      int default_offset = instruction.offset + operand.default_handler_offset;
      ss << std::endl
         << "      default: " << ResolveLabel(default_offset, labels);

      if (table.is_error()) {
        return "<error>";
      }

      return ss.str();
    }

    default:
      return "opcode " + std::to_string(instruction.opcode);
  }
}

// Prints disassembly of the class file for testing and diagnostic purposes.
// Does not include fields, annotations and attributes.
static std::string PrintClassFile(ClassFile* class_file) {
  std::ostringstream ss;

  const ConstantPool::ClassRef* this_cls = class_file->GetClass();
  if (this_cls == nullptr) {
    return "<error>";
  }

  EXPECT_EQ(JType::Object, class_file->class_signature().type);
  EXPECT_EQ(this_cls->type->GetSignature(),
            class_file->class_signature().object_signature);

  ss << "class " << this_cls->internal_name.str() << " {" << std::endl
     << std::endl;

  int methods_count = class_file->GetMethodsCount();
  for (int method_index = 0; method_index < methods_count; ++method_index) {
    ss << std::endl;

    ClassFile::Method* method = class_file->GetMethod(method_index);

    ss << "  ";
    if (method->method_modifiers() & JVM_ACC_STATIC) {
      ss << "static ";
    }
    if (method->method_modifiers() & JVM_ACC_NATIVE) {
      ss << "native ";
    }
    ss << method->name().str() << method->signature().str();
    ss << std::endl;

    if (!method->HasCode()) {
      continue;
    }

    int offset = 0;

    // Figure out which instructions have labels.
    std::set<int> labeled_instructions;
    while (offset < method->GetCodeSize()) {
      Nullable<ClassFile::Instruction> instruction =
          method->GetInstruction(offset);
      if (!instruction.has_value()) {
        return "<error>";
      }

      std::map<int, std::string> labels;
      PrintInstruction(
          class_file->constant_pool(),
          instruction.value(),
          &labels);

      for (const auto& item : labels) {
        labeled_instructions.insert(item.first);
      }

      offset = instruction.value().next_instruction_offset;
    }

    // Print exception table.
    std::map<int, std::string> labels;

    for (int i = 0; i < method->GetExceptionTableSize(); ++i) {
      Nullable<ClassFile::TryCatchBlock> try_catch_block =
          method->GetTryCatchBlock(i);
      if (!try_catch_block.has_value()) {
        return "<error>";
      }

      ss << "    TRYCATCHBLOCK ";
      ss << ResolveLabel(try_catch_block.value().begin_offset, &labels);
      ss << ' ';
      ss << ResolveLabel(try_catch_block.value().end_offset, &labels);
      ss << ' ';
      ss << ResolveLabel(try_catch_block.value().handler_offset, &labels);
      ss << ' ';

      if (try_catch_block.value().type != nullptr) {
        ss << try_catch_block.value().type->internal_name.str();
      } else {
        ss << "null";
      }

      ss << std::endl;
    }

    // Print instructions.
    offset = 0;
    while (offset < method->GetCodeSize()) {
      Nullable<ClassFile::Instruction> instruction =
          method->GetInstruction(offset);
      if (!instruction.has_value())
        return "<error>";

      if ((labeled_instructions.find(offset) != labeled_instructions.end()) ||
          (labels.find(offset) != labels.end())) {
        ss << "   " << ResolveLabel(offset, &labels) << std::endl;
      }

      ss << "    " << PrintInstruction(class_file->constant_pool(),
                                       instruction.value(), &labels)
         << std::endl;

      offset = instruction.value().next_instruction_offset;
    }

    ss << "    MAXSTACK = " << method->GetMaxStack() << std::endl;
    ss << "    MAXLOCALS = " << method->GetMaxLocals() << std::endl;
  }

  ss << "}";

  return ss.str();
}

// Wraps "JvmClassIndexer" so that every referenced class is forcibly loaded.
// Normally "JvmClassIndexer" will never load a class, but in this test we
// actually want that.
class ClassLoadingIndexer : public ClassIndexer {
 public:
  ClassLoadingIndexer() {
    jvm_class_indexer_.Initialize();
  }

  OnClassPreparedEvent::Cookie SubscribeOnClassPreparedEvents(
      OnClassPreparedEvent::Callback fn) override {
    return jvm_class_indexer_.SubscribeOnClassPreparedEvents(fn);
  }

  void UnsubscribeOnClassPreparedEvents(
      OnClassPreparedEvent::Cookie cookie) override {
    return jvm_class_indexer_.UnsubscribeOnClassPreparedEvents(
        std::move(cookie));
  }

  JniLocalRef FindClassBySignature(
      const std::string& class_signature) override {
    return jvm_class_indexer_.FindClassBySignature(class_signature);
  }

  JniLocalRef FindClassByName(const std::string& class_name) override {
    return jvm_class_indexer_.FindClassByName(class_name);
  }

  std::shared_ptr<Type> GetPrimitiveType(JType type) override {
    return jvm_class_indexer_.GetPrimitiveType(type);
  }

  std::shared_ptr<Type> GetReference(const std::string& signature) override {
    EnsureClass(JSignatureFromSignature(signature));
    return jvm_class_indexer_.GetReference(signature);
  }

  JvmClassIndexer* GetJvmClassIndexer() {
    return &jvm_class_indexer_;
  }

 private:
  void EnsureClass(JSignature signature) {
    if (signature.type != JType::Object) {
      return;  // Primitive types are already loaded by the JVM.
    }

    if (IsArrayObjectType(signature)) {
      return EnsureClass(GetArrayElementJSignature(signature));
    }

    std::string internal_name = signature.object_signature.substr(
        1, signature.object_signature.size() - 2);

    JavaClass cls;
    EXPECT_TRUE(cls.FindWithJNI(internal_name.c_str()));

    jvm_class_indexer_.JvmtiOnClassPrepare(cls.get());
  }

 private:
  JvmClassIndexer jvm_class_indexer_;

  DISALLOW_COPY_AND_ASSIGN(ClassLoadingIndexer);
};


class ClassFileAsmTest : public TestWithParam<const char*> {
 protected:
  void SetUp() override {
    CHECK(BindSystemClasses());
    CHECK(jniproxy::BindClassFileTextifier());
    CHECK(jniproxy::BindClassPathLookup());
  }

  void TearDown() override {
    jniproxy::CleanupClassFileTextifier();
    jniproxy::CleanupClassPathLookup();
    CleanupSystemClasses();
  }
};


class ClassFileTest : public Test {
 protected:
  void SetUp() override {
    CHECK(BindSystemClasses());
    CHECK(jniproxy::BindClassFileTextifier());
    CHECK(jniproxy::BindClassPathLookup());
    CHECK(jniproxy::jasmin::BindMain());
  }

  void TearDown() override {
    jniproxy::CleanupClassFileTextifier();
    jniproxy::CleanupClassPathLookup();
    jniproxy::jasmin::CleanupMain();
    CleanupSystemClasses();
  }
};


TEST(Utf8Test, Access) {
  constexpr char kData[] = "12345678";
  ByteSource data(kData, sizeof(kData) - 1);
  ConstantPool::Utf8Ref utf8(data);

  EXPECT_EQ(8, utf8.size());
  EXPECT_EQ(kData, utf8.begin());
  EXPECT_EQ(8, utf8.end() - utf8.begin());
}


TEST(Utf8Test, ToStdString) {
  constexpr char kData[] = "12345678";
  ByteSource data(kData, sizeof(kData) - 1);
  ConstantPool::Utf8Ref utf8(data);

  EXPECT_EQ(std::string(kData), utf8.str());
}


TEST(Utf8Test, Comparison) {
  constexpr char kData[] = "12345678";
  ByteSource data(kData, sizeof(kData) - 1);
  ConstantPool::Utf8Ref utf8(data);

  EXPECT_TRUE(utf8 == kData);  // NOLINT
  EXPECT_TRUE(utf8 == std::string(kData));  // NOLINT

  EXPECT_FALSE(utf8 == "1234567");  // NOLINT
  EXPECT_FALSE(utf8 == std::string("1234567"));  // NOLINT

  EXPECT_FALSE(utf8 == "123456789");  // NOLINT
  EXPECT_FALSE(utf8 == std::string("123456789"));  // NOLINT
}


TEST(Utf8Test, Empty) {
  ConstantPool::Utf8Ref utf8(ByteSource(nullptr, 0));

  EXPECT_EQ(0, utf8.size());
  EXPECT_EQ(utf8.begin(), utf8.end());

  EXPECT_TRUE(utf8 == "");  // NOLINT
  EXPECT_TRUE(utf8 == std::string());  // NOLINT

  EXPECT_FALSE(utf8 == "a");  // NOLINT
  EXPECT_FALSE(utf8 == std::string("a"));  // NOLINT
}


TEST_F(ClassFileTest, FromEmptyBlob) {
  ClassLoadingIndexer class_indexer;
  EXPECT_EQ(nullptr, ClassFile::LoadFromBlob(&class_indexer, std::string()));
}


TEST_F(ClassFileTest, FromInvalidBlob) {
  ClassLoadingIndexer class_indexer;
  EXPECT_EQ(
      nullptr,
      ClassFile::LoadFromBlob(
          &class_indexer,
          "this is not a valid class file"));
}


TEST_F(ClassFileTest, FindMethod) {
  JavaClass cls;
  ASSERT_TRUE(cls.FindWithJNI("java/lang/Integer"));

  ClassLoadingIndexer class_indexer;
  std::unique_ptr<ClassFile> class_file =
      ClassFile::Load(&class_indexer, cls.get());
  ASSERT_NE(nullptr, class_file);

  EXPECT_NE(nullptr, class_file->FindMethod(false, "doubleValue", "()D"));
  EXPECT_EQ(nullptr, class_file->FindMethod(true, "doubleValue", "()D"));
  EXPECT_EQ(nullptr, class_file->FindMethod(false, "doubleValue2", "()D"));
  EXPECT_EQ(nullptr, class_file->FindMethod(true, "doubleValue", "()Z"));

  constexpr char kDecodeSignature[] = "(Ljava/lang/String;)Ljava/lang/Integer;";
  EXPECT_NE(nullptr, class_file->FindMethod(true, "decode", kDecodeSignature));
  EXPECT_EQ(nullptr, class_file->FindMethod(false, "decode", kDecodeSignature));
  EXPECT_EQ(nullptr, class_file->FindMethod(true, "decod", kDecodeSignature));
  EXPECT_EQ(nullptr, class_file->FindMethod(true, "decode", "()Z"));
}


TEST_F(ClassFileTest, FixupSignaturePolymorphicMethod) {
  JvmClassIndexer class_indexer;

  JavaClass cls;
  ASSERT_TRUE(cls.FindWithJNI("java/lang/invoke/MethodHandle"));
  class_indexer.JvmtiOnClassPrepare(cls.get());

  // Call the MethodHandle.invoke method using the (II)Z signature.
  std::unique_ptr<ClassFile> class_file = ClassFile::LoadFromBlob(
      &class_indexer,
      AssembleMethod(
          "V",
          "invokevirtual java/lang/invoke/MethodHandle/invoke(II)Z"));
  ASSERT_NE(nullptr, class_file);

  // Get the assembled method.
  ClassFile::Method* method = class_file->GetMethod(0);
  ASSERT_NE(nullptr, method);

  // Get the invokevirtual instruction in the assembled method.
  Nullable<ClassFile::Instruction> instruction = method->GetInstruction(0);
  ASSERT_TRUE(instruction.has_value());

  EXPECT_TRUE(instruction.value().method_operand->is_found);
  EXPECT_TRUE(instruction.value().method_operand->metadata.has_value());

  // Get the method referenced by the invokvevirtual instruction.
  const ConstantPool::MethodRef* method_ref =
      instruction.value().method_operand;
  ASSERT_NE(nullptr, method_ref);

  // Verify that the target method's signature is transformed into the
  // corresponding polymorphic signature.
  ASSERT_EQ(
      "([Ljava/lang/Object;)Ljava/lang/Object;",
      method_ref->metadata.value().signature);
}


TEST_F(ClassFileTest, MethodOperandUnknownClass) {
  JvmClassIndexer class_indexer;
  std::unique_ptr<ClassFile> class_file = ClassFile::LoadFromBlob(
      &class_indexer,
      AssembleMethod("V", "invokestatic com/my/UnknownClass/someMethod()V"));
  ASSERT_NE(nullptr, class_file);

  ClassFile::Method* method = class_file->GetMethod(0);
  ASSERT_NE(nullptr, method);

  Nullable<ClassFile::Instruction> instruction = method->GetInstruction(0);
  ASSERT_TRUE(instruction.has_value());

  EXPECT_FALSE(instruction.value().method_operand->is_found);
  EXPECT_FALSE(instruction.value().method_operand->metadata.has_value());
  EXPECT_EQ(nullptr, instruction.value().method_operand->owner_cls);
  EXPECT_EQ(nullptr, instruction.value().method_operand->method_id);
}


TEST_F(ClassFileTest, FieldOperandUnknownClass) {
  JvmClassIndexer class_indexer;
  std::unique_ptr<ClassFile> class_file = ClassFile::LoadFromBlob(
      &class_indexer,
      AssembleMethod("V", "getstatic com/my/UnknownClass/someField I"));
  ASSERT_NE(nullptr, class_file);

  ClassFile::Method* method = class_file->GetMethod(0);
  ASSERT_NE(nullptr, method);

  Nullable<ClassFile::Instruction> instruction = method->GetInstruction(0);
  ASSERT_TRUE(instruction.has_value());

  EXPECT_FALSE(instruction.value().field_operand->is_found);
  EXPECT_EQ(nullptr, instruction.value().field_operand->owner_cls);
  EXPECT_EQ(nullptr, instruction.value().field_operand->field_id);
}


TEST_P(ClassFileAsmTest, LoadClass) {
  LOG(INFO) << "Loading and disassembling class " << GetParam();

  JavaClass cls;
  ASSERT_TRUE(cls.FindWithJNI(GetParam()));

  ClassLoadingIndexer class_indexer;

  std::unique_ptr<ClassFile> class_file =
      ClassFile::Load(&class_indexer, cls.get());
  ASSERT_NE(nullptr, class_file);

  std::string expected = jniproxy::ClassFileTextifier()
                             ->textify(cls.get(), true)
                             .Release(ExceptionAction::LOG_AND_IGNORE);
  ASSERT_FALSE(expected.empty());

  std::string actual = PrintClassFile(class_file.get());

  // Collapse multiple consecutive blank lines into one. This reduces the
  // chance that minor changes to source files (in particular, which
  // annotations they use) will produce diffs.
  RE2::GlobalReplace(&expected, "\n{3,}", "\n\n");
  RE2::GlobalReplace(&actual, "\n{3,}", "\n\n");
  EXPECT_EQ(expected, actual);
}

INSTANTIATE_TEST_SUITE_P(
    com_google_devtools_cdbg_debuglets, ClassFileAsmTest,
    Values("com/google/devtools/cdbg/debuglets/java/ClassFileTextifier",
           "com/google/devtools/cdbg/debuglets/java/"
           "ClassFileTextifier$MethodFilter",
           "com/google/devtools/cdbg/debuglets/java/"
           "ClassFileTextifier$ClassFilter",
           "com/google/devtools/cdbg/debuglets/java/"
           "ClassFileTextifier$FilteredTextifier"));
INSTANTIATE_TEST_SUITE_P(java_lang, ClassFileAsmTest,
                         Values("java/lang/Integer", "java/lang/String",
                                "java/lang/StringBuilder"));

INSTANTIATE_TEST_SUITE_P(
    java_util, ClassFileAsmTest,
    Values("java/util/AbstractList$Itr", "java/util/ArrayDeque",
           "java/util/ArrayDeque$DeqIterator", "java/util/ArrayList",
           "java/util/ArrayList$Itr", "java/util/Arrays",
           "java/util/Arrays$ArrayList", "java/util/Collection",
           "java/util/Collections$EmptyMap", "java/util/Collections$EmptySet",
           "java/util/Collections$SynchronizedSet",
           "java/util/Collections$UnmodifiableMap", "java/util/EnumMap",
           "java/util/EnumSet", "java/util/HashMap",
           "java/util/HashMap$EntryIterator", "java/util/HashMap$EntrySet",
           "java/util/HashMap$KeyIterator", "java/util/HashSet",
           "java/util/Hashtable", "java/util/Hashtable$Entry",
           "java/util/Hashtable$Enumerator", "java/util/IdentityHashMap",
           "java/util/Iterator", "java/util/LinkedHashMap",
           "java/util/LinkedHashMap$Entry",
           "java/util/LinkedHashMap$LinkedEntryIterator",
           "java/util/LinkedHashMap$LinkedEntrySet",
           "java/util/LinkedHashMap$LinkedKeyIterator",
           "java/util/LinkedHashSet", "java/util/LinkedList",
           "java/util/LinkedList$ListItr", "java/util/Map",
           "java/util/PriorityQueue", "java/util/PriorityQueue$Itr",
           "java/util/Properties", "java/util/Set", "java/util/Stack",
           "java/util/TreeMap", "java/util/TreeMap$EntryIterator",
           "java/util/TreeMap$EntrySet", "java/util/TreeMap$KeyIterator",
           "java/util/TreeSet", "java/util/Vector", "java/util/Vector$Itr",
           "java/util/WeakHashMap"));

INSTANTIATE_TEST_SUITE_P(
    java_util_concurrent, ClassFileAsmTest,
    Values("java/util/concurrent/ArrayBlockingQueue",
           "java/util/concurrent/ArrayBlockingQueue$Itr",
           "java/util/concurrent/ConcurrentHashMap",
           "java/util/concurrent/ConcurrentHashMap$EntryIterator",
           "java/util/concurrent/ConcurrentHashMap$EntrySetView",
           "java/util/concurrent/ConcurrentLinkedDeque",
           "java/util/concurrent/ConcurrentLinkedDeque$Itr",
           "java/util/concurrent/ConcurrentLinkedQueue",
           "java/util/concurrent/ConcurrentLinkedQueue$Itr",
           "java/util/concurrent/ConcurrentSkipListMap",
           "java/util/concurrent/ConcurrentSkipListMap$EntryIterator",
           "java/util/concurrent/ConcurrentSkipListMap$EntrySet",
           "java/util/concurrent/CopyOnWriteArrayList",
           "java/util/concurrent/CopyOnWriteArrayList$COWIterator",
           "java/util/concurrent/CopyOnWriteArraySet",
           "java/util/concurrent/LinkedBlockingDeque",
           "java/util/concurrent/LinkedBlockingDeque$Itr",
           "java/util/concurrent/LinkedBlockingQueue",
           "java/util/concurrent/LinkedBlockingQueue$Itr"));

INSTANTIATE_TEST_SUITE_P(
    org_objectweb_asm, ClassFileAsmTest,
    Values("org/objectweb/asm/AnnotationVisitor",
           "org/objectweb/asm/AnnotationWriter", "org/objectweb/asm/Attribute",
           "org/objectweb/asm/ByteVector", "org/objectweb/asm/ClassReader",
           "org/objectweb/asm/ClassVisitor", "org/objectweb/asm/ClassWriter",
           "org/objectweb/asm/Context", "org/objectweb/asm/Edge",
           "org/objectweb/asm/FieldVisitor", "org/objectweb/asm/FieldWriter",
           "org/objectweb/asm/Frame", "org/objectweb/asm/Handle",
           "org/objectweb/asm/Handler", "org/objectweb/asm/Label",
           "org/objectweb/asm/MethodVisitor", "org/objectweb/asm/MethodWriter",
           "org/objectweb/asm/Opcodes", "org/objectweb/asm/Type",
           "org/objectweb/asm/TypePath", "org/objectweb/asm/TypeReference"));

INSTANTIATE_TEST_SUITE_P(org_objectweb_asm_signature, ClassFileAsmTest,
                         Values("org/objectweb/asm/signature/SignatureReader",
                                "org/objectweb/asm/signature/SignatureVisitor",
                                "org/objectweb/asm/signature/SignatureWriter"));

INSTANTIATE_TEST_SUITE_P(
    org_objectweb_asm_util, ClassFileAsmTest,
    Values("org/objectweb/asm/util/ASMifierSupport",
           "org/objectweb/asm/util/ASMifier",
           "org/objectweb/asm/util/CheckAnnotationAdapter",
           "org/objectweb/asm/util/CheckClassAdapter",
           "org/objectweb/asm/util/CheckFieldAdapter",
           "org/objectweb/asm/util/CheckMethodAdapter$1",
           "org/objectweb/asm/util/CheckMethodAdapter",
           "org/objectweb/asm/util/CheckSignatureAdapter",
           "org/objectweb/asm/util/Printer",
           "org/objectweb/asm/util/TextifierSupport",
           "org/objectweb/asm/util/Textifier",
           "org/objectweb/asm/util/TraceAnnotationVisitor",
           "org/objectweb/asm/util/TraceClassVisitor",
           "org/objectweb/asm/util/TraceFieldVisitor",
           "org/objectweb/asm/util/TraceMethodVisitor",
           "org/objectweb/asm/util/TraceSignatureVisitor"));

INSTANTIATE_TEST_SUITE_P(
    org_objectweb_asm_tree, ClassFileAsmTest,
    Values("org/objectweb/asm/tree/AbstractInsnNode",
           "org/objectweb/asm/tree/AnnotationNode",
           "org/objectweb/asm/tree/ClassNode",
           "org/objectweb/asm/tree/FieldInsnNode",
           "org/objectweb/asm/tree/FieldNode",
           "org/objectweb/asm/tree/FrameNode",
           "org/objectweb/asm/tree/IincInsnNode",
           "org/objectweb/asm/tree/InnerClassNode",
           "org/objectweb/asm/tree/InsnList$InsnListIterator",
           "org/objectweb/asm/tree/InsnList", "org/objectweb/asm/tree/InsnNode",
           "org/objectweb/asm/tree/IntInsnNode",
           "org/objectweb/asm/tree/InvokeDynamicInsnNode",
           "org/objectweb/asm/tree/JumpInsnNode",
           "org/objectweb/asm/tree/LabelNode",
           "org/objectweb/asm/tree/LdcInsnNode",
           "org/objectweb/asm/tree/LineNumberNode",
           "org/objectweb/asm/tree/LocalVariableAnnotationNode",
           "org/objectweb/asm/tree/LocalVariableNode",
           "org/objectweb/asm/tree/LookupSwitchInsnNode",
           "org/objectweb/asm/tree/MethodInsnNode",
           "org/objectweb/asm/tree/MethodNode$1",
           "org/objectweb/asm/tree/MethodNode",
           "org/objectweb/asm/tree/MultiANewArrayInsnNode",
           "org/objectweb/asm/tree/ParameterNode",
           "org/objectweb/asm/tree/TableSwitchInsnNode",
           "org/objectweb/asm/tree/TryCatchBlockNode",
           "org/objectweb/asm/tree/TypeAnnotationNode",
           "org/objectweb/asm/tree/TypeInsnNode",
           "org/objectweb/asm/tree/VarInsnNode"));

INSTANTIATE_TEST_SUITE_P(
    org_objectweb_asm_tree_analysis, ClassFileAsmTest,
    Values("org/objectweb/asm/tree/analysis/Analyzer",
           "org/objectweb/asm/tree/analysis/AnalyzerException",
           "org/objectweb/asm/tree/analysis/BasicInterpreter",
           "org/objectweb/asm/tree/analysis/BasicValue",
           "org/objectweb/asm/tree/analysis/BasicVerifier",
           "org/objectweb/asm/tree/analysis/Frame",
           "org/objectweb/asm/tree/analysis/Interpreter",
           "org/objectweb/asm/tree/analysis/SimpleVerifier",
           "org/objectweb/asm/tree/analysis/SmallSet",
           "org/objectweb/asm/tree/analysis/SourceInterpreter",
           "org/objectweb/asm/tree/analysis/SourceValue",
           "org/objectweb/asm/tree/analysis/Subroutine",
           "org/objectweb/asm/tree/analysis/Value"));

INSTANTIATE_TEST_SUITE_P(
    com_google_common_base, ClassFileAsmTest,
    Values(
        "com/google/common/base/Ascii",
        "com/google/common/base/CaseFormat$StringConverter",
        "com/google/common/base/CaseFormat",
        "com/google/common/base/CharMatcher",
        "com/google/common/base/Charsets",
        "com/google/common/base/Converter",
        "com/google/common/base/Defaults",
        "com/google/common/base/Enums",
        "com/google/common/base/Equivalence",
        "com/google/common/base/FinalizablePhantomReference",
        "com/google/common/base/FinalizableReferenceQueue",
        "com/google/common/base/FinalizableSoftReference",
        "com/google/common/base/FinalizableWeakReference",
        "com/google/common/base/Function",
        "com/google/common/base/FunctionalEquivalence",
        "com/google/common/base/Functions",
        "com/google/common/base/Joiner",
        "com/google/common/base/MoreObjects",
        "com/google/common/base/Objects",
        "com/google/common/base/Optional",
        "com/google/common/base/PairwiseEquivalence",
        "com/google/common/base/Platform",
        "com/google/common/base/Preconditions",
        "com/google/common/base/Predicate",
        "com/google/common/base/Predicates",
        "com/google/common/base/Present",
        "com/google/common/base/SmallCharMatcher",
        "com/google/common/base/Splitter",
        "com/google/common/base/StandardSystemProperty",
        "com/google/common/base/Strings",
        "com/google/common/base/Supplier",
        "com/google/common/base/Throwables",
        "com/google/common/base/Ticker",
        "com/google/common/base/Utf8",
        "com/google/common/base/Verify",
        "com/google/common/base/VerifyException"));

}  // namespace cdbg
}  // namespace devtools

