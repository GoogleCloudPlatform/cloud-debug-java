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

#include "fake_jni.h"

#include "mock_jni_env.h"
#include "mock_jvmti_env.h"

using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::IsNull;
using testing::Matcher;
using testing::MatcherInterface;
using testing::MatchResultListener;
using testing::NotNull;
using testing::Test;
using testing::Return;
using testing::ReturnNew;
using testing::WithArgs;
using testing::Invoke;
using testing::InvokeArgument;
using testing::SetArgPointee;
using testing::SetArrayArgument;

namespace devtools {
namespace cdbg {

// Metadata of stock classes.
static const struct StockClassMetadata {
  FakeJni::StockClass stock_class;
  FakeJni::ClassMetadata class_metadata;
} kStockClassesMetadata[] = {
    {FakeJni::StockClass::Object,
     {std::string(), "Ljava/lang/Object;", std::string()}},
    {FakeJni::StockClass::String,
     {std::string(), "Ljava/lang/String;", std::string()}},
    {FakeJni::StockClass::StringArray,
     {std::string(), "[Ljava/lang/String;", std::string()}},
    {FakeJni::StockClass::IntArray, {std::string(), "[I", std::string()}},
    {FakeJni::StockClass::BigDecimal,
     {
         std::string(),
         "Ljava/math/BigDecimal;",
         std::string(),
     }},
    {FakeJni::StockClass::BigInteger,
     {
         std::string(),
         "Ljava/math/BigInteger;",
         std::string(),
     }},
    {FakeJni::StockClass::Iterable,
     {
         std::string(),
         "Ljava/lang/Iterable;",
         std::string(),
     }},
    {FakeJni::StockClass::Map,
     {
         std::string(),
         "Ljava/util/Map;",
         std::string(),
     }},
    {FakeJni::StockClass::MapEntry,
     {
         std::string(),
         "Ljava/util/Map$Entry;",
         std::string(),
     }},
    {FakeJni::StockClass::MyClass1,
     {"MyClass1.java", "Lcom/prod/MyClass1;", std::string()}},
    {FakeJni::StockClass::MyClass2,
     {"MyClass2.java", "Lcom/prod/MyClass2;", std::string()}},
    {FakeJni::StockClass::MyClass3,
     {"MyClass3.java", "Lcom/prod/MyClass3;", std::string()}},
};

// Duplicates string to be subsequently freed with delete[].
static char* AllocateJvmtiString(const std::string& s) {
  if (s.empty()) {
    return nullptr;
  }

  char* buffer = new char[s.size() + 1];

  std::copy(s.begin(), s.end(), buffer);
  buffer[s.size()] = '\0';

  return buffer;
}

FakeJni::FakeJni()
    : jvmti_(new testing::NiceMock<MockJvmtiEnv>),
      jni_(new testing::NiceMock<MockJNIEnv>),
      is_external_jvmti_(false),
      is_external_jni_(false) {
  SetUp();
}


FakeJni::FakeJni(MockJNIEnv* external_jni)
    : jvmti_(new testing::NiceMock<MockJvmtiEnv>),
      jni_(external_jni),
      is_external_jvmti_(false),
      is_external_jni_(true) {
  SetUp();
}


FakeJni::FakeJni(
    MockJvmtiEnv* external_jvmti)
    : jvmti_(external_jvmti),
      jni_(new testing::NiceMock<MockJNIEnv>),
      is_external_jvmti_(true),
      is_external_jni_(false) {
  SetUp();
}


FakeJni::FakeJni(
    MockJvmtiEnv* external_jvmti,
    MockJNIEnv* external_jni)
    : jvmti_(external_jvmti),
      jni_(external_jni),
      is_external_jvmti_(true),
      is_external_jni_(true) {
  SetUp();
}


FakeJni::~FakeJni() {
  // Release stock classes.
  for (const auto& stock_class : stock_) {
    jni_->DeleteLocalRef(stock_class.second);
  }

  // Report all leaking references and release memory.
  for (FakeRef* ref : refs_) {
    if (!ref->is_internal) {
      ADD_FAILURE() << "Leaking reference (type "
                    << static_cast<int>(ref->reference_kind)
                    << ") to object " << ref->obj;
    }

    delete ref;
  }

  for (FakeObject* obj : obj_) {
    delete obj;
  }

  if (!is_external_jni_) {
    delete jni_;
  }

  if (!is_external_jvmti_) {
    delete jvmti_;
  }
}


void FakeJni::SetUp() {
  SetUpJniMocks();
  SetUpJvmtiMocks();

  for (const auto& stock_class_metadata : kStockClassesMetadata) {
    stock_[stock_class_metadata.stock_class] =
        CreateNewClass(stock_class_metadata.class_metadata);
  }
}


void FakeJni::SetUpJniMocks() {
  // JNI documentation explicitly states that calling NewLocalRef(nullptr)
  // returns nullptr.
  ON_CALL(*jni_, NewLocalRef(_))
      .WillByDefault(Invoke([this] (jobject ref) -> jobject {
        if (ref == nullptr) {
          // NewLocalRef(NULL) is supposed to return NULL.
          return nullptr;
        }

        FakeObject* obj = Dereference(ref);
        if (obj == nullptr) {
          // NewLocalRef returns NULL for reclaimed weak global references.
          return nullptr;
        }

        return CreateNewRef(JVariant::ReferenceKind::Local, obj);
      }));

  // JNI documentation is not clear about NewGlobalRef(nullptr).
  ON_CALL(*jni_, NewGlobalRef(NotNull()))
      .WillByDefault(Invoke([this] (jobject ref) -> jobject {
        FakeObject* obj = Dereference(ref);
        if (obj == nullptr) {
          // NewLocalRef returns NULL for reclaimed weak global references.
          return nullptr;
        }

        return CreateNewRef(JVariant::ReferenceKind::Global, obj);
      }));

  // JNI documentation is not clear about NewWeakGlobalRef(nullptr).
  ON_CALL(*jni_, NewWeakGlobalRef(NotNull()))
      .WillByDefault(Invoke([this] (jobject ref) -> jobject {
        FakeObject* obj = Dereference(ref);
        if (obj == nullptr) {
          // NewLocalRef returns NULL for reclaimed weak global references.
          return nullptr;
        }

        return CreateNewRef(JVariant::ReferenceKind::WeakGlobal, obj);
      }));

  ON_CALL(*jni_, DeleteLocalRef(_))
      .WillByDefault(Invoke([this] (jobject ref) {
        DeleteRef(JVariant::ReferenceKind::Local, ref);
      }));

  ON_CALL(*jni_, DeleteGlobalRef(_))
      .WillByDefault(Invoke([this] (jobject ref) {
        DeleteRef(JVariant::ReferenceKind::Global, ref);
      }));

  ON_CALL(*jni_, DeleteWeakGlobalRef(_))
      .WillByDefault(Invoke([this] (jobject ref) {
        DeleteRef(JVariant::ReferenceKind::WeakGlobal, ref);
      }));

  ON_CALL(*jni_, GetObjectRefType(NotNull()))
      .WillByDefault(Invoke([this] (jobject ref) {
        FakeRef* fake_ref = reinterpret_cast<FakeRef*>(ref);
        auto it_ref = refs_.find(fake_ref);
        if (it_ref == refs_.end()) {
          ADD_FAILURE() << "Invalid reference";
          return JNIInvalidRefType;
        }

        return static_cast<jobjectRefType>(fake_ref->reference_kind);
      }));

  ON_CALL(*jni_, GetObjectClass(NotNull()))
      .WillByDefault(Invoke([this] (jobject ref) {
        FakeObject* obj = Dereference(ref);
        CHECK(obj != nullptr);

        return static_cast<jclass>(
            CreateNewRef(JVariant::ReferenceKind::Local, obj->cls));
      }));

  ON_CALL(*jni_, IsSameObject(_, _))
      .WillByDefault(Invoke([this] (jobject ref1, jobject ref2) {
        // Take care of invalidated weak global references.
        FakeObject* obj1 = nullptr;
        if (ref1 != nullptr) {
          obj1 = Dereference(ref1);
        }

        FakeObject* obj2 = nullptr;
        if (ref2 != nullptr) {
          obj2 = Dereference(ref2);
        }

        return obj1 == obj2;
      }));

  ON_CALL(*jni_, IsInstanceOf(_, _))
      .WillByDefault(Invoke([this] (jobject ref1, jclass ref2) {
        if (ref1 == nullptr) {
          return true;
        }

        return Dereference(ref1)->cls == Dereference(ref2);
      }));

  ON_CALL(*jni_, IsAssignableFrom(_, _))
      .WillByDefault(Invoke([this] (jclass from_cls, jclass to_cls) {
        const auto& from_metadata = MutableClassMetadata(from_cls);
        const auto& to_metadata = MutableClassMetadata(to_cls);

        return from_metadata.signature == to_metadata.signature;
      }));

  ON_CALL(*jni_, GetMethodID(NotNull(), NotNull(), NotNull()))
      .WillByDefault(Invoke([this](jclass cls, const char* name,
                                   const char* signature) -> jmethodID {
        ClassMetadata& metadata = DereferenceClass(cls);
        for (const auto& method : metadata.methods) {
          if (!method.metadata.is_static() &&
              (method.metadata.name == name) &&
              (method.metadata.signature == signature)) {
            return method.id;
          }
        }

        return nullptr;  // Method not found.
      }));

  ON_CALL(*jni_, GetStaticMethodID(NotNull(), NotNull(), NotNull()))
      .WillByDefault(Invoke([this](jclass cls, const char* name,
                                   const char* signature) -> jmethodID {
        ClassMetadata& metadata = DereferenceClass(cls);
        for (const auto& method : metadata.methods) {
          if (method.metadata.is_static() &&
              (method.metadata.name == name) &&
              (method.metadata.signature == signature)) {
            return method.id;
          }
        }

        return nullptr;  // Method not found.
      }));

  ON_CALL(*jni_, NewString(NotNull(), _))
      .WillByDefault(Invoke([this] (
          const jchar* str,
          jsize length) -> jstring {
        std::vector<jchar> content;
        if (length > 0) {
          content = std::vector<jchar>(str, str + length);
        }

        // Special string to simulate out of memory conditions.
        std::string out_of_memory_string = "magic-memory-loss";
        std::vector<jchar> out_of_memory_vector;
        out_of_memory_vector.assign(
            out_of_memory_string.begin(),
            out_of_memory_string.end());

        if (out_of_memory_vector == content) {
          return nullptr;
        }

        return CreateNewJavaString(content);
      }));

  ON_CALL(*jni_, NewStringUTF(NotNull()))
      .WillByDefault(Invoke([this] (const char* str) {
        return CreateNewJavaString(str);
      }));

  ON_CALL(*jni_, GetStringLength(NotNull()))
      .WillByDefault(Invoke([this](jstring ref) {
        FakeObject* obj = Dereference(ref);
        CHECK(obj != nullptr);
        auto it = jstring_data_.find(obj);
        CHECK(it != jstring_data_.end());
        return it->second.size();
      }));

  ON_CALL(*jni_, GetStringCritical(NotNull(), _))
      .WillByDefault(Invoke([this](jstring ref, jboolean* is_copy) {
        FakeObject* obj = Dereference(ref);
        CHECK(obj != nullptr);
        auto it = jstring_data_.find(obj);
        CHECK(it != jstring_data_.end());

        if (is_copy != nullptr) {
          *is_copy = false;
        }

        if (it->second.empty()) {
          static jchar empty_buffer[] = { };
          return empty_buffer;
        }

        return &it->second[0];
      }));

  ON_CALL(*jni_, GetStringUTFRegion(NotNull(), _, _, NotNull()))
      .WillByDefault(
          Invoke([this](jstring ref, jsize start, jsize len, char* buf) {
            FakeObject* obj = Dereference(ref);
            CHECK(obj != nullptr);
            auto it = jstring_data_.find(obj);
            CHECK(it != jstring_data_.end());

            if (it->second.empty()) {
              *buf = '\0';
              return;
            }

            len += start;
            const std::vector<jchar>& jstr = it->second;
            for (int i = start; i < len; ++i) {
              if (jstr[i] < 0x80) {
                *buf = static_cast<char>(jstr[i]);
              } else {
                *buf = static_cast<char>(jstr[i] & 0x7f);
                buf++;
                *buf = '*';
              }
              ++buf;
            }
          }));

  ON_CALL(*jni_, ReleaseStringCritical(NotNull(), NotNull()))
      .WillByDefault(Return());

  ON_CALL(*jni_, GetStringUTFChars(NotNull(), _))
      .WillByDefault(Invoke([this](jstring ref, jboolean* is_copy) {
        FakeObject* obj = Dereference(ref);
        CHECK(obj != nullptr);
        auto it = jstring_data_.find(obj);
        CHECK(it != jstring_data_.end());

        if (is_copy != nullptr) {
          *is_copy = true;
        }

        const std::vector<jchar>& unicode_chars = it->second;

        // Just cast jchar to char, don't really care about UTF-8 encoding
        // of non-ASCII characters.
        std::string utf_chars(unicode_chars.begin(), unicode_chars.end());

        char* buffer = new char[unicode_chars.size() + 1];
        std::copy(unicode_chars.begin(), unicode_chars.end(), buffer);
        buffer[unicode_chars.size()] = '\0';

        return buffer;
      }));

  ON_CALL(*jni_, ReleaseStringUTFChars(NotNull(), NotNull()))
      .WillByDefault(
          Invoke([] (jstring ref, const char* buffer) {
            delete[] buffer;
          }));

  ON_CALL(*jni_, Throw(NotNull()))
      .WillByDefault(
          Invoke([this] (jthrowable exception) {
            pending_exception_ = JniNewLocalRef(exception);
            return 0;
          }));

  ON_CALL(*jni_, ExceptionCheck())
      .WillByDefault(
          Invoke([this] () {
            return pending_exception_ != nullptr;
          }));

  ON_CALL(*jni_, ExceptionOccurred())
      .WillByDefault(
          Invoke([this] () {
            return static_cast<jthrowable>(
                JniNewLocalRef(pending_exception_.get()).release());
          }));

  ON_CALL(*jni_, ExceptionClear())
      .WillByDefault(
          Invoke([this] () {
            pending_exception_ = nullptr;
          }));

  ON_CALL(*jni_, FindClass(_))
      .WillByDefault(
          Invoke([this] (const char* name) {
            return FindClassByShortSignature(name);
          }));
}

void FakeJni::SetUpJvmtiMocks() {
  ON_CALL(*jvmti_, Deallocate(NotNull()))
      .WillByDefault(Invoke([] (unsigned char* p) {
        delete[] reinterpret_cast<char*>(p);
        return JVMTI_ERROR_NONE;
      }));

  ON_CALL(*jvmti_, GetObjectHashCode(_, NotNull()))
      .WillByDefault(Invoke([] (jobject ref, jint* hash_code) {
        // Deliberately cause hash table contention through inherently bad
        // hash function.
        *hash_code = reinterpret_cast<uint64>(ref) & 0x03;
        return JVMTI_ERROR_NONE;
      }));

  ON_CALL(*jvmti_, GetClassStatus(NotNull(), NotNull()))
      .WillByDefault(DoAll(
          SetArgPointee<1>(JVMTI_CLASS_STATUS_PREPARED),
          Return(JVMTI_ERROR_NONE)));

  ON_CALL(*jvmti_, GetSourceFileName(NotNull(), NotNull()))
      .WillByDefault(Invoke([this] (jclass ref, char** file_name) {
        const ClassMetadata& metadata = DereferenceClass(ref);

        if (metadata.file_name.empty()) {
          return JVMTI_ERROR_ABSENT_INFORMATION;
        }

        *file_name = AllocateJvmtiString(metadata.file_name);

        return JVMTI_ERROR_NONE;
      }));

  ON_CALL(*jvmti_, GetClassSignature(NotNull(), _, _))
      .WillByDefault(Invoke([this] (
          jclass ref,
          char** class_signature,
          char** generic_signature) {
        const ClassMetadata& metadata = DereferenceClass(ref);

        if (metadata.signature.empty()) {
          return JVMTI_ERROR_ABSENT_INFORMATION;
        }

        if (class_signature != nullptr) {
          *class_signature = AllocateJvmtiString(metadata.signature);
        }

        if (generic_signature != nullptr) {
          *generic_signature = AllocateJvmtiString(metadata.generic);
        }

        return JVMTI_ERROR_NONE;
      }));

  ON_CALL(*jvmti_, GetLoadedClasses(NotNull(), NotNull()))
      .WillByDefault(Invoke([this] (
          jint* class_count,
          jclass** classes) {
        std::vector<FakeObject*> non_array_classes;
        for (const auto& entry : cls_) {
          auto signature = JSignatureFromSignature(entry.second.signature);
          if (!IsArrayObjectType(signature)) {
            non_array_classes.push_back(entry.first);
          }
        }

        *class_count = non_array_classes.size();
        *classes = reinterpret_cast<jclass*>(
            new char[sizeof(jclass) * non_array_classes.size()]);

        jclass* p = *classes;
        for (FakeObject* entry : non_array_classes) {
          // Although JVMTI documentation specifies that GetLoadedClasses
          // returns array of local references, this seems to be a mistake.
          // None of the other projects seems to treat the return values as
          // something that needs to be managed explicitly. Also the number of
          // loaded classes will almost certainly exceed the number of local
          // variable slots. Therefore we are assuming that it's a mistake in
          // JVMTI documentation.
          *p = static_cast<jclass>(
              CreateNewRef(JVariant::ReferenceKind::Local, entry));
          reinterpret_cast<FakeRef*>(*p)->is_internal = true;

          ++p;
        }

        return JVMTI_ERROR_NONE;
      }));

  ON_CALL(*jvmti_, GetClassMethods(NotNull(), NotNull(), NotNull()))
      .WillByDefault(Invoke([this] (
          jclass ref,
          jint* count,
          jmethodID** methods) {
        ClassMetadata& metadata = DereferenceClass(ref);

        *methods = reinterpret_cast<jmethodID*>(
            new char[sizeof(jmethodID) * metadata.methods.size()]);

        *count = metadata.methods.size();
        for (int i = 0; i < metadata.methods.size(); ++i) {
          CHECK(metadata.methods[i].id != nullptr);
          (*methods)[i] = metadata.methods[i].id;
        }

        return JVMTI_ERROR_NONE;
      }));

  ON_CALL(*jvmti_, GetMethodName(NotNull(), _, _, _))
      .WillByDefault(Invoke([this] (
          jmethodID method_id,
          char** name,
          char** signature,
          char** generic) {
        MethodMetadata& method_metadata = MutableMethodMetadata(method_id);

        if (name != nullptr) {
          *name = AllocateJvmtiString(method_metadata.metadata.name);
        }

        if (signature != nullptr) {
          *signature = AllocateJvmtiString(method_metadata.metadata.signature);
        }

        if (generic != nullptr) {
          *generic = nullptr;
        }

        return JVMTI_ERROR_NONE;
      }));

  ON_CALL(*jvmti_, GetMethodDeclaringClass(NotNull(), NotNull()))
      .WillByDefault(Invoke([this] (jmethodID method_id, jclass* cls) {
        MethodMetadata& method_metadata = MutableMethodMetadata(method_id);
        *cls = FindClassBySignature(
            method_metadata.metadata.class_signature.object_signature);
        return JVMTI_ERROR_NONE;
      }));

  ON_CALL(*jvmti_, GetLineNumberTable(NotNull(), NotNull(), NotNull()))
      .WillByDefault(Invoke([this] (
          jmethodID method_id,
          jint* entry_count,
          jvmtiLineNumberEntry** table) {
        MethodMetadata& method_metadata = MutableMethodMetadata(method_id);

        jint count = method_metadata.line_number_table.size();
        if (count == 0) {
          return JVMTI_ERROR_ABSENT_INFORMATION;
        }

        *entry_count = count;

        *table = reinterpret_cast<jvmtiLineNumberEntry*>(
            new char[sizeof(jvmtiLineNumberEntry) *
                     method_metadata.line_number_table.size()]);
        std::copy(
            method_metadata.line_number_table.begin(),
            method_metadata.line_number_table.end(),
            *table);

        return JVMTI_ERROR_NONE;
      }));
}


JNIEnv* FakeJni::jni() const {
  return jni_;
}


jvmtiEnv* FakeJni::jvmti() const {
  return jvmti_;
}


jclass FakeJni::GetStockClass(StockClass stock_class) {
  CHECK(stock_.find(stock_class) != stock_.end());

  return stock_[stock_class];
}


FakeJni::ClassMetadata& FakeJni::MutableClassMetadata(jclass cls) {
  return DereferenceClass(cls);
}


FakeJni::ClassMetadata& FakeJni::MutableStockClassMetadata(
    FakeJni::StockClass stock_class) {
  return MutableClassMetadata(GetStockClass(stock_class));
}


FakeJni::MethodMetadata& FakeJni::MutableMethodMetadata(jmethodID method) {
  for (auto& entry : cls_) {
    for (auto& method_metadata : entry.second.methods) {
      if (method_metadata.id == method) {
        return method_metadata;
      }
    }
  }

  LOG(FATAL) << "Method " << method << " not found in the fake classes list";
}

jclass FakeJni::FindClassBySignature(const std::string& class_signature) {
  for (auto& entry : cls_) {
    if (entry.second.signature == class_signature) {
      return static_cast<jclass>(
          CreateNewRef(JVariant::ReferenceKind::Local, entry.first));
    }
  }

  return nullptr;
}

jclass FakeJni::FindClassByShortSignature(const std::string& class_signature) {
  for (auto& entry : cls_) {
    // Arrays are not supported by signature conversion; it calls CHECK on them
    if (entry.second.signature[0] != '[') {
      if (TrimJObjectSignature(entry.second.signature) ==
          class_signature) {
        return static_cast<jclass>(
            CreateNewRef(JVariant::ReferenceKind::Local, entry.first));
      }
    }
  }

  return nullptr;
}

jclass FakeJni::CreateNewClass(const ClassMetadata& cls_metadata) {
  FakeObject* cls_object = new FakeObject;
  cls_object->is_valid = true;
  cls_object->reference_count = 0;
  cls_object->cls = nullptr;

  cls_[cls_object] = cls_metadata;

  obj_.insert(cls_object);

  return static_cast<jclass>(
      CreateNewRef(JVariant::ReferenceKind::Local, cls_object));
}


jobject FakeJni::CreateNewObject(jclass cls) {
  FakeObject* object = new FakeObject;
  object->is_valid = true;
  object->reference_count = 0;
  object->cls = Dereference(cls);
  CHECK(object->cls != nullptr);

  obj_.insert(object);

  return CreateNewRef(JVariant::ReferenceKind::Local, object);
}

jstring FakeJni::CreateNewJavaString(const std::string& content) {
  // Convert string to Unicode.
  return CreateNewJavaString(
      std::vector<jchar>(content.begin(), content.end()));
}

jstring FakeJni::CreateNewJavaString(std::vector<jchar> content) {
  jstring jstr = static_cast<jstring>(CreateNewObject(StockClass::String));

  FakeObject* obj = Dereference(jstr);
  CHECK(obj != nullptr);
  jstring_data_[obj] = std::move(content);

  return jstr;
}


jobject FakeJni::CreateNewObject(StockClass stock_class) {
  return CreateNewObject(GetStockClass(stock_class));
}


jobject FakeJni::CreateNewRef(
    JVariant::ReferenceKind reference_kind,
    FakeObject* obj) {
  CHECK(obj != nullptr);
  CHECK(obj->is_valid);
  obj->reference_count++;

  FakeRef* ref = new FakeRef;
  ref->obj = obj;
  ref->reference_kind = reference_kind;

  refs_.insert(ref);

  return reinterpret_cast<jobject>(ref);
}


FakeJni::FakeObject* FakeJni::Dereference(jobject ref) {
  FakeRef* fake_ref = reinterpret_cast<FakeRef*>(ref);
  CHECK(refs_.find(fake_ref) != refs_.end()) << "Invalid reference";

  CHECK(fake_ref->obj != nullptr);
  CHECK(fake_ref->obj->is_valid);

  if (fake_ref->obj->reclaimed) {
    CHECK(fake_ref->reference_kind == JVariant::ReferenceKind::WeakGlobal);
    return nullptr;
  }

  return fake_ref->obj;
}


FakeJni::ClassMetadata& FakeJni::DereferenceClass(jclass ref) {
  FakeObject* obj = Dereference(ref);
  CHECK(obj != nullptr);

  auto it_metadata = cls_.find(obj);
  CHECK(it_metadata != cls_.end());

  return it_metadata->second;
}


void FakeJni::DeleteRef(JVariant::ReferenceKind reference_kind, jobject ref) {
  if (ref == nullptr) {
    return;
  }

  FakeRef* fake_ref = reinterpret_cast<FakeRef*>(ref);
  auto it_ref = refs_.find(fake_ref);
  CHECK(it_ref != refs_.end()) << "Invalid reference";

  CHECK_EQ(false, fake_ref->is_internal);

  CHECK(fake_ref->obj != nullptr);
  CHECK(fake_ref->obj->is_valid);
  CHECK_GT(fake_ref->obj->reference_count, 0);
  CHECK(fake_ref->reference_kind == reference_kind);

  fake_ref->obj->reference_count--;

  if (fake_ref->obj->reference_count == 0) {
    fake_ref->obj->is_valid = true;
  }

  delete fake_ref;

  refs_.erase(it_ref);
}


void FakeJni::InvalidateObject(jobject ref) {
  FakeObject* obj = Dereference(ref);
  CHECK(obj != nullptr);
  obj->reclaimed = true;
}


}  // namespace cdbg
}  // namespace devtools
