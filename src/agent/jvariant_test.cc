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

#include "jvariant.h"

#include "gmock/gmock.h"
#include "src/agent/test_util/mock_jni_env.h"
#include "src/agent/test_util/mock_jvmti_env.h"

using testing::_;
using testing::AtMost;
using testing::Invoke;
using testing::Return;

namespace devtools {
namespace cdbg {

class JVariantTest : public ::testing::Test {
 protected:
  JVariantTest() : global_jvm_(&jvmti_, &jni_) {
  }

  void SetUp() override {
    EXPECT_CALL(jni_, NewLocalRef(_))
        .WillRepeatedly(Invoke([] (jobject obj) {
          return obj;
        }));
  }

  template <typename T>
  static void VerifyJVariant(const JVariant& jvariant, T expected_value) {
    T actual_value;
    ASSERT_TRUE(jvariant.get<T>(&actual_value));
    EXPECT_EQ(expected_value, actual_value);
  }

 protected:
  MockJNIEnv jni_;
  MockJvmtiEnv jvmti_;
  GlobalJvmEnv global_jvm_;
};


TEST_F(JVariantTest, DefaultConstructor) {
  JVariant v;
  EXPECT_EQ(JType::Void, v.type());
}


TEST_F(JVariantTest, Constructor_jboolean) {
  VerifyJVariant<jboolean>(JVariant::Boolean(true), true);
  VerifyJVariant<jboolean>(JVariant::Boolean(false), false);
}


TEST_F(JVariantTest, Constructor_jbyte) {
  VerifyJVariant<jbyte>(JVariant::Byte(-119), -119);
}


TEST_F(JVariantTest, Constructor_jchar) {
  VerifyJVariant<jchar>(JVariant::Char(52345), 52345);
}


TEST_F(JVariantTest, Constructor_jshort) {
  VerifyJVariant<jshort>(JVariant::Short(-12345), -12345);
}


TEST_F(JVariantTest, Constructor_jint) {
  VerifyJVariant<jint>(JVariant::Int(12345678), 12345678);
}


TEST_F(JVariantTest, Constructor_jlong) {
  VerifyJVariant<jlong>(JVariant::Long(0x1234123412), 0x1234123412);
}


TEST_F(JVariantTest, Constructor_jfloat) {
  VerifyJVariant<jfloat>(JVariant::Float(1.1f), 1.1f);
}


TEST_F(JVariantTest, Constructor_jdouble) {
  VerifyJVariant<jdouble>(JVariant::Double(3.1415), 3.1415);
}


TEST_F(JVariantTest, ConstructorJObjectLocalReference) {
  const jobject kLocalRef = reinterpret_cast<jobject>(3478534785);
  JVariant jvariant = JVariant::LocalRef(kLocalRef);
  VerifyJVariant<jobject>(jvariant, kLocalRef);

  EXPECT_CALL(jni_, DeleteLocalRef(kLocalRef))
      .WillOnce(Return());
}


TEST_F(JVariantTest, ConstructorJObjectGlobalReference) {
  const jobject kGlobalRef = reinterpret_cast<jobject>(3478534785);
  JVariant jvariant = JVariant::GlobalRef(kGlobalRef);
  VerifyJVariant<jobject>(jvariant, kGlobalRef);

  EXPECT_CALL(jni_, DeleteGlobalRef(kGlobalRef))
      .WillOnce(Return());
}


TEST_F(JVariantTest, swap) {
  JVariant v1 = JVariant::Int(34857643);
  JVariant v2 = JVariant::Boolean(true);

  v1.swap(&v2);

  VerifyJVariant<jboolean>(v1, true);
  VerifyJVariant<jint>(v2, 34857643);
}


TEST_F(JVariantTest, type) {
  EXPECT_EQ(JType::Boolean, JVariant::Boolean(true).type());
  EXPECT_EQ(JType::Byte, JVariant::Byte(-119).type());
  EXPECT_EQ(JType::Char, JVariant::Char(52345).type());
  EXPECT_EQ(JType::Short, JVariant::Short(-12345).type());
  EXPECT_EQ(JType::Int, JVariant::Int(12345678).type());
  EXPECT_EQ(JType::Long, JVariant::Long(0x1234123412).type());
  EXPECT_EQ(JType::Float, JVariant::Float(1.1f).type());
  EXPECT_EQ(JType::Double, JVariant::Double(3.1415).type());
  EXPECT_EQ(JType::Object, JVariant::Null().type());
}


TEST_F(JVariantTest, get_WrongType) {
  JVariant stock_jvariants[] = {
    JVariant::Boolean(true),
    JVariant::Byte(-119),
    JVariant::Char(52345),
    JVariant::Short(-12345),
    JVariant::Int(12345678),
    JVariant::Long(0x1234123412),
    JVariant::Float(1.1f),
    JVariant::Double(3.1415),
    JVariant::Null()
  };

  for (const JVariant& jvariant : stock_jvariants) {
    if (jvariant.type() != JType::Boolean) {
      jboolean value;
      EXPECT_FALSE(jvariant.get<jboolean>(&value));
    }

    if (jvariant.type() != JType::Byte) {
      jbyte value;
      EXPECT_FALSE(jvariant.get<jbyte>(&value));
    }

    if (jvariant.type() != JType::Char) {
      jchar value;
      EXPECT_FALSE(jvariant.get<jchar>(&value));
    }

    if (jvariant.type() != JType::Short) {
      jshort value;
      EXPECT_FALSE(jvariant.get<jshort>(&value));
    }

    if (jvariant.type() != JType::Int) {
      jint value;
      EXPECT_FALSE(jvariant.get<jint>(&value));
    }

    if (jvariant.type() != JType::Long) {
      jlong value;
      EXPECT_FALSE(jvariant.get<jlong>(&value));
    }

    if (jvariant.type() != JType::Float) {
      jfloat value;
      EXPECT_FALSE(jvariant.get<jfloat>(&value));
    }

    if (jvariant.type() != JType::Double) {
      jdouble value;
      EXPECT_FALSE(jvariant.get<jdouble>(&value));
    }

    if (jvariant.type() != JType::Object) {
      jobject value;
      EXPECT_FALSE(jvariant.get<jobject>(&value));
    }
  }
}


TEST_F(JVariantTest, has_non_null_object) {
  const jobject kLocalRef = reinterpret_cast<jobject>(11111);

  EXPECT_FALSE(JVariant::Boolean(true).has_non_null_object());
  EXPECT_FALSE(JVariant::Float(1.23f).has_non_null_object());

  EXPECT_FALSE(JVariant::Null().has_non_null_object());

  EXPECT_CALL(jni_, DeleteLocalRef(kLocalRef)).Times(1);
  EXPECT_TRUE(JVariant::LocalRef(kLocalRef).has_non_null_object());
}


TEST_F(JVariantTest, assign_jboolean) {
  JVariant v;
  v = JVariant::Boolean(true);
  VerifyJVariant<jboolean>(v, true);
}


TEST_F(JVariantTest, assign_jbyte) {
  JVariant v;
  v = JVariant::Byte(-119);
  VerifyJVariant<jbyte>(v, -119);
}


TEST_F(JVariantTest, assign_jchar) {
  JVariant v;
  v = JVariant::Char(52345);
  VerifyJVariant<jchar>(v, 52345);
}


TEST_F(JVariantTest, assign_jshort) {
  JVariant v;
  v = JVariant::Short(-12345);
  VerifyJVariant<jshort>(v, -12345);
}


TEST_F(JVariantTest, assign_jint) {
  JVariant v;
  v = JVariant::Int(12345678);
  VerifyJVariant<jint>(v, 12345678);
}


TEST_F(JVariantTest, assign_jlong) {
  JVariant v;
  v = JVariant::Long(0x1234123412);
  VerifyJVariant<jlong>(v, 0x1234123412);
}


TEST_F(JVariantTest, assign_jfloat) {
  JVariant v;
  v = JVariant::Float(1.1f);
  VerifyJVariant<jfloat>(v, 1.1f);
}


TEST_F(JVariantTest, assign_jdouble) {
  JVariant v;
  v = JVariant::Double(3.1415);
  VerifyJVariant<jdouble>(v, 3.1415);
}


TEST_F(JVariantTest, assign_JVariant_jobject) {
  const jobject kLocalRef1 = reinterpret_cast<jobject>(11111);
  const jobject kLocalRef2a = reinterpret_cast<jobject>(22222);
  const jobject kLocalRef2b = reinterpret_cast<jobject>(22222);

  JVariant jvariant1 = JVariant::LocalRef(kLocalRef1);
  JVariant jvariant2 = JVariant::LocalRef(kLocalRef2a);

  EXPECT_CALL(jni_, DeleteLocalRef(kLocalRef1)).Times(1);
  EXPECT_CALL(jni_, NewLocalRef(kLocalRef2a)).WillOnce(Return(kLocalRef2b));

  jvariant1 = JVariant(jvariant2);

  VerifyJVariant<jobject>(jvariant1, kLocalRef2b);

  EXPECT_CALL(jni_, DeleteLocalRef(kLocalRef2b)).Times(1);
  jvariant1.ReleaseRef();

  EXPECT_CALL(jni_, DeleteLocalRef(kLocalRef2a)).Times(1);
  jvariant2.ReleaseRef();
}


TEST_F(JVariantTest, assign_JVariant_boolean) {
  const jobject kLocalRef = reinterpret_cast<jobject>(11111);

  JVariant jvariant1 = JVariant::LocalRef(kLocalRef);
  JVariant jvariant2 = JVariant::Boolean(true);

  EXPECT_CALL(jni_, DeleteLocalRef(kLocalRef)).Times(1);
  jvariant1 = JVariant(jvariant2);

  VerifyJVariant<jboolean>(jvariant1, true);
}


TEST_F(JVariantTest, change_ref_type_PrimitiveType) {
  JVariant jvariant = JVariant::Boolean(true);
  jvariant.change_ref_type(JVariant::ReferenceKind::Global);

  VerifyJVariant<jboolean>(jvariant, true);
}


TEST_F(JVariantTest, AttachGlobalRef) {
  const jobject kLocalRef = reinterpret_cast<jobject>(0x1111);
  const jobject kGlobalRef = reinterpret_cast<jobject>(0x2222);

  jobject ref = nullptr;

  JVariant jvariant = JVariant::LocalRef(kLocalRef);

  EXPECT_CALL(jni_, GetObjectRefType(kGlobalRef))
      .Times(AtMost(1))
      .WillRepeatedly(Return(JNIGlobalRefType));

  EXPECT_CALL(jni_, DeleteLocalRef(kLocalRef))
      .WillOnce(Return());

  jvariant.attach_ref(JVariant::ReferenceKind::Global, kGlobalRef);

  EXPECT_TRUE(jvariant.get<jobject>(&ref));
  EXPECT_EQ(kGlobalRef, ref);

  EXPECT_CALL(jni_, DeleteGlobalRef(kGlobalRef))
      .WillOnce(Return());

  jvariant.ReleaseRef();
}


TEST_F(JVariantTest, change_ref_type_LocalToGlobal) {
  const jobject kLocalRef = reinterpret_cast<jobject>(0x1111);
  const jobject kGlobalRef = reinterpret_cast<jobject>(0x2222);

  jobject ref = nullptr;

  JVariant jvariant = JVariant::LocalRef(kLocalRef);

  EXPECT_TRUE(jvariant.get<jobject>(&ref));
  EXPECT_EQ(kLocalRef, ref);

  EXPECT_CALL(jni_, NewGlobalRef(kLocalRef))
      .WillOnce(Return(kGlobalRef));
  EXPECT_CALL(jni_, DeleteLocalRef(kLocalRef))
      .WillOnce(Return());

  jvariant.change_ref_type(JVariant::ReferenceKind::Global);

  EXPECT_TRUE(jvariant.get<jobject>(&ref));
  EXPECT_EQ(kGlobalRef, ref);

  EXPECT_CALL(jni_, DeleteGlobalRef(kGlobalRef))
      .WillOnce(Return());
}


TEST_F(JVariantTest, jvalue_jboolean) {
  JVariant v;

  v = JVariant::Boolean(true);
  EXPECT_EQ(JNI_TRUE, v.get_jvalue().z);

  v = JVariant::Boolean(false);
  EXPECT_EQ(JNI_FALSE, v.get_jvalue().z);
}


TEST_F(JVariantTest, jvalue_jobject) {
  const jobject kLocalRef = reinterpret_cast<jobject>(0x1111);
  JVariant jvariant = JVariant::LocalRef(kLocalRef);

  EXPECT_EQ(kLocalRef, jvariant.get_jvalue().l);

  EXPECT_CALL(jni_, DeleteLocalRef(kLocalRef))
      .WillOnce(Return());

  jvariant.ReleaseRef();
}


TEST_F(JVariantTest, ToString) {
  const struct {
    JVariant jvariant;
    std::string concise_str;
    std::string long_str;
  } test_cases[] = {
    {
      JVariant::Boolean(true),
      "true",
      "<boolean>true",
    },
    {
      JVariant::Byte(-119),
      "-119",
      "<byte>-119"
    },
    {
      JVariant::Char(52345),
      "52345",
      "<char>52345"
    },
    {
      JVariant::Short(-12345),
      "-12345",
      "<short>-12345"
    },
    {
      JVariant::Int(12345678),
      "12345678",
      "<int>12345678"
    },
    {
      JVariant::Long(0x1234123412),
      "78183019538",
      "<long>78183019538"
    },
    {
      JVariant::Float(1.1f),
      "1.1",
      "<float>1.1"
    },
    {
      JVariant::Double(3.1415),
      "3.1415",
      "<double>3.1415"
    },
    {
      JVariant::Null(),
      "null",
      "null"
    }
  };

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(test_case.concise_str, test_case.jvariant.ToString(true));
    EXPECT_EQ(test_case.long_str, test_case.jvariant.ToString(false));
  }
}


TEST_F(JVariantTest, MoveLocalRef) {
  const jobject kLocalRef = reinterpret_cast<jobject>(0x1111);
  JVariant jvariant1 = JVariant::LocalRef(kLocalRef);

  jobject obj = nullptr;

  JVariant jvariant2(std::move(jvariant1));
  EXPECT_EQ(JType::Void, jvariant1.type());
  EXPECT_TRUE(jvariant2.get<jobject>(&obj) && (obj == kLocalRef));

  EXPECT_CALL(jni_, DeleteLocalRef(kLocalRef))
      .WillOnce(Return());
}


TEST_F(JVariantTest, MoveGlobalRef) {
  const jobject kGlobalRef = reinterpret_cast<jobject>(0x2222);
  JVariant jvariant1 = JVariant::GlobalRef(kGlobalRef);

  jobject obj = nullptr;

  JVariant jvariant2(std::move(jvariant1));
  EXPECT_EQ(JType::Void, jvariant1.type());
  EXPECT_TRUE(jvariant2.get<jobject>(&obj) && (obj == kGlobalRef));

  EXPECT_CALL(jni_, DeleteGlobalRef(kGlobalRef))
      .WillOnce(Return());
}


TEST_F(JVariantTest, JVariantMoveInt) {
  JVariant jvariant1 = JVariant::Int(732);

  jint int_value = 0;

  JVariant jvariant2(std::move(jvariant1));
  EXPECT_EQ(JType::Void, jvariant1.type());
  EXPECT_EQ(JType::Int, jvariant2.type());
  EXPECT_TRUE(jvariant2.get(&int_value) && (int_value == 732));
}


TEST_F(JVariantTest, MoveAssignRef) {
  const jobject kGlobalRef = reinterpret_cast<jobject>(0x2222);
  JVariant jvariant1 = JVariant::GlobalRef(kGlobalRef);

  jobject obj = nullptr;

  JVariant jvariant2 = JVariant::Int(18);
  jvariant2 = std::move(jvariant1);
  EXPECT_EQ(JType::Void, jvariant1.type());
  EXPECT_TRUE(jvariant2.get<jobject>(&obj) && (obj == kGlobalRef));

  EXPECT_CALL(jni_, DeleteGlobalRef(kGlobalRef))
      .WillOnce(Return());
}


TEST_F(JVariantTest, BorrowedRef) {
  const jobject kRef = reinterpret_cast<jobject>(0x384956834L);

  EXPECT_CALL(jni_, NewLocalRef(_)).Times(0);
  EXPECT_CALL(jni_, NewGlobalRef(_)).Times(0);
  EXPECT_CALL(jni_, NewWeakGlobalRef(_)).Times(0);
  EXPECT_CALL(jni_, DeleteLocalRef(_)).Times(0);
  EXPECT_CALL(jni_, DeleteGlobalRef(_)).Times(0);
  EXPECT_CALL(jni_, DeleteWeakGlobalRef(_)).Times(0);

  const JVariant& jvariant = JVariant::BorrowedRef(kRef);

  jobject obj = nullptr;
  EXPECT_TRUE(jvariant.get<jobject>(&obj));
  EXPECT_EQ(kRef, obj);
}


}  // namespace cdbg
}  // namespace devtools

