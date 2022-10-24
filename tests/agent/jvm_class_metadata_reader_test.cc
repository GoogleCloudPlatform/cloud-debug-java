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

#include "src/agent/jvm_class_metadata_reader.h"

#include <cstdint>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_object.h"
#include "src/agent/glob_data_visibility_policy.h"
#include "src/agent/instance_field_reader.h"
#include "src/agent/static_field_reader.h"
#include "src/agent/structured_data_visibility_policy.h"
#include "tests/agent/mock_jni_env.h"
#include "tests/agent/mock_jvmti_env.h"

using testing::_;
using testing::AtMost;
using testing::DoAll;
using testing::Eq;
using testing::IsNull;
using testing::Invoke;
using testing::NiceMock;
using testing::NotNull;
using testing::Test;
using testing::Return;
using testing::SetArgPointee;

namespace devtools {
namespace cdbg {

class ProhibitiveFieldsVisibility : public DataVisibilityPolicy {
 public:
  std::unique_ptr<Class> GetClassVisibility(jclass cls) override {
    return std::unique_ptr<Class>(new ClassImpl);
  }

  bool HasSetupError(std::string* error) const override { return false; }

 private:
  class ClassImpl : public Class {
    bool IsFieldVisible(const std::string& name,
                        int32_t field_modifiers) override {
      return false;
    }

    bool IsFieldDataVisible(const std::string& name, int32_t field_modifiers,
                            std::string* reason) override {
      return false;
    }

    bool IsMethodVisible(const std::string& method_name,
                         const std::string& method_signature,
                         int32_t method_modifiers) override {
      return true;
    }

    bool IsVariableVisible(const std::string& method_name,
                           const std::string& method_signature,
                           const std::string& variable_name) override {
      return true;
    }

    bool IsVariableDataVisible(const std::string& method_name,
                               const std::string& method_signature,
                               const std::string& variable_name,
                               std::string* reason) override {
      return true;
    }
  };
};


class ProhibitiveMethodsVisibility : public DataVisibilityPolicy {
 public:
  std::unique_ptr<Class> GetClassVisibility(jclass cls) override {
    return std::unique_ptr<Class>(new ClassImpl);
  }

  bool HasSetupError(std::string* error) const override { return false; }

 private:
  class ClassImpl : public Class {
    bool IsFieldVisible(const std::string& name,
                        int32_t field_modifiers) override {
      return true;
    }

    bool IsFieldDataVisible(const std::string& name, int32_t field_modifiers,
                            std::string* reason) override {
      return true;
    }

    bool IsMethodVisible(const std::string& method_name,
                         const std::string& method_signature,
                         int32_t method_modifiers) override {
      return false;
    }

    bool IsVariableVisible(const std::string& method_name,
                           const std::string& method_signature,
                           const std::string& variable_name) override {
      return true;
    }

    bool IsVariableDataVisible(const std::string& method_name,
                               const std::string& method_signature,
                               const std::string& variable_name,
                               std::string* reason) override {
      return true;
    }
  };
};


//
// Simulate this hierarchy:
//
// class SuperClass (implicitly extends Object)
// interface SuperInterface (implicitly extends Object)
// interface CustomInterface extends SuperInterface (implicitly extends Object)
// class CustomClass implements CustomInterface extends SuperClass
//

static const jclass kCustomClass = reinterpret_cast<jclass>(0x1234456556L);
static const jclass kCustomInterface = reinterpret_cast<jclass>(0x656234L);
static const jclass kSuperInterface = reinterpret_cast<jclass>(0x4727834L);
static const jclass kSuperClass = reinterpret_cast<jclass>(0x34765432L);
static const jclass kObjectClass = reinterpret_cast<jclass>(0x9078243L);

static constexpr char kCustomClassSignature[] = "Lcom/prod/MyFavoriteClass;";
static constexpr char kCustomInterfaceSignature[] = "LCustomInterface;";
static constexpr char kSuperInterfaceSignature[] = "LSuperInterface;";
static constexpr char kSuperClassSignature[] = "Lorg/ngo/TheirSuperClass;";
static constexpr char kObjectClassSignature[] = "Ljava/lang/Object;";

static const struct ClassFields {
  jclass cls;
  std::vector<jfieldID> fields;
} classes_fields[] = {
  {
    kCustomClass,
    {
      reinterpret_cast<jfieldID>(100),
      reinterpret_cast<jfieldID>(101),
      reinterpret_cast<jfieldID>(201),
      reinterpret_cast<jfieldID>(102),
      reinterpret_cast<jfieldID>(103),
      reinterpret_cast<jfieldID>(104),
      reinterpret_cast<jfieldID>(105),
      reinterpret_cast<jfieldID>(106),
      reinterpret_cast<jfieldID>(206),
      reinterpret_cast<jfieldID>(107),
      reinterpret_cast<jfieldID>(108),
      reinterpret_cast<jfieldID>(109),
      reinterpret_cast<jfieldID>(110)
    },
  },
  {
    kCustomInterface,
    {}
  },
  {
    kSuperInterface,
    {}
  },
  {
    kSuperClass,
    {
      reinterpret_cast<jfieldID>(111),
      reinterpret_cast<jfieldID>(212)
    }
  },
  {
    kObjectClass,
    {}
  }
};

static const struct ClassMethods {
  jclass cls;
  std::vector<jmethodID> methods;
} classes_methods[] = {
  {
    kCustomClass,
    {
      reinterpret_cast<jmethodID>(0x41),
      reinterpret_cast<jmethodID>(0x42),
      reinterpret_cast<jmethodID>(0x43),
      reinterpret_cast<jmethodID>(0x44),
      reinterpret_cast<jmethodID>(0x45),
      reinterpret_cast<jmethodID>(0x46)
    },
  },
  {
    kCustomInterface,
    {
      reinterpret_cast<jmethodID>(0x60)
    }
  },
  {
    kSuperInterface,
    {
      reinterpret_cast<jmethodID>(0x70)
    }
  },
  {
    kSuperClass,
    {
      reinterpret_cast<jmethodID>(0x47),
      reinterpret_cast<jmethodID>(0x48),
      reinterpret_cast<jmethodID>(0x49),
      reinterpret_cast<jmethodID>(0x4A)
    }
  },
  {
    kObjectClass,
    {
        reinterpret_cast<jmethodID>(0x50),
    }
  }
};

static const struct SuperclassInfo {
  jclass cls;
  jclass super;
  std::vector<jclass> interfaces;
} superclass_infos[] = {
  {
    kCustomClass,           // cls
    kSuperClass,            // super
    {                       // interfaces
      kCustomInterface
    }
  },
  {
    kCustomInterface,       // cls
    nullptr,                // super
    {                       // interfaces
      kSuperInterface
    }
  },
  {
    kSuperInterface,        // cls
    nullptr                 // super
  },
  {
    kSuperClass,            // cls
    kObjectClass            // super
  },
  {
    kObjectClass,           // cls
    nullptr                 // super
  }
};

static const struct FieldInfo {
  jclass cls;
  jfieldID field_id;
  const char* name;
  const char* signature;
  jint modifiers;
} field_infos[] = {
  {
    kCustomClass,                     // cls
    reinterpret_cast<jfieldID>(100),  // field_id
    "myint",                          // name
    "I",                              // signature
    0                                 // modifiers
  },
  {
    kCustomClass,                     // cls
    reinterpret_cast<jfieldID>(101),  // field_id
    "mybool",                         // name
    "Z",                              // signature
    0                                 // modifiers
  },
  {
    kCustomClass,                     // cls
    reinterpret_cast<jfieldID>(201),  // field_id
    "myStaticBool",                   // name
    "Z",                              // signature
    JVM_ACC_STATIC                    // modifiers
  },
  {
    kCustomClass,                     // cls
    reinterpret_cast<jfieldID>(102),  // field_id
    "mybyte",                         // name
    "B",                              // signature
    0                                 // modifiers
  },
  {
    kCustomClass,                       // cls
    reinterpret_cast<jfieldID>(103),    // field_id
    "mychar",                           // name
    "C",                                // signature
    0                                   // modifiers
  },
  {
    kCustomClass,                       // cls
    reinterpret_cast<jfieldID>(104),    // field_id
    "myshort",                          // name
    "S",                                // signature
    0                                   // modifiers
  },
  {
    kCustomClass,                       // cls
    reinterpret_cast<jfieldID>(105),    // field_id
    "mylong",                           // name
    "J",                                // signature
    0                                   // modifiers
  },
  {
    kCustomClass,                       // cls
    reinterpret_cast<jfieldID>(106),    // field_id
    "myfloat",                          // name
    "F",                                // signature
    0                                   // modifiers
  },
  {
    kCustomClass,                       // cls
    reinterpret_cast<jfieldID>(206),    // field_id
    "myStaticFloat",                    // name
    "F",                                // signature
    JVM_ACC_STATIC                      // modifiers
  },
  {
    kCustomClass,                       // cls
    reinterpret_cast<jfieldID>(107),    // field_id
    "mydouble",                         // name
    "D",                                // signature
    0                                   // modifiers
  },
  {
    kCustomClass,                       // cls
    reinterpret_cast<jfieldID>(108),    // field_id
    "mystring",                         // name
    "Ljava/lang/String",                // signature
    0                                   // modifiers
  },
  {
    kCustomClass,                       // cls
    reinterpret_cast<jfieldID>(109),    // field_id
    "myintarray",                       // name
    "[I",                               // signature
    0                                   // modifiers
  },
  {
    kCustomClass,                       // cls
    reinterpret_cast<jfieldID>(110),    // field_id
    "myStaticDouble",                   // name
    "D",                                // signature
    JVM_ACC_STATIC                      // modifiers
  },
  {
    kSuperClass,                        // cls
    reinterpret_cast<jfieldID>(111),    // field_id
    "superdouble",                      // name
    "D",                                // signature
    0                                   // modifiers
  },
  {
    kSuperClass,                        // cls
    reinterpret_cast<jfieldID>(212),    // field_id
    "mySuperStaticInt",                 // name
    "I",                                // signature
    JVM_ACC_STATIC                      // modifiers
  }
};


static const struct MethodInfo {
  jclass cls;
  jmethodID method_id;
  const char* name;
  const char* signature;
  jint modifiers;
} method_infos[] = {
  {
    kCustomClass,                       // cls
    reinterpret_cast<jmethodID>(0x41),  // method_id
    "firstMethod",                      // name
    "(Z)[I",                            // signature
    0                                   // modifiers
  },
  {
    kCustomClass,                       // cls
    reinterpret_cast<jmethodID>(0x42),  // method_id
    "firstMethod",                      // name
    "(I)[I",                            // signature
    0                                   // modifiers
  },
  {
    kCustomClass,                       // cls
    reinterpret_cast<jmethodID>(0x43),  // method_id
    "secondMethod",                     // name
    "()V",                              // signature
    0                                   // modifiers
  },
  {
    kCustomClass,                       // cls
    reinterpret_cast<jmethodID>(0x44),  // method_id
    "staticMethod",                     // name
    "()Ljava/lang/String;",             // signature
    JVM_ACC_STATIC                      // modifiers
  },
  {
    kCustomClass,                       // cls
    reinterpret_cast<jmethodID>(0x45),  // method_id
    "instanceOverload",                 // name
    "(ZIZI)LSomeBaseClass;",            // signature
    0                                   // modifiers
  },
  {
    kCustomClass,                       // cls
    reinterpret_cast<jmethodID>(0x46),  // method_id
    "staticOverload",                   // name
    "(ZIZI)LSomeBaseClass;",            // signature
    JVM_ACC_STATIC                      // modifiers
  },
  {
    kCustomInterface,                   // cls
    reinterpret_cast<jmethodID>(0x60),  // method_id
    "customInterfaceMethod",            // name
    "()V;",                             // signature
    0,                                  // modifiers
  },
  {
    kSuperInterface,                    // cls
    reinterpret_cast<jmethodID>(0x70),  // method_id
    "superInterfaceMethod",             // name
    "()V;",                             // signature
    0,                                  // modifiers
  },
  {
    kSuperClass,                        // cls
    reinterpret_cast<jmethodID>(0x47),  // method_id
    "superInstanceMethod",              // name
    "(Ljava/lang/String;)Z",            // signature
    0                                   // modifiers
  },
  {
    kSuperClass,                        // cls
    reinterpret_cast<jmethodID>(0x48),  // method_id
    "superStaticMethod",                // name
    "(Ljava/lang/String;)Z",            // signature
    JVM_ACC_STATIC                      // modifiers
  },
  {
    kSuperClass,                        // cls
    reinterpret_cast<jmethodID>(0x49),  // method_id
    "instanceOverload",                 // name
    "(ZIZI)LSomeSuperClass;",           // signature
    0                                   // modifiers
  },
  {
    kSuperClass,                        // cls
    reinterpret_cast<jmethodID>(0x4A),  // method_id
    "staticOverload",                   // name
    "(ZIZI)LSomeSuperClass;",           // signature
    JVM_ACC_STATIC                      // modifiers
  },
  {
    kObjectClass,                       // cls
    reinterpret_cast<jmethodID>(0x50),  // method_id
    "toString",                         // name
    "()Ljava/lang/String;",             // signature
    0,                                  // modifiers
  }
};


// Gets pointer to the buffer of std::vector removing const qualifier.
template <typename T>
T* GetVectorData(const std::vector<T>& v) {
  if (v.empty()) {
    return nullptr;
  }

  return const_cast<T*>(v.data());
}


class JvmClassMetadataReaderTest : public ::testing::Test {
 protected:
  JvmClassMetadataReaderTest() : global_jvm_(&jvmti_, &jni_) {
  }

  void SetUp() override {
    jniproxy::InjectObject(&object_);

    EXPECT_CALL(object_, GetClass())
        .WillRepeatedly(Return(kObjectClass));

    EXPECT_CALL(jni_, GetObjectRefType(_))
        .WillRepeatedly(Return(JNILocalRefType));

    EXPECT_CALL(jni_, NewLocalRef(_))
        .WillRepeatedly(Invoke([] (jobject obj) {
          return obj;
        }));

    EXPECT_CALL(jni_, NewGlobalRef(_))
        .WillRepeatedly(Invoke([] (jobject obj) { return obj; }));

    EXPECT_CALL(jni_, NewWeakGlobalRef(_))
        .WillRepeatedly(Invoke([] (jobject obj) {
          return obj;
        }));

    EXPECT_CALL(jni_, DeleteWeakGlobalRef(_))
        .WillRepeatedly(Return());

    EXPECT_CALL(jni_, DeleteLocalRef(_))
        .WillRepeatedly(Return());

    EXPECT_CALL(jni_, DeleteGlobalRef(_))
        .WillRepeatedly(Return());

    EXPECT_CALL(jni_, IsSameObject(_, _))
        .WillRepeatedly(Invoke([] (jobject obj1, jobject obj2) {
          return obj1 == obj2;
        }));

    EXPECT_CALL(jvmti_, Deallocate(NotNull()))
        .WillRepeatedly(Return(JVMTI_ERROR_NONE));

    EXPECT_CALL(jvmti_, GetObjectHashCode(_, NotNull()))
        .WillRepeatedly(DoAll(SetArgPointee<1>(0), Return(JVMTI_ERROR_NONE)));

    EXPECT_CALL(jvmti_, GetClassSignature(kCustomClass, NotNull(), nullptr))
        .WillRepeatedly(DoAll(
            SetArgPointee<1>(const_cast<char*>(kCustomClassSignature)),
            Return(JVMTI_ERROR_NONE)));

    EXPECT_CALL(jvmti_, GetClassSignature(kCustomInterface, NotNull(), nullptr))
        .WillRepeatedly(DoAll(
            SetArgPointee<1>(const_cast<char*>(kCustomInterfaceSignature)),
            Return(JVMTI_ERROR_NONE)));

    EXPECT_CALL(jvmti_, GetClassSignature(kSuperInterface, NotNull(), nullptr))
        .WillRepeatedly(DoAll(
            SetArgPointee<1>(const_cast<char*>(kSuperInterfaceSignature)),
            Return(JVMTI_ERROR_NONE)));

    EXPECT_CALL(jvmti_, GetClassSignature(kSuperClass, NotNull(), nullptr))
        .WillRepeatedly(DoAll(
            SetArgPointee<1>(const_cast<char*>(kSuperClassSignature)),
            Return(JVMTI_ERROR_NONE)));

    EXPECT_CALL(jvmti_, GetClassSignature(kObjectClass, NotNull(), nullptr))
        .WillRepeatedly(DoAll(
            SetArgPointee<1>(const_cast<char*>(kObjectClassSignature)),
            Return(JVMTI_ERROR_NONE)));

    for (const auto& class_fields : classes_fields) {
      EXPECT_CALL(
          jvmti_,
          GetClassFields(class_fields.cls, NotNull(), NotNull()))
          .Times(AtMost(1))  // AtMost(1) verifies class metadata cache.
          .WillRepeatedly(DoAll(
              SetArgPointee<1>(class_fields.fields.size()),
              SetArgPointee<2>(GetVectorData(class_fields.fields)),
              Return(JVMTI_ERROR_NONE)));
    }

    for (const auto& class_methods : classes_methods) {
      EXPECT_CALL(
          jvmti_,
          GetClassMethods(class_methods.cls, NotNull(), NotNull()))
          .Times(AtMost(1))  // AtMost(1) verifies class metadata cache.
          .WillRepeatedly(DoAll(
              SetArgPointee<1>(class_methods.methods.size()),
              SetArgPointee<2>(GetVectorData(class_methods.methods)),
              Return(JVMTI_ERROR_NONE)));
    }

    for (const auto& field_info : field_infos) {
      EXPECT_CALL(
          jvmti_,
          GetFieldModifiers(field_info.cls, field_info.field_id, NotNull()))
          .WillRepeatedly(DoAll(
              SetArgPointee<2>(field_info.modifiers),
              Return(JVMTI_ERROR_NONE)));

      EXPECT_CALL(
          jvmti_,
          GetFieldName(
              field_info.cls,
              field_info.field_id,
              NotNull(),
              NotNull(),
              nullptr))
          .WillRepeatedly(DoAll(
              SetArgPointee<2>(const_cast<char*>(field_info.name)),
              SetArgPointee<3>(const_cast<char*>(field_info.signature)),
              Return(JVMTI_ERROR_NONE)));
    }

    for (const auto& method_info : method_infos) {
      EXPECT_CALL(
          jvmti_,
          GetMethodModifiers(method_info.method_id, NotNull()))
          .WillRepeatedly(DoAll(
              SetArgPointee<1>(method_info.modifiers),
              Return(JVMTI_ERROR_NONE)));

      EXPECT_CALL(
          jvmti_,
          GetMethodName(method_info.method_id, NotNull(), NotNull(), nullptr))
          .WillRepeatedly(DoAll(
              SetArgPointee<1>(const_cast<char*>(method_info.name)),
              SetArgPointee<2>(const_cast<char*>(method_info.signature)),
              Return(JVMTI_ERROR_NONE)));
    }

    for (const auto& superclass_info : superclass_infos) {
      EXPECT_CALL(jni_, GetSuperclass(superclass_info.cls))
          .WillRepeatedly(Return(superclass_info.super));

      EXPECT_CALL(jvmti_, GetImplementedInterfaces(superclass_info.cls, _, _))
          .WillRepeatedly(DoAll(
              SetArgPointee<1>(superclass_info.interfaces.size()),
              SetArgPointee<2>(GetVectorData(superclass_info.interfaces)),
              Return(JVMTI_ERROR_NONE)));
    }
  }

  void TearDown() override {
    jniproxy::InjectObject(nullptr);
  }

 protected:
  NiceMock<MockJvmtiEnv> jvmti_;
  MockJNIEnv jni_;
  GlobalJvmEnv global_jvm_;
  NiceMock<jniproxy::MockObject> object_;
};


TEST_F(JvmClassMetadataReaderTest, GetClassMetadata) {
  StructuredDataVisibilityPolicy data_visibility_policy;
  JvmClassMetadataReader metadata_reader(&data_visibility_policy);
  const auto& metadata = metadata_reader.GetClassMetadata(kCustomClass);

  EXPECT_EQ(JType::Object, metadata.signature.type);
  EXPECT_EQ(kCustomClassSignature, metadata.signature.object_signature);

  std::vector<std::string> instance_field_names;
  for (const auto& instance_field : metadata.instance_fields) {
    instance_field_names.push_back(instance_field->GetName());
    jobject source_object = nullptr;
    JVariant result;
    FormatMessageModel message;
    EXPECT_TRUE(instance_field->ReadValue(source_object, &result, &message));
  }

  std::vector<std::string> static_field_names;
  for (const auto& static_field : metadata.static_fields) {
    static_field_names.push_back(static_field->GetName());
    JVariant result;
    FormatMessageModel message;
    EXPECT_TRUE(static_field->ReadValue(&result, &message));
  }

  EXPECT_EQ(std::vector<std::string>(
                {"superdouble",  // Fields from base class go first.
                 "myint", "mybool", "mybyte", "mychar", "myshort", "mylong",
                 "myfloat", "mydouble", "mystring", "myintarray"}),
            instance_field_names);

  EXPECT_EQ(std::vector<std::string>(
                {"mySuperStaticInt",  // Fields from base class go first.
                 "myStaticBool", "myStaticFloat", "myStaticDouble"}),
            static_field_names);

  // Specifically verify overriding rules due to virtual functions.
  std::vector<std::string> expected_methods(
      {"9:Lcom/prod/MyFavoriteClass;:firstMethod:(Z)[I:0",
       "9:Lcom/prod/MyFavoriteClass;:firstMethod:(I)[I:0",
       "9:Lcom/prod/MyFavoriteClass;:secondMethod:()V:0",
       "9:Lcom/prod/MyFavoriteClass;:staticMethod:()Ljava/lang/String;:8",
       "9:Lcom/prod/MyFavoriteClass;:instanceOverload:(ZIZI)LSomeBaseClass;:0",
       "9:Lcom/prod/MyFavoriteClass;:staticOverload:(ZIZI)LSomeBaseClass;:8",
       "9:LCustomInterface;:customInterfaceMethod:()V;:0",
       "9:LSuperInterface;:superInterfaceMethod:()V;:0",
       "9:Lorg/ngo/TheirSuperClass;:superInstanceMethod:(Ljava/lang/"
       "String;)Z:0",
       "9:Lorg/ngo/TheirSuperClass;:superStaticMethod:(Ljava/lang/String;)Z:8",
       "9:Lorg/ngo/TheirSuperClass;:staticOverload:(ZIZI)LSomeSuperClass;:8",
       "9:Ljava/lang/Object;:toString:()Ljava/lang/String;:0"});

  std::vector<std::string> actual_methods;
  for (const auto& method_metadata : metadata.methods) {
    actual_methods.push_back(
        std::to_string(static_cast<int>(method_metadata.class_signature.type)) +
        ":" +
        method_metadata.class_signature.object_signature +
        ":" +
        method_metadata.name +
        ":" +
        method_metadata.signature +
        ":" +
        std::to_string(method_metadata.modifiers));
  }

  EXPECT_EQ(expected_methods, actual_methods);
  EXPECT_FALSE(metadata.instance_fields_omitted);
}


TEST_F(JvmClassMetadataReaderTest, ImplicitObjectSuperclass) {
  StructuredDataVisibilityPolicy data_visibility_policy;
  JvmClassMetadataReader metadata_reader(&data_visibility_policy);
  const auto& metadata = metadata_reader.GetClassMetadata(kCustomInterface);

  for (const auto& method : metadata.methods) {
    if ((method.class_signature.object_signature == kObjectClassSignature) &&
        (method.name == "toString")) {
      return;  // Test passed.
    }
  }

  ADD_FAILURE() << "Object.toString not found in class metadata";
}


TEST_F(JvmClassMetadataReaderTest, StaticFieldClassReference) {
  StructuredDataVisibilityPolicy data_visibility_policy;
  JvmClassMetadataReader metadata_reader(&data_visibility_policy);
  const auto& metadata = metadata_reader.GetClassMetadata(kCustomClass);

  EXPECT_CALL(
      jni_,
      GetStaticDoubleField(kCustomClass, reinterpret_cast<jfieldID>(110)))
      .WillOnce(Return(3.14));

  EXPECT_CALL(
      jni_,
      GetStaticIntField(kSuperClass, reinterpret_cast<jfieldID>(212)))
      .WillOnce(Return(721));

  const StaticFieldReader* my_static_double_reader = nullptr;
  const StaticFieldReader* my_super_static_int_reader = nullptr;

  for (const auto& static_field : metadata.static_fields) {
    if (static_field->GetName() == "myStaticDouble") {
      my_static_double_reader = static_field.get();
    }

    if (static_field->GetName() == "mySuperStaticInt") {
      my_super_static_int_reader = static_field.get();
    }
  }

  ASSERT_TRUE(my_static_double_reader != nullptr);
  ASSERT_TRUE(my_super_static_int_reader != nullptr);

  JVariant my_static_double;
  FormatMessageModel error;
  EXPECT_TRUE(my_static_double_reader->ReadValue(&my_static_double, &error));

  double my_static_double_value = 0;
  EXPECT_TRUE(my_static_double.get<jdouble>(&my_static_double_value));
  EXPECT_EQ(3.14, my_static_double_value);

  JVariant my_super_static_int;
  EXPECT_TRUE(
      my_super_static_int_reader->ReadValue(&my_super_static_int, &error));

  int my_super_static_int_value = 0;
  EXPECT_TRUE(my_super_static_int.get<jint>(&my_super_static_int_value));
  EXPECT_EQ(721, my_super_static_int_value);
}


TEST_F(JvmClassMetadataReaderTest, Cache) {
  // SetUp configures "GetClassFields" to expect only a single call for each
  // class. If cache doesn't work, "GetClassFields" will be called twice and
  // this test will fail.

  StructuredDataVisibilityPolicy data_visibility_policy;
  JvmClassMetadataReader metadata_reader(&data_visibility_policy);
  metadata_reader.GetClassMetadata(kCustomClass);
  metadata_reader.GetClassMetadata(kCustomClass);
}


TEST_F(JvmClassMetadataReaderTest, StructuredFieldVisibilityPolicy) {
  StructuredDataVisibilityPolicy::Config config;
  auto& fields = config.packages["com/prod"].classes["MyFavoriteClass"].fields;
  fields.resize(2);
  fields[0].name = "myint";
  fields[0].invisible = true;
  fields[1].name = "myStaticBool";
  fields[1].invisible = true;

  StructuredDataVisibilityPolicy data_visibility_policy;
  data_visibility_policy.SetConfig(config);
  JvmClassMetadataReader metadata_reader(&data_visibility_policy);
  const auto& metadata = metadata_reader.GetClassMetadata(kCustomClass);

  std::vector<std::string> instance_field_names;
  for (const auto& instance_field : metadata.instance_fields) {
    instance_field_names.push_back(instance_field->GetName());
  }

  std::vector<std::string> static_field_names;
  for (const auto& static_field : metadata.static_fields) {
    static_field_names.push_back(static_field->GetName());
  }

  // Verify that only "myint" is filtered out.
  EXPECT_EQ(std::vector<std::string>(
                {"superdouble",  // Fields from base class go first.
                 "mybool", "mybyte", "mychar", "myshort", "mylong", "myfloat",
                 "mydouble", "mystring", "myintarray"}),
            instance_field_names);

  // Verify that only "myStaticBool" is filtered out.
  EXPECT_EQ(std::vector<std::string>(
                {"mySuperStaticInt",  // Fields from base class go first.
                 "myStaticFloat", "myStaticDouble"}),
            static_field_names);

  EXPECT_TRUE(metadata.instance_fields_omitted);
}


TEST_F(JvmClassMetadataReaderTest, GlobFieldVisibilityPolicy) {
  GlobDataVisibilityPolicy::Config config;
  config.blocklists.Add("*");
  config.blocklists.Prepare();
  GlobDataVisibilityPolicy data_visibility_policy;
  data_visibility_policy.SetConfig(config);

  JvmClassMetadataReader metadata_reader(&data_visibility_policy);
  const auto& metadata = metadata_reader.GetClassMetadata(kCustomClass);

  EXPECT_EQ(11, metadata.instance_fields.size());

  for (const auto& instance_field : metadata.instance_fields) {
    jobject source_object = nullptr;
    JVariant result;
    FormatMessageModel message;
    // Object should NOT be readable
    EXPECT_FALSE(instance_field->ReadValue(source_object, &result, &message));
    EXPECT_NE("", message.format);
  }

  EXPECT_EQ(4, metadata.static_fields.size());

  for (const auto& static_field : metadata.static_fields) {
    JVariant result;
    FormatMessageModel message;
    // Object should NOT be readable
    EXPECT_FALSE(static_field->ReadValue(&result, &message));
    EXPECT_NE("", message.format);
  }
}


TEST_F(JvmClassMetadataReaderTest, ProhibitiveFieldVisibilityPolicy) {
  ProhibitiveFieldsVisibility data_visibility_policy;
  JvmClassMetadataReader metadata_reader(&data_visibility_policy);
  const auto& metadata = metadata_reader.GetClassMetadata(kCustomClass);

  EXPECT_EQ(0, metadata.instance_fields.size());
  EXPECT_EQ(0, metadata.static_fields.size());
  EXPECT_TRUE(metadata.instance_fields_omitted);
}


TEST_F(JvmClassMetadataReaderTest, ProhibitiveMethodVisibilityPolicy) {
  ProhibitiveMethodsVisibility data_visibility_policy;
  JvmClassMetadataReader metadata_reader(&data_visibility_policy);
  const auto& metadata = metadata_reader.GetClassMetadata(kCustomClass);

  EXPECT_EQ(0, metadata.methods.size());
  EXPECT_FALSE(metadata.instance_fields_omitted);
}

}  // namespace cdbg
}  // namespace devtools
