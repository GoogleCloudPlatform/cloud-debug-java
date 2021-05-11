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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_CLASS_FILE_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_CLASS_FILE_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>

#include "nullable.h"
#include "byte_source.h"
#include "class_indexer.h"
#include "class_metadata_reader.h"
#include "common.h"
#include "type_util.h"

namespace devtools {
namespace cdbg {

// Reads constant pool and resolve references to classes, methods and fields
// into types internally used in the Cloud Debugger code. This class only
// keeps pointers. It doesn't own the buffers.
class ConstantPool {
 public:
  // Reference to a UTF-8 string defined in a constant pool. The string has
  // no NULL terminator.
  class Utf8Ref {
   public:
    // Empty string.
    Utf8Ref() : Utf8Ref(ByteSource()) {}

    explicit Utf8Ref(ByteSource buffer) : buffer_(buffer) {}

    Utf8Ref(const Utf8Ref& other) = default;

    Utf8Ref(Utf8Ref&& other) = default;

    Utf8Ref& operator= (const Utf8Ref& other) = default;

    Utf8Ref& operator= (Utf8Ref&& other) = default;

    // Iterator-like access.
    const char* begin() const { return buffer_.data<char>(); }
    const char* end() const { return begin() + size(); }

    // Gets the string size in bytes (decoded UTF-8 string might be shorter).
    int size() const { return buffer_.size(); }

    // Copies the UTF-8 string into a newly allocated C++ string.
    std::string str() const { return std::string(begin(), end()); }

    // Compares to another string.
    bool operator==(const std::string& other) const {
      if (size() != other.size()) {
        return false;
      }

      if (size() == 0) {
        return true;
      }

      return memcmp(other.data(), begin(), size()) == 0;
    }

    // Compares with a null terminated string.
    bool operator== (const char* other) const {
      if (size() != strlen(other)) {
        return false;
      }

      return memcmp(begin(), other, size()) == 0;
    }

   private:
    ByteSource buffer_;
  };

  // Class constant pool entry.
  struct ClassRef {
    // Internal name of the class.
    Utf8Ref internal_name;

    // Reference to a class object.
    std::shared_ptr<ClassIndexer::Type> type;
  };

  // String constant pool entry.
  struct StringRef {
    // UTF-8 representation of the string.
    Utf8Ref utf8;

    // Java string object constructed from the UTF-8 representation.
    JniGlobalRef str;
  };

  // NameAndType constant pool entry.
  struct NameAndTypeRef {
    Utf8Ref name;
    Utf8Ref type;
  };

  // FieldRef constant pool entry.
  struct FieldRef {
    // Class that defined the field. It also keeps a reference on the class
    // ensuring that the class will not get unloaded by the JVM.
    const ClassRef* owner = nullptr;

    // FieldRef name.
    std::string field_name;

    // Field type (signature and the class object).
    std::shared_ptr<ClassIndexer::Type> field_type;

    // True if the field has been found or false otherwise. Normally a field
    // is not found if the referenced class is not found by classs indexer. It
    // can also happen due to invalid bytecode or missing class dependencies.
    bool is_found = false;

    // Global reference to class that defined the field. We keep it here
    // to ensure that the class doesn't get unloaded. Otherwise "field_id" will
    // be pointing to invalid memory. If "is_found" is false, "owner_cls" will
    // be nullptr.
    JniGlobalRef owner_cls;

    // Weak reference to Java field or nullptr if "is_found" is false.
    jfieldID field_id = nullptr;

    // Distinguishes between static and instance fields or nullptr if "is_found"
    // is false.
    Nullable<bool> is_static;
  };

  // MethodRef constant pool entry.
  struct MethodRef {
    // Class that defined the field. It also keeps a reference on the class
    // ensuring that the class will not get unloaded by the JVM.
    const ClassRef* owner = nullptr;

    // Parsed method signature.
    JMethodSignature method_signature;

    // True if the method has been found or false otherwise. Normally a method
    // is not found if the referenced class is not found by classs indexer. It
    // can also happen due to invalid bytecode or missing class dependencies.
    bool is_found = false;

    // Method metadata or nullptr if "is_found" is false.
    Nullable<ClassMetadataReader::Method> metadata;

    // Global reference to class that defined the method. We keep it here
    // to ensure that the method is not unloaded. Otherwise "method_id" will
    // be pointing to invalid memory.
    JniGlobalRef owner_cls;

    // Weak reference to Java method or nullptr if "is_found" is false.
    jmethodID method_id = nullptr;
  };

  explicit ConstantPool(ClassIndexer* class_indexer);

  ~ConstantPool();

  // Creates an index to constant pool table. Returns false on error. Sets
  // "end_offset" to the first byte beyond the constant pool.
  bool Initialize(ByteSource class_file, int* end_offset);

  // Returns type of the constant pool item at the specified index or 0
  // if "index" is invalid or no data in that item.
  int GetType(int index) const;

  // Reads UTF-8 encoded string from constant pool. Returns nullptr on error.
  const Utf8Ref* GetUtf8(int index);

  // Reads 32 bit integer from constant pool. Returns nullptr on error.
  Nullable<jint> GetInteger(int index);

  // Reads 32 bit float from constant pool. Returns nullptr on error.
  Nullable<jfloat> GetFloat(int index);

  // Reads 64 bit integer from constant pool. Returns nullptr on error.
  Nullable<jlong> GetLong(int index);

  // Reads 64 bit float from constant pool. Returns nullptr on error.
  Nullable<jdouble> GetDouble(int index);

  // Reads referenced class from constant pool. Returns nullptr on error.
  const ClassRef* GetClass(int index);

  // Reads referenced string from constant pool. Returns nullptr on error.
  const StringRef* GetString(int index);

  // Reads FieldRef entry from constant pool. Returns nullptr on error.
  const FieldRef* GetField(int index);

  // Reads MethodRef entry from constant pool. Returns nullptr on error.
  const MethodRef* GetMethod(int index);

  // Reads NameAndType entry from constant pool. Returns nullptr on error.
  const NameAndTypeRef* GetNameAndType(int index);

 private:
  // Constant pool item.
  struct Item {
    // Type of this constant pool entry or 0 if this is not a valid item.
    uint8_t type{0};

    // Pointer to the raw constant pool item data in the class file.
    ByteSource data;

    // Expanded content of the constant pool item or nullptr if not cached
    // yet. The actual type depends on constant pool item type.
    std::atomic<void*> cache { nullptr };
  };

  // Gets the buffer of the specified constant pool entry. Returns nullptr
  // if the index is invalid or if the entry has a type different from
  // "expected_type". If "expected_type" is null, the type is not checked.
  Item* GetConstantPoolItem(int index, Nullable<uint8_t> expected_type);

  // Initializes constant pool item with no cache.
  bool InitializeConstantPoolItem(Item* item, ByteSource data);

  // If resolved constant pool item is already in cache, just returns it.
  // Otherwise loads the cache and returns pointer to it. Returns nullptr
  // on failures.
  template <typename T>
  static const T* Fetch(
      Item* item,
      std::function<std::unique_ptr<T>(uint8_t, ByteSource)> resolver);

 private:
  // Resolves class signatures to class objects. Not owned by this class.
  ClassIndexer* const class_indexer_;

  // Points to buffer of each constant pool item.
  std::vector<Item> items_;

  DISALLOW_COPY_AND_ASSIGN(ConstantPool);
};


class ClassFile {
 public:
  // Represents a single row in an exception table.
  struct TryCatchBlock {
    // Code range [begin_offset..end_offset) to which this block applies.
    int begin_offset;
    int end_offset;

    // Catch block location.
    int handler_offset;

    // Exception type to catch or nullptr to catch all exceptions thrown
    // from [begin_offset..end_offset) code range.
    const ConstantPool::ClassRef* type;
  };


  // Reader class for offsets table in TABLESWITCH instruction.
  class TableSwitchTable {
   public:
    TableSwitchTable() {}

    explicit TableSwitchTable(ByteSource table) : table_(table) {}

    // Gets the number of rows in the table.
    int size() const { return table_.size() / sizeof(int32_t); }

    // Reads the specified row from the table.
    int32_t offset(int row) {
      return table_.ReadInt32BE(row * sizeof(int32_t));
    }

    // Returns true if previous read operations failed.
    bool is_error() const { return table_.is_error(); }

   private:
    ByteSource table_;
  };


  // Reader class for lookup table in LOOKUPSWITCH instruction.
  class LookupSwitchTable {
   public:
    LookupSwitchTable() {}

    explicit LookupSwitchTable(ByteSource table) : table_(table) {}

    // Gets the number of rows in the table.
    int size() const { return table_.size() / 8; }

    // Reads value in the specified row from the table.
    int32_t value(int row) { return table_.ReadInt32BE(row * 8); }

    // Reads offset in the specified row from the table.
    int32_t offset(int row) { return table_.ReadInt32BE(row * 8 + 4); }

    // Returns true if previous read operations failed.
    bool is_error() const { return table_.is_error(); }

   private:
    ByteSource table_;
  };


  struct Instruction {
    Instruction() {
      memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
    }

    // Instruction opcode.
    uint8_t opcode;

    // Offset of this instruction relative to first instruction.
    int offset;

    // Instruction operand.
    union {
      // Integer operand used for instructions like ISTORE and branch. Index of
      // a constant pool for LDC instruction.
      int32_t int_operand;

      // Operand for IINC instruction.
      struct {
        // Index of the incremented local variable.
        uint16_t local_index;

        // Increment value.
        int16_t increment;
      } iinc_operand;

      // Operand for TABLE_SWITCH instruction.
      struct {
        // Switch value corresponding to the first entry in the table.
        int32_t low;

        // Branch table. Each row is an offset from the current instruction.
        // First first row corresponds to value of "low". The second row
        // corresponds to "low + 1", etc.
        TableSwitchTable table;

        // Offset to the default handler (relative to first instruction).
        int default_handler_offset;
      } table_switch_operand;

      // Operand for LOOKUP_SWITCH instruction.
      struct {
        // Lookup table. Each row contains the value and the offset (relative
        // to first instruction).
        LookupSwitchTable table;

        // Offset to the default handler (relative to first instruction).
        int default_handler_offset;
      } lookup_switch_operand;

      // Type operand used in instructions like ANEW.
      const ConstantPool::ClassRef* type_operand;

      // Operand for field instructions.
      const ConstantPool::FieldRef* field_operand;

      // Operand for invoke method instructions.
      const ConstantPool::MethodRef* method_operand;
    };

    // Offset to the next instruction starting (relative to first instruction).
    int next_instruction_offset;
  };

  // Classification of a Java instruction. All instructions of the same type
  // handle operands in the same way and instruction size is computed the
  // same way.
  enum class InstructionType : uint8 {
    NO_ARG,                     // Instructions without any arguments.
    INT8,                       // Signed byte argument.
    INT16,                      // Signed short argument.
    LOCAL_VAR_INDEX,            // Local variable index argument.
    IMPLICIT_LOCAL_VAR_INDEX,   // Implicit local variable index argument.
    TYPE,                       // Type descriptor argument.
    FIELD,                      // Field access instructions.
    METHOD,                     // Method invocations instructions.
    INVOKEINTERFACE,            // INVOKEINTERFACE instruction.
    INVOKEDYNAMIC,              // INVOKEDYNAMIC instruction.
    LABEL,                      // 2 bytes bytecode offset label.
    LABEL_W,                    // 4 bytes bytecode offset label.
    LDC,                        // LDC instruction.
    LDC_W,                      // LDC_W and LDC2_W instructions.
    IINC,                       // IINC instruction.
    TABLESWITCH,                // TABLESWITCH instruction.
    LOOKUPSWITCH,               // LOOKUPSWITCH instruction.
    MULTIANEWARRAY,             // MULTIANEWARRAY instruction.
    WIDE                        // WIDE instruction.
  };

  // Information about a single method in a class file.
  class Method {
   public:
    // "class_file" not owned by this class and must outlive it.
    explicit Method(ClassFile* class_file);

    Method(Method&& other) = default;

    // Gets weak reference to the constant pool.
    ClassFile* class_file() { return class_file_; }

    // Reads method information.
    bool Load(int offset, int* method_size);

    // Returns method modifiers (e.g. static, public, native).
    uint16_t method_modifiers() const { return method_modifiers_; }

    // Returns true if the method was declared as static.
    bool IsStatic() const { return (method_modifiers_ & JVM_ACC_STATIC) != 0; }

    // Gets the method name (e.g. "toString").
    const ConstantPool::Utf8Ref& name() const { return name_; }

    // Gets the method signature string (e.g. "(IZ)Ljava/lang/String;").
    const ConstantPool::Utf8Ref& signature() const { return signature_; }

    // Gets the method return type.
    std::shared_ptr<ClassIndexer::Type> return_type() const {
      return return_type_;
    }

    // Returns false if the method doesn't have code (e.g. native or abstract).
    bool HasCode() const { return GetCodeSize() > 0; }

    // Gets the total size in bytes of method instructions.
    int GetCodeSize() const { return code_.size(); }

    // Gets the maximum number of stack slots that this method uses.
    uint16_t GetMaxStack() const { return max_stack_; }

    // Gets the maximum number of local variable slots that this method uses.
    uint16_t GetMaxLocals() const { return max_locals_; }

    // Gets the number of elements in exception table.
    int GetExceptionTableSize() const { return exception_table_.size() / 8; }

    // Reads single entry from exception table. Returns nullptr on error.
    Nullable<TryCatchBlock> GetTryCatchBlock(int index);

    // Reads instruction at the specified byte offset from the first
    // instruction. Returns nullptr on error.
    Nullable<Instruction> GetInstruction(int offset);

    // Gets classification of an instruction by opcode.
    static InstructionType GetInstructionType(uint8_t opcode);

   private:
    // Class file that defined this method. Not owned by this class.
    ClassFile* const class_file_;

    // Method modifiers (e.g. static, native, etc.).
    uint16_t method_modifiers_{0xFFFF};

    // MethodRef name (e.g. "toString").
    ConstantPool::Utf8Ref name_;

    // Method signature (e.g. "(IZ)Ljava/lang/String;").
    ConstantPool::Utf8Ref signature_;

    // Reference to the returned type (either primitive or a class).
    std::shared_ptr<ClassIndexer::Type> return_type_;

    // Maximum number of stack slots that this method uses.
    uint16_t max_stack_{0};

    // Maximum number of local variable slots that this method uses.
    uint16_t max_locals_{0};

    // Code buffer. Not owned by this class.
    ByteSource code_;

    // Exception table buffer.
    ByteSource exception_table_;

    DISALLOW_COPY_AND_ASSIGN(Method);
  };

 public:
  // Loads the class file. Returns nullptr on failure (e.g. dynamic class that
  // doesn't have an underlying .class file).
  // "class_indexer" is not owned by this class and must outlive it.
  static std::unique_ptr<ClassFile> Load(
      ClassIndexer* class_indexer,
      jclass cls);

  // Loads the class file from a BLOB. Returns nullptr if this is not a valid
  // class file.
  static std::unique_ptr<ClassFile> LoadFromBlob(ClassIndexer* class_indexer,
                                                 std::string blob);

  // Gets a class file data wrapper.
  ByteSource GetData() const { return ByteSource(buffer_); }

  // Reads class modifiers (e.g. public, static). Returns nullptr on error.
  Nullable<int> GetClassModifiers() const;

  // Gets reference to this class object. Returns nullptr on error.
  const ConstantPool::ClassRef* GetClass();

  // Gets signature of this class. Returns JType::Void on error.
  JSignature class_signature() const { return class_signature_; }

  // Gets the class indexer used to resolve class signatures to class objects.
  ClassIndexer* class_indexer() { return class_indexer_; }

  // Gets the constant pool of the class. This class owns the returned pointer.
  ConstantPool* constant_pool() { return &constant_pool_; }

  // Gets the total number of methods in this class.
  int GetMethodsCount() const { return methods_.size(); }

  // Gets method by index.
  Method* GetMethod(int method_index) { return &methods_[method_index]; }

  // Find a particular method in the class file.
  Method* FindMethod(bool is_static, const std::string& name,
                     const std::string& signature);

 private:
  // Does not take ownership of "class_indexer" which must outlive this object.
  ClassFile(ClassIndexer* class_indexer, std::string buffer);

  // Reads the structure of the class file and prepares indexes.
  bool Initialize();

  // Verifies that the class file version is supported. Raises error if the
  // class file is unsupported.
  bool CheckClassFileVersion();

  // Calculates the offset to methods in the class file. Returns false on
  // corrupt class files.
  bool CalculateMethodsOffset(
      int constant_pool_end_offset,
      int* methods_offset) const;

  // Creates an index of class methods. Raises error on corrupt input.
  bool IndexMethods();

 private:
  // Class file BLOB.
  const std::string buffer_;

  // Offset of the first byte beyond the constant pool in the class file.
  int constant_pool_end_offset_ { 0 };

  // Resolves class signatures to class objects. Not owned by this class.
  ClassIndexer* const class_indexer_;

  // Constant pool (shared across all methods).
  ConstantPool constant_pool_;

  // Class signature.
  JSignature class_signature_;

  // Information and readers about each method.
  std::vector<Method> methods_;

  DISALLOW_COPY_AND_ASSIGN(ClassFile);
};

}  // namespace cdbg
}  // namespace devtools


#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_CLASS_FILE_H_
