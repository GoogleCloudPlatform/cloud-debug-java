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

#include <cstdint>

#include "jni_proxy_classpathlookup.h"
#include "jvmti_buffer.h"

namespace devtools {
namespace cdbg {

// Initialize and returns static array mapping each opcode to its type.
static ClassFile::InstructionType* BuildInstructionTypeMap() {
  static ClassFile::InstructionType map[256];

  for (int i = 0; i < arraysize(map); ++i) {
    map[i] = ClassFile::InstructionType::NO_ARG;
  }

  map[JVM_OPC_newarray] = ClassFile::InstructionType::INT8;
  map[JVM_OPC_bipush] = ClassFile::InstructionType::INT8;

  map[JVM_OPC_sipush] = ClassFile::InstructionType::INT16;

  map[JVM_OPC_ret] = ClassFile::InstructionType::LOCAL_VAR_INDEX;

  for (int i = JVM_OPC_iload; i <= JVM_OPC_aload; ++i) {
    map[i] = ClassFile::InstructionType::LOCAL_VAR_INDEX;
  }

  for (int i = JVM_OPC_istore; i <= JVM_OPC_astore; ++i) {
    map[i] = ClassFile::InstructionType::LOCAL_VAR_INDEX;
  }

  for (int i = JVM_OPC_iload_0; i <= JVM_OPC_aload_3; ++i) {
    map[i] = ClassFile::InstructionType::IMPLICIT_LOCAL_VAR_INDEX;
  }

  for (int i = JVM_OPC_istore_0; i <= JVM_OPC_astore_3; ++i) {
    map[i] = ClassFile::InstructionType::IMPLICIT_LOCAL_VAR_INDEX;
  }

  map[JVM_OPC_new] = ClassFile::InstructionType::TYPE;
  map[JVM_OPC_anewarray] = ClassFile::InstructionType::TYPE;
  map[JVM_OPC_checkcast] = ClassFile::InstructionType::TYPE;
  map[JVM_OPC_instanceof] = ClassFile::InstructionType::TYPE;

  for (int i = JVM_OPC_getstatic; i <= JVM_OPC_putfield; ++i) {
    map[i] = ClassFile::InstructionType::FIELD;
  }

  for (int i = JVM_OPC_invokevirtual; i <= JVM_OPC_invokestatic; ++i) {
    map[i] = ClassFile::InstructionType::METHOD;
  }

  map[JVM_OPC_invokeinterface] = ClassFile::InstructionType::INVOKEINTERFACE;
  map[JVM_OPC_invokedynamic] = ClassFile::InstructionType::INVOKEDYNAMIC;

  for (int i = JVM_OPC_ifeq; i <= JVM_OPC_jsr; ++i) {
    map[i] = ClassFile::InstructionType::LABEL;
  }

  map[JVM_OPC_ifnull] = ClassFile::InstructionType::LABEL;
  map[JVM_OPC_ifnonnull] = ClassFile::InstructionType::LABEL;
  map[JVM_OPC_goto_w] = ClassFile::InstructionType::LABEL_W;
  map[JVM_OPC_jsr_w] = ClassFile::InstructionType::LABEL_W;

  map[JVM_OPC_ldc] = ClassFile::InstructionType::LDC;
  map[JVM_OPC_ldc_w] = ClassFile::InstructionType::LDC_W;
  map[JVM_OPC_ldc2_w] = ClassFile::InstructionType::LDC_W;

  map[JVM_OPC_iinc] = ClassFile::InstructionType::IINC;
  map[JVM_OPC_tableswitch] = ClassFile::InstructionType::TABLESWITCH;
  map[JVM_OPC_lookupswitch] = ClassFile::InstructionType::LOOKUPSWITCH;
  map[JVM_OPC_multianewarray] = ClassFile::InstructionType::MULTIANEWARRAY;
  map[JVM_OPC_wide] = ClassFile::InstructionType::WIDE;

  return map;
}


// Returns true if input method is a Signature Polymorphic method.
//
// A method is Signature Polymorphic if:
//  1. It is a method of class Varhandle or MethodHandle,
//  2. It takes one argument of type Object[] and can return any type.
//  3. It has the access modifiers: ACC_NATIVE and ACC_VARARGS.
//
// The caller of the Java method does not know that it is making a call to a
// signature polymorphic method (the JVM bridges the gap and transforms the
// signatures). Therefore, we also need to make a similar transformation. Here,
// we search the target class to see if there is a signature polymorphic method
// that matches the Java method being called.
static bool IsPolymorphicMethod(jclass owner_cls,
                                const std::string& owner_cls_signature,
                                const std::string& method_name,
                                std::string* polymorphic_method_signature,
                                jmethodID* polymorphic_instance_method_id,
                                jmethodID* polymorphic_static_method_id) {
  if (owner_cls_signature != "Ljava/lang/invoke/VarHandle;" &&
      owner_cls_signature != "Ljava/lang/invoke/MethodHandle;") {
    return false;
  }

  // Since signature polymorphic methods can have any return type,
  // we cannot search by method signature. Instead, we have to iterate
  // over all methods in the target class and find the one that
  // satisfies the above criteria for being 'polymorphic'.
  jint methods_count = 0;
  JvmtiBuffer<jmethodID> methods_buf;
  jvmtiError err = jvmti()->GetClassMethods(
      owner_cls,
      &methods_count,
      methods_buf.ref());
  if (err != JVMTI_ERROR_NONE) {
    return false;  // Failed to get class methods.
  }

  for (int method_index = 0; method_index < methods_count; ++method_index) {
    jmethodID cur_method = methods_buf.get()[method_index];

    JvmtiBuffer<char> name_buf;
    JvmtiBuffer<char> sig_buf;
    err = jvmti()->GetMethodName(
        cur_method,
        name_buf.ref(),
        sig_buf.ref(),
        nullptr);
    if (err != JVMTI_ERROR_NONE) {
      continue;  // Failed to get method name.
    }

    if ((name_buf.get() == nullptr) ||
        (method_name != name_buf.get())) {
      continue;  // Mismatch in method name.
    }

    if (TrimReturnType(sig_buf.get()) == "[Ljava/lang/Object;") {
      continue;  // Mismatch in args type.
    }

    jint method_modifiers = 0;
    jvmtiError err =
        jvmti()->GetMethodModifiers(cur_method, &method_modifiers);
    if (err != JVMTI_ERROR_NONE) {
      continue;  // Failed to get method modifiers.
    }

    if ((method_modifiers & JVM_ACC_NATIVE) == 0 ||
        (method_modifiers & JVM_ACC_VARARGS) == 0) {
      continue;
    }

    // Match found.
    if ((method_modifiers & JVM_ACC_STATIC) != 0) {
      *polymorphic_static_method_id = cur_method;
    } else {
      *polymorphic_instance_method_id = cur_method;
    }

    *polymorphic_method_signature = sig_buf.get();
    return true;
  }

  return false;  // Not found.
}

ClassFile::ClassFile(ClassIndexer* class_indexer, std::string buffer)
    : buffer_(std::move(buffer)),
      class_indexer_(class_indexer),
      constant_pool_(class_indexer) {}

bool ClassFile::Initialize() {
  if (!CheckClassFileVersion()) {
    return false;
  }

  if (!constant_pool_.Initialize(GetData(), &constant_pool_end_offset_)) {
    return false;
  }

  if (!IndexMethods()) {
    return false;
  }

  const ConstantPool::ClassRef* ref = GetClass();
  if (ref != nullptr) {
    class_signature_ = { JType::Object, ref->type->GetSignature() };
  }

  return true;
}


std::unique_ptr<ClassFile> ClassFile::Load(
    ClassIndexer* class_indexer,
    jclass cls) {
  std::string blob = jniproxy::ClassPathLookup()->readClassFile(cls).Release(
      ExceptionAction::LOG_AND_IGNORE);
  return LoadFromBlob(class_indexer, std::move(blob));
}

std::unique_ptr<ClassFile> ClassFile::LoadFromBlob(ClassIndexer* class_indexer,
                                                   std::string blob) {
  std::unique_ptr<ClassFile> instance(
      new ClassFile(class_indexer, std::move(blob)));
  if (!instance->Initialize()) {
    return nullptr;
  }

  return instance;
}

Nullable<int> ClassFile::GetClassModifiers() const {
  ByteSource reader = GetData();

  uint16_t modifiers = reader.ReadUInt16BE(constant_pool_end_offset_);
  if (reader.is_error()) {
    return nullptr;
  }

  return modifiers;
}


const ConstantPool::ClassRef* ClassFile::GetClass() {
  ByteSource reader = GetData();
  int index = reader.ReadUInt16BE(constant_pool_end_offset_ + 2);
  if (reader.is_error()) {
    return nullptr;
  }

  return constant_pool_.GetClass(index);
}

ClassFile::Method* ClassFile::FindMethod(bool is_static,
                                         const std::string& name,
                                         const std::string& signature) {
  for (Method& method : methods_) {
    if ((method.IsStatic() == is_static) &&
        (method.name() == name) &&
        (method.signature() == signature)) {
      return &method;
    }
  }

  return nullptr;  // Not found.
}

bool ClassFile::CheckClassFileVersion() {
  ByteSource reader(buffer_);

  // Check class file version.
  int16_t version = reader.ReadInt16BE(6);
  if (version > 55) {
    LOG(ERROR) << "Unsupported class file version " << version;
    return false;
  }

  return !reader.is_error();
}


bool ClassFile::CalculateMethodsOffset(
    int constant_pool_end_offset,
    int* methods_offset) const {
  ByteSource reader(buffer_);

  *methods_offset = constant_pool_end_offset;

  // Skip class information and list of implemented interfaces.
  *methods_offset += 8 + 2 * reader.ReadUInt16BE(*methods_offset + 6);

  // Skip class fields.
  uint16_t fields_count = reader.ReadUInt16BE(*methods_offset);
  *methods_offset += 2;

  if (reader.is_error()) {
    return false;
  }

  for (int i = 0; i < fields_count; ++i) {
    uint16_t attributes_count = reader.ReadUInt16BE(*methods_offset + 6);
    *methods_offset += 8;

    if (reader.is_error()) {
      return false;
    }

    for (int j = 0; j < attributes_count; ++j) {
      *methods_offset += 6 + reader.ReadInt32BE(*methods_offset + 2);
    }
  }

  return !reader.is_error();
}


bool ClassFile::IndexMethods() {
  int methods_offset = 0;
  if (!CalculateMethodsOffset(constant_pool_end_offset_, &methods_offset)) {
    LOG(ERROR) << "Failed to calculate offset to class methods";
    return false;
  }

  ByteSource reader(buffer_);

  // Loop through class methods.
  int offset = methods_offset;
  const uint16_t methods_count = reader.ReadUInt16BE(offset);
  offset += 2;

  methods_.reserve(methods_count);

  for (int i = 0; i < methods_count; ++i) {
    int method_size = 0;

    Method method(this);
    if (!method.Load(offset, &method_size)) {
      LOG(ERROR) << "Failed to load method " << i;
      return false;
    }

    offset += method_size;

    methods_.push_back(std::move(method));
  }

  return !reader.is_error();
}


ConstantPool::ConstantPool(ClassIndexer* class_indexer)
    : class_indexer_(class_indexer) {
}


ConstantPool::~ConstantPool() {
  for (Item& item : items_) {
    void* cache = item.cache.load();
    if (cache == nullptr) {
      continue;
    }

    switch (item.type) {
      case JVM_CONSTANT_Utf8:
        delete static_cast<Utf8Ref*>(cache);
        break;

      case JVM_CONSTANT_Class:
        delete static_cast<ClassRef*>(cache);
        break;

      case JVM_CONSTANT_String:
        delete static_cast<StringRef*>(cache);
        break;

      case JVM_CONSTANT_Fieldref:
        delete static_cast<FieldRef*>(cache);
        break;

      case JVM_CONSTANT_Methodref:
      case JVM_CONSTANT_InterfaceMethodref:
        delete static_cast<MethodRef*>(cache);
        break;

      case JVM_CONSTANT_NameAndType:
        delete static_cast<NameAndTypeRef*>(cache);
        break;

      default:
        DCHECK(false) << "Missing cleanup for constant pool item of type "
                      << static_cast<int>(item.type);
        break;
    }
  }
}


bool ConstantPool::Initialize(ByteSource class_file, int* end_offset) {
  DCHECK_EQ(0, items_.size()) << "IndexConstantPool can only be called once";

  *end_offset = 10;

  const uint16_t constant_pool_count = class_file.ReadUInt16BE(8);
  items_ = std::vector<Item>(constant_pool_count);

  for (int i = 1; i < constant_pool_count; ++i) {
    int size = 0;
    switch (class_file.ReadUInt8(*end_offset)) {
      case JVM_CONSTANT_Fieldref:
      case JVM_CONSTANT_Methodref:
      case JVM_CONSTANT_InterfaceMethodref:
      case JVM_CONSTANT_Integer:
      case JVM_CONSTANT_Float:
      case JVM_CONSTANT_NameAndType:
      case JVM_CONSTANT_InvokeDynamic:
        size = 5;
        break;

      case JVM_CONSTANT_Long:
      case JVM_CONSTANT_Double:
        size = 9;
        break;

      case JVM_CONSTANT_Utf8:
        size = 3 + class_file.ReadUInt16BE(*end_offset + 1);
        break;

      case JVM_CONSTANT_MethodHandle:
        size = 4;
        break;

      default:
        size = 3;
        break;
    }

    if (!InitializeConstantPoolItem(
            &items_[i],
            class_file.sub(*end_offset, size))) {
      LOG(WARNING) << "Failed to initialize constant pool item " << i;
      return false;
    }

    if ((items_[i].type == JVM_CONSTANT_Long) ||
        (items_[i].type == JVM_CONSTANT_Double)) {
      ++i;
    }

    *end_offset += size;
  }

  return !class_file.is_error();
}


bool ConstantPool::InitializeConstantPoolItem(
    ConstantPool::Item* item,
    ByteSource data) {
  item->type = data.ReadUInt8(0);
  item->data = data;

  return !data.is_error();
}


// This function is only used in this module, so the implementation doesn't
// have to go to the header file.
template <typename T>
const T* ConstantPool::Fetch(
    ConstantPool::Item* item,
    std::function<std::unique_ptr<T>(uint8_t, ByteSource)> resolver) {
  if (item == nullptr) {
    LOG(ERROR) << "Null constant pool item";
    return nullptr;  // Error occurred.
  }

  void* p = item->cache.load(std::memory_order_relaxed);
  if (p != nullptr) {
    return reinterpret_cast<T*>(p);  // Common code path.
  }

  std::unique_ptr<T> value = resolver(item->type, item->data);
  if (value == nullptr) {
    LOG(ERROR) << "Failed to resolve constant pool item of type "
               << static_cast<int>(item->type);
    return nullptr;  // Error occurred.
  }

  void* expected = nullptr;
  if (item->cache.compare_exchange_strong(expected, value.get())) {
    return reinterpret_cast<T*>(value.release());  // Cached.
  }

  // Another thread just populated cache, discard "value".
  DCHECK(expected != nullptr);
  return reinterpret_cast<T*>(expected);
}

ConstantPool::Item* ConstantPool::GetConstantPoolItem(
    int index, Nullable<uint8_t> expected_type) {
  if ((index < 0) || (index >= items_.size())) {
    LOG(ERROR) << "Bad constant pool item " << index;
    return nullptr;
  }

  Item& item = items_[index];
  if (expected_type.has_value() && (item.type != expected_type.value())) {
    LOG(ERROR) << "Constant pool item " << index << " has type "
               << static_cast<int>(item.type)
               << ", expected " << static_cast<int>(expected_type.value());
    return nullptr;
  }

  return &item;
}

int ConstantPool::GetType(int index) const {
  if ((index < 0) || (index >= items_.size())) {
    LOG(ERROR) << "Bad constant pool item " << index;
    return 0;
  }

  return items_[index].type;
}


const ConstantPool::Utf8Ref* ConstantPool::GetUtf8(int index) {
  return Fetch<Utf8Ref>(
      GetConstantPoolItem(index, JVM_CONSTANT_Utf8),
      [this] (uint8 type, ByteSource data) -> std::unique_ptr<Utf8Ref> {
        const uint16 length = data.ReadUInt16BE(1);

        // Header of UTF-8 string in a constant pool is 3 byte long: 1 byte
        // for the type (JVM_CONSTANT_Utf8) and two bytes for string size.
        if (data.is_error() || (length + 3 > data.size())) {
          LOG(ERROR) << "Bad UTF-8 string in constant pool";
          return nullptr;
        }

        return std::unique_ptr<Utf8Ref>(new Utf8Ref(data.sub(3, length)));
      });
}


Nullable<jint> ConstantPool::GetInteger(int index) {
  Item* item = GetConstantPoolItem(index, JVM_CONSTANT_Integer);
  if (item == nullptr) {
    return nullptr;
  }

  ByteSource data = item->data;

  const int32_t value = data.ReadInt32BE(1);
  if (data.is_error()) {
    return nullptr;
  }

  return value;
}


Nullable<jfloat> ConstantPool::GetFloat(int index) {
  Item* item = GetConstantPoolItem(index, JVM_CONSTANT_Float);
  if (item == nullptr) {
    return nullptr;
  }

  ByteSource data = item->data;

  const int32_t value = data.ReadInt32BE(1);
  if (data.is_error()) {
    return nullptr;
  }

  static_assert(sizeof(jfloat) == sizeof(value), "valid cast");

  return *reinterpret_cast<const jfloat*>(&value);
}


Nullable<jlong> ConstantPool::GetLong(int index) {
  Item* item = GetConstantPoolItem(index, JVM_CONSTANT_Long);
  if (item == nullptr) {
    return nullptr;
  }

  ByteSource data = item->data;

  const int64_t value = data.ReadInt64BE(1);
  if (data.is_error()) {
    return nullptr;
  }

  return value;
}


Nullable<jdouble> ConstantPool::GetDouble(int index) {
  Item* item = GetConstantPoolItem(index, JVM_CONSTANT_Double);
  if (item == nullptr) {
    return nullptr;
  }

  ByteSource data = item->data;

  const int64_t value = data.ReadInt64BE(1);
  if (data.is_error()) {
    return nullptr;
  }

  static_assert(sizeof(jdouble) == sizeof(value), "valid cast");

  return *reinterpret_cast<const jdouble*>(&value);
}


const ConstantPool::ClassRef* ConstantPool::GetClass(int index) {
  return Fetch<ClassRef>(
      GetConstantPoolItem(index, JVM_CONSTANT_Class),
      [this] (uint8 type, ByteSource data) -> std::unique_ptr<ClassRef> {
        const int index = data.ReadUInt16BE(1);
        if (data.is_error()) {
          return nullptr;
        }

        const Utf8Ref* internal_name = GetUtf8(index);
        if (internal_name == nullptr) {
          return nullptr;
        }

        std::string signature = internal_name->str();
        if ((signature.size() > 1) && (signature.front() != '[')) {
          signature.insert(signature.begin(), 'L');
          signature.insert(signature.end(), ';');
        }

        std::shared_ptr<ClassIndexer::Type> type_reference = JSignatureToType(
            class_indexer_,
            JSignatureFromSignature(signature));
        if (type_reference == nullptr) {
          LOG(ERROR) << "Failed to obtain type reference from " << signature;
          return nullptr;
        }

        return std::unique_ptr<ClassRef>(
            new ClassRef { *internal_name, type_reference });
      });
}


const ConstantPool::StringRef* ConstantPool::GetString(int index) {
  return Fetch<StringRef>(
      GetConstantPoolItem(index, JVM_CONSTANT_String),
      [this] (uint8 type, ByteSource data) -> std::unique_ptr<StringRef> {
        const int index = data.ReadUInt16BE(1);
        if (data.is_error()) {
          return nullptr;
        }

        const Utf8Ref* utf8 = GetUtf8(index);
        if (utf8 == nullptr) {
          return nullptr;
        }

        // Call "c_str()" to append the NULL terminator.
        JniLocalRef str(jni()->NewStringUTF(utf8->str().c_str()));
        if (str == nullptr) {
          LOG(ERROR) << "UTF-8 string could not be constructed";
          return nullptr;
        }

        return std::unique_ptr<StringRef>(
            new StringRef { *utf8, JniNewGlobalRef(str.get()) });
      });
}


const ConstantPool::NameAndTypeRef* ConstantPool::GetNameAndType(int index) {
  return Fetch<NameAndTypeRef>(
      GetConstantPoolItem(index, JVM_CONSTANT_NameAndType),
      [this] (uint8 cp_type, ByteSource data)
          -> std::unique_ptr<NameAndTypeRef> {
        const int name_index = data.ReadUInt16BE(1);
        const int type_index = data.ReadUInt16BE(3);
        if (data.is_error()) {
          return nullptr;
        }

        const Utf8Ref* name = GetUtf8(name_index);
        const Utf8Ref* type = GetUtf8(type_index);
        if ((name == nullptr) || (type == nullptr)) {
          return nullptr;
        }

        return std::unique_ptr<NameAndTypeRef>(
            new NameAndTypeRef { *name, *type });
      });
}


const ConstantPool::FieldRef* ConstantPool::GetField(int index) {
  return Fetch<FieldRef>(
      GetConstantPoolItem(index, JVM_CONSTANT_Fieldref),
      [this] (uint8 type, ByteSource data) -> std::unique_ptr<FieldRef> {
        const int owner_index = data.ReadUInt16BE(1);
        const int field_index = data.ReadUInt16BE(3);
        if (data.is_error()) {
          return nullptr;
        }

        std::unique_ptr<FieldRef> field(new FieldRef);

        field->owner = GetClass(owner_index);
        const NameAndTypeRef* names = GetNameAndType(field_index);
        if ((field->owner == nullptr) || (names == nullptr)) {
          return nullptr;
        }

        field->field_name = names->name.str();

        field->field_type = JSignatureToType(
            class_indexer_,
            JSignatureFromSignature(names->type.str()));
        if (field->field_type == nullptr) {
          LOG(ERROR) << "Failed to obtain field type reference from "
                     << names->type.str();
          return nullptr;
        }

        // Find the class that defined the field. If the class is not
        // available, we don't fail and keep "is_found" false. This is a valid
        // situation when the needs to be handled gracefully (as opposed to
        // internal error).
        field->owner_cls = JniNewGlobalRef(field->owner->type->FindClass());
        if (field->owner_cls != nullptr) {
          // Obtain the JVM field ID. Use "c_str()" to append the NULL
          // terminator. We can't tell at this point if the field is static or
          // instance, so we try to load both.
          jfieldID instance_field_id = jni()->GetFieldID(
              static_cast<jclass>(field->owner_cls.get()),
              field->field_name.c_str(),
              names->type.str().c_str());
          if (CatchOr("GetFieldID", nullptr).HasException()) {
            instance_field_id = nullptr;
          }

          jfieldID static_field_id = jni()->GetStaticFieldID(
              static_cast<jclass>(field->owner_cls.get()),
              field->field_name.c_str(),
              names->type.str().c_str());
          if (CatchOr("GetStaticFieldID", nullptr).HasException()) {
            static_field_id = nullptr;
          }

          if ((instance_field_id == nullptr) && (static_field_id == nullptr)) {
            LOG(ERROR) << "Field not available, class = "
                       << field->owner->type->GetSignature()
                       << ", field name = " << field->field_name
                       << ", field type = " << names->type.str();
            return nullptr;
          }

          // Java doesn't allow static and instance field with the same name
          // in the same class. If we got it, there is some sort of an error.
          if ((instance_field_id != nullptr) && (static_field_id != nullptr)) {
            LOG(ERROR) << "Both static and instance field found";
            return nullptr;
          }

          if (static_field_id != nullptr) {
            field->field_id = static_field_id;
            field->is_static = true;
          } else {
            field->field_id = instance_field_id;
            field->is_static = false;
          }

          field->is_found = true;
        }

        return field;
      });
}


const ConstantPool::MethodRef* ConstantPool::GetMethod(int index) {
  return Fetch<MethodRef>(
      GetConstantPoolItem(index, nullptr),
      [this] (uint8 type, ByteSource data) -> std::unique_ptr<MethodRef> {
        if ((type != JVM_CONSTANT_Methodref) &&
            (type != JVM_CONSTANT_InterfaceMethodref)) {
          LOG(ERROR) << "Unexpected constant pool item type "
                     << static_cast<int>(type);
          return nullptr;
        }

        const int owner_index = data.ReadUInt16BE(1);
        const int method_index = data.ReadUInt16BE(3);
        if (data.is_error()) {
          return nullptr;
        }

        std::unique_ptr<MethodRef> method(new MethodRef);

        method->owner = GetClass(owner_index);
        const NameAndTypeRef* names = GetNameAndType(method_index);
        if ((method->owner == nullptr) || (names == nullptr)) {
          return nullptr;
        }

        if (!ParseJMethodSignature(names->type.str(),
                                   &method->method_signature)) {
          LOG(ERROR) << "bad method signature " << names->type.str();
          return nullptr;
        }

        // Find the class that defined the method. If the class is not
        // available, we don't fail and keep "is_found" false. This is a valid
        // situation that needs to be handled gracefully (as opposed to
        // internal error).
        method->owner_cls = JniNewGlobalRef(method->owner->type->FindClass());
        if (method->owner_cls == nullptr) {
          return method;
        }

        // The name and signature of the method to search for.
        std::string method_name = names->name.str();
        std::string method_signature = names->type.str();

        // Search for the JVM method ID using name and signature. Use "c_str()"
        // to append the NULL terminator. We can't tell at this point if the
        // method is static or instance, so we try to load both.
        jmethodID instance_method_id = jni()->GetMethodID(
            static_cast<jclass>(method->owner_cls.get()),
            method_name.c_str(),
            method_signature.c_str());
        if (CatchOr("GetMethodID", nullptr).HasException()) {
          instance_method_id = nullptr;
        }

        jmethodID static_method_id = jni()->GetStaticMethodID(
            static_cast<jclass>(method->owner_cls.get()),
            method_name.c_str(),
            method_signature.c_str());
        if (CatchOr("GetStaticMethodID", nullptr).HasException()) {
          static_method_id = nullptr;
        }

        // Java doesn't allow static and instance method with the same name
        // in the same class. If we got it, there is some sort of an error.
        if ((instance_method_id != nullptr) &&
            (static_method_id != nullptr)) {
          LOG(ERROR) << "Both static and instance method found";
          return nullptr;
        }

        if (instance_method_id == nullptr && static_method_id == nullptr) {
          // The method with the given signature is not found in the target
          // class. Typically, this means an internal error.
          //
          // This can also happen if the target method is signature polymorphic.
          // In this case, the signature at the call site will not match the
          // signature of the actual method.
          if (!IsPolymorphicMethod(
              static_cast<jclass>(method->owner_cls.get()),
              method->owner->type->GetSignature(),
              method_name,
              &method_signature,
              &instance_method_id,
              &static_method_id)) {
            LOG(ERROR) << "Method not available, class = "
                       << method->owner->type->GetSignature()
                       << ", method name = " << method_name
                       << ", method signature = " << method_signature;
            return nullptr;
          }

          // Use the adjusted (polymorphic) method signature and method ids.
        }

        ClassMetadataReader::Method metadata;
        metadata.class_signature = {
          method->owner->type->GetType(),
          method->owner->type->GetSignature()
        };
        metadata.name = method_name;
        metadata.signature = method_signature;
        metadata.modifiers = (static_method_id == nullptr) ? 0 : JVM_ACC_STATIC;
        method->metadata = std::move(metadata);
        method->method_id = static_method_id ? : instance_method_id;

        method->is_found = true;

        return method;
      });
}


ClassFile::Method::Method(ClassFile* class_file)
    : class_file_(class_file) {
}


bool ClassFile::Method::Load(int offset, int* method_size) {
  *method_size = 0;

  ByteSource data = class_file_->GetData();
  ConstantPool* constant_pool = class_file_->constant_pool();

  method_modifiers_ = data.ReadUInt16BE(offset);

  const ConstantPool::Utf8Ref* name =
      constant_pool->GetUtf8(data.ReadUInt16BE(offset + 2));
  if (name == nullptr) {
    return false;
  }

  name_ = *name;

  const ConstantPool::Utf8Ref* signature =
      constant_pool->GetUtf8(data.ReadUInt16BE(offset + 4));
  if (signature == nullptr) {
    return false;
  }

  signature_ = *signature;

  JMethodSignature parsed_signature;
  if (!ParseJMethodSignature(signature_.str(), &parsed_signature)) {
    LOG(ERROR) << "Failed to parse method signature " << signature_.str();
    return false;
  }

  return_type_ = JSignatureToType(
      class_file_->class_indexer(),
      parsed_signature.return_type);
  if (return_type_ == nullptr) {
    LOG(ERROR) << "Invalid method return type";
    return false;
  }

  const uint16_t attributes_count = data.ReadUInt16BE(offset + 6);
  *method_size += 8;

  for (int j = 0; j < attributes_count; ++j) {
    const ConstantPool::Utf8Ref* attribute_name =
        constant_pool->GetUtf8(data.ReadUInt16BE(offset + *method_size));
    if (attribute_name == nullptr) {
      return false;
    }

    if (*attribute_name == "Code") {
      const int code_offset_ = offset + *method_size + 6;
      const int code_size_ = data.ReadInt32BE(code_offset_ + 4);
      code_ = data.sub(code_offset_ + 8, code_size_);

      max_stack_ = data.ReadUInt16BE(code_offset_);
      max_locals_ = data.ReadUInt16BE(code_offset_ + 2);

      const int table_start = code_offset_ + 8 + code_size_;
      const int table_size_ = data.ReadUInt16BE(table_start);
      exception_table_ = data.sub(table_start + 2, table_size_ * 8);
    }

    *method_size += 6 + data.ReadInt32BE(offset + *method_size + 2);
  }

  return !data.is_error();
}


Nullable<ClassFile::TryCatchBlock> ClassFile::Method::GetTryCatchBlock(
    int index) {
  // Size of a single row in exception table in .class file.
  constexpr int kExceptionTableRowSize = 8;

  ByteSource entry_reader = exception_table_.sub(index * kExceptionTableRowSize,
                                                 kExceptionTableRowSize);

  const uint16_t type_constant_pool_index = entry_reader.ReadUInt16BE(6);

  const ConstantPool::ClassRef* type = nullptr;
  if (type_constant_pool_index != 0) {
    type = class_file_->constant_pool()->GetClass(type_constant_pool_index);
    if (type == nullptr) {
      return nullptr;
    }
  }

  TryCatchBlock try_catch_block;
  try_catch_block.begin_offset = entry_reader.ReadUInt16BE(0);
  try_catch_block.end_offset = entry_reader.ReadUInt16BE(2);
  try_catch_block.handler_offset = entry_reader.ReadUInt16BE(4);
  try_catch_block.type = type;

  if (entry_reader.is_error()) {
    return nullptr;
  }

  return try_catch_block;
}


Nullable<ClassFile::Instruction> ClassFile::Method::GetInstruction(int offset) {
  ByteSource code = code_.sub(offset, code_.size());

  Instruction instruction;
  instruction.opcode = code.ReadUInt8(0);
  instruction.offset = offset;

  switch (GetInstructionType(instruction.opcode)) {
    case InstructionType::NO_ARG:
      instruction.next_instruction_offset = offset + 1;
      break;

    case InstructionType::IMPLICIT_LOCAL_VAR_INDEX:
      // Convert instructions like istore_3 to istore(3).
      if (instruction.opcode > JVM_OPC_istore) {
        uint8_t d = instruction.opcode - JVM_OPC_istore_0;
        instruction.int_operand = d & 0x03;
        instruction.opcode = JVM_OPC_istore + (d >> 2);
      } else {
        uint8_t d = instruction.opcode - JVM_OPC_iload_0;
        instruction.int_operand = d & 0x03;
        instruction.opcode = JVM_OPC_iload + (d >> 2);
      }

      instruction.next_instruction_offset = offset + 1;
      break;

    case InstructionType::LABEL:
      instruction.int_operand = code.ReadInt16BE(1);
      instruction.next_instruction_offset = offset + 3;
      break;

    case InstructionType::LABEL_W:
      instruction.int_operand = code.ReadInt32BE(1);
      instruction.next_instruction_offset = offset + 5;
      break;

    case InstructionType::WIDE:
      instruction.opcode = code.ReadUInt8(1);
      if (instruction.opcode == JVM_OPC_iinc) {
        instruction.iinc_operand.local_index = code.ReadUInt16BE(2);
        instruction.iinc_operand.increment = code.ReadInt16BE(4);
        instruction.next_instruction_offset = offset + 6;
      } else {
        instruction.int_operand = code.ReadUInt16BE(2);
        instruction.next_instruction_offset = offset + 4;
      }
      break;

    case InstructionType::TABLESWITCH: {
      auto& operand = instruction.table_switch_operand;

      // Skips opcode and 0 to 3 padding bytes.
      const int operand_offset = 4 - (offset & 3);
      int32_t low = code.ReadInt32BE(operand_offset + 4);
      int32_t high = code.ReadInt32BE(operand_offset + 8);
      int32_t table_offset = operand_offset + 12;
      int32_t table_size = (high - low + 1) * 4;

      operand.low = low;
      operand.default_handler_offset = code.ReadInt32BE(operand_offset);
      operand.table = TableSwitchTable(code.sub(table_offset, table_size));
      instruction.next_instruction_offset = offset + table_offset + table_size;
      break;
    }

    case InstructionType::LOOKUPSWITCH: {
      auto& operand = instruction.lookup_switch_operand;

      // Skips opcode and 0 to 3 padding bytes.
      const int operand_offset = 4 - (offset & 3);
      int32_t table_offset = operand_offset + 8;
      int32_t table_size = code.ReadInt32BE(operand_offset + 4) * 8;

      operand.default_handler_offset = code.ReadInt32BE(operand_offset);
      operand.table = LookupSwitchTable(code.sub(table_offset, table_size));
      instruction.next_instruction_offset = offset + table_offset + table_size;
      break;
    }

    case InstructionType::LOCAL_VAR_INDEX:
      instruction.int_operand = code.ReadUInt8(1);
      instruction.next_instruction_offset = offset + 2;
      break;

    case InstructionType::INT8:
      instruction.int_operand = code.ReadInt8(1);
      instruction.next_instruction_offset = offset + 2;
      break;

    case InstructionType::INT16:
      instruction.int_operand = code.ReadInt16BE(1);
      instruction.next_instruction_offset = offset + 3;
      break;

    case InstructionType::LDC:
      instruction.int_operand = code.ReadUInt8(1);
      instruction.next_instruction_offset = offset + 2;
      break;

    case InstructionType::LDC_W:
      instruction.opcode = JVM_OPC_ldc;  // Constant pool item disambiguates.
      instruction.int_operand = code.ReadUInt16BE(1);
      instruction.next_instruction_offset = offset + 3;
      break;

    case InstructionType::FIELD: {
      const ConstantPool::FieldRef* field =
          class_file_->constant_pool()->GetField(code.ReadUInt16BE(1));
      if (field == nullptr) {
        return nullptr;  // Error.
      }

      instruction.field_operand = field;
      instruction.next_instruction_offset = offset + 3;
      break;
    }

    case InstructionType::METHOD: {
      const ConstantPool::MethodRef* method =
          class_file_->constant_pool()->GetMethod(code.ReadUInt16BE(1));
      if (method == nullptr) {
        return nullptr;  // Error.
      }

      instruction.method_operand = method;
      instruction.next_instruction_offset = offset + 3;
      break;
    }

    case InstructionType::INVOKEINTERFACE: {
      const ConstantPool::MethodRef* method =
          class_file_->constant_pool()->GetMethod(code.ReadUInt16BE(1));
      if (method == nullptr) {
        return nullptr;  // Error.
      }

      instruction.method_operand = method;
      instruction.next_instruction_offset = offset + 5;
      break;
    }

    case InstructionType::TYPE: {
      const ConstantPool::ClassRef* type =
          class_file_->constant_pool()->GetClass(code.ReadUInt16BE(1));
      if (type == nullptr) {
        return nullptr;  // Error.
      }

      instruction.type_operand = type;
      instruction.next_instruction_offset = offset + 3;
      break;
    }

    case InstructionType::IINC:
      instruction.iinc_operand.local_index = code.ReadUInt8(1);
      instruction.iinc_operand.increment = code.ReadInt8(2);
      instruction.next_instruction_offset = offset + 3;
      break;

    // TODO: implement support for INVOKE_DYNAMIC instruction.
    case InstructionType::INVOKEDYNAMIC:
      instruction.next_instruction_offset = offset + 5;
      break;

    // TODO: implement support for MULTIANEWARRAY instruction.
    case InstructionType::MULTIANEWARRAY:
      instruction.next_instruction_offset = offset + 4;
      break;

    default:
      return nullptr;
  }

  if (code.is_error()) {
    return nullptr;
  }

  return instruction;
}

ClassFile::InstructionType ClassFile::Method::GetInstructionType(
    uint8_t opcode) {
  static InstructionType* map = BuildInstructionTypeMap();
  return map[opcode];
}

}  // namespace cdbg
}  // namespace devtools

