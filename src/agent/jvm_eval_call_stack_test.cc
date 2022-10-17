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

#include "jvm_eval_call_stack.h"

#include <algorithm>
#include <cstdint>
#include <iterator>

#include "mock_jni_env.h"
#include "mock_jvmti_env.h"
#include "gmock/gmock.h"

ABSL_DECLARE_FLAG(int32, cdbg_max_stack_depth);

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

static const jthread thread = reinterpret_cast<jthread>(0x67125374);

static const jmethodID method1 = reinterpret_cast<jmethodID>(0x72834634158L);
static const jmethodID method2 = reinterpret_cast<jmethodID>(0x12543468754L);

static const jclass class1 = reinterpret_cast<jclass>(0x3874657834);

static const struct MethodInfoTable {
  jmethodID method;
  const char* method_name;
  jclass method_class;
} method_info_table[] = {
  {
    method1,
    "FirstMethod",
    class1
  },
  {
    method2,
    "SecondMethod",
    class1
  }
};

static const struct ClassInfoTable {
  jclass cls;
  const char* class_signature;
  const char* class_generic;
  const char* source_file_name;
} class_info_table[] = {
  {
    class1,
    "Lcom/myorg/myprod/Class1;",
    nullptr,
    "Class1.java"
  }
};

static const jvmtiLineNumberEntry method1_line_table[] = {
  { 100400, 104 },
  { 100100, 101 },
  { 100300, 103 },
  { 100200, 102 }
};

static const jvmtiLineNumberEntry method2_line_table[] = {
  { 200100, 202 }
};

static const jvmtiFrameInfo stack_frames[] = {
    { method1, 100100 },
    { method1, 100399 },
    { method1, 100401 },
    { method2, 200101 }
};

const JvmEvalCallStack::FrameInfo expected_frames[] = {
    {"Lcom/myorg/myprod/Class1;", std::string(), "FirstMethod", "Class1.java",
     101},
    {"Lcom/myorg/myprod/Class1;", std::string(), "FirstMethod", "Class1.java",
     103},
    {"Lcom/myorg/myprod/Class1;", std::string(), "FirstMethod", "Class1.java",
     104},
    {"Lcom/myorg/myprod/Class1;", std::string(), "SecondMethod", "Class1.java",
     202},
};

static_assert(arraysize(expected_frames) == arraysize(stack_frames), "frames");

std::ostream& operator<<(std::ostream& os,
                         const JvmEvalCallStack::FrameInfo& frame_info) {
  os << "JvmEvalCallStack::FrameInfo:" << std::endl
     << "    class_signature: " << frame_info.class_signature << std::endl
     << "    class_generic: " << frame_info.class_generic << std::endl
     << "    method_name: " << frame_info.method_name << std::endl
     << "    source_file_name: " << frame_info.source_file_name << std::endl
     << "    line_number: " << frame_info.line_number;
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const JvmEvalCallStack::JvmFrame& jvm_frame) {
  os << "JvmEvalCallStack::JvmFrame:" << std::endl << "    code_location: {"
     << std::hex << std::showbase << jvm_frame.code_location.method << ", "
     << jvm_frame.code_location.location << "}" << std::endl
     << "    frame_info_key: " << std::dec << jvm_frame.frame_info_key;
  return os;
}


class FrameInfoEqMatcher
    : public MatcherInterface<const JvmEvalCallStack::FrameInfo&> {
 public:
  explicit FrameInfoEqMatcher(const JvmEvalCallStack::FrameInfo& expected)
      : expected_(expected) {
  }

  bool MatchAndExplain(
      const JvmEvalCallStack::FrameInfo& actual,
      MatchResultListener* /* listener */) const override {
    return (expected_.class_signature == actual.class_signature) &&
           (expected_.class_generic == actual.class_generic) &&
           (expected_.method_name == actual.method_name) &&
           (expected_.source_file_name == actual.source_file_name) &&
           (expected_.line_number == actual.line_number);
  }

  void DescribeTo(::std::ostream* os) const override {
    *os << expected_;
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << expected_;
  }

 private:
  const JvmEvalCallStack::FrameInfo& expected_;
};


inline Matcher<const JvmEvalCallStack::FrameInfo&> FrameInfoEq(
    const JvmEvalCallStack::FrameInfo& expected) {
  return MakeMatcher(new FrameInfoEqMatcher(expected));
}


bool operator== (
    const JvmEvalCallStack::JvmFrame& jf1,
    const JvmEvalCallStack::JvmFrame& jf2) {
  return (jf1.code_location.method == jf2.code_location.method) &&
         (jf1.code_location.location == jf2.code_location.location) &&
         (jf1.frame_info_key == jf2.frame_info_key);
}


// Implement custom matcher because regular test::Eq matcher doesn't compile
// with JvmEvalCallStack::JvmFrame (probably because of jvmtiFrameInfo typedef).
class JvmFrameEqMatcher
    : public MatcherInterface<const JvmEvalCallStack::JvmFrame&> {
 public:
  explicit JvmFrameEqMatcher(const JvmEvalCallStack::JvmFrame& expected)
      : expected_(expected) {
  }

  bool MatchAndExplain(
      const JvmEvalCallStack::JvmFrame& actual,
      MatchResultListener* /* listener */) const override {
    return expected_ == actual;
  }

  void DescribeTo(::std::ostream* os) const override {
    *os << expected_;
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << expected_;
  }

 private:
  const JvmEvalCallStack::JvmFrame& expected_;
};


inline Matcher<const JvmEvalCallStack::JvmFrame&> JvmFrameEq(
    const JvmEvalCallStack::JvmFrame& expected) {
  return MakeMatcher(new JvmFrameEqMatcher(expected));
}


// Implement custom matcher because regular test::Ne matcher doesn't compile
// with JvmEvalCallStack::JvmFrame (probably because of jvmtiFrameInfo typedef).
class JvmFrameNeMatcher
    : public MatcherInterface<const JvmEvalCallStack::JvmFrame&> {
 public:
  explicit JvmFrameNeMatcher(const JvmEvalCallStack::JvmFrame& expected)
      : expected_(expected) {
  }

  bool MatchAndExplain(
      const JvmEvalCallStack::JvmFrame& actual,
      MatchResultListener* /* listener */) const override {
    return !(expected_ == actual);
  }

  void DescribeTo(::std::ostream* os) const override {
    *os << expected_;
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << expected_;
  }

 private:
  const JvmEvalCallStack::JvmFrame& expected_;
};


inline Matcher<const JvmEvalCallStack::JvmFrame&> JvmFrameNe(
    const JvmEvalCallStack::JvmFrame& expected) {
  return MakeMatcher(new JvmFrameNeMatcher(expected));
}


class JvmEvalCallStackTest : public ::testing::Test {
 protected:
  JvmEvalCallStackTest() : global_jvm_(&jvmti_, &jni_) {
  }

  void SetUp() override {
    EXPECT_CALL(jni_, GetObjectRefType(_))
        .WillRepeatedly(Return(JNILocalRefType));

    EXPECT_CALL(jni_, DeleteLocalRef(_))
        .WillRepeatedly(Return());

    EXPECT_CALL(jvmti_, Deallocate(NotNull()))
      .WillRepeatedly(Return(JVMTI_ERROR_NONE));

    EXPECT_CALL(jvmti_, GetStackTrace(thread, 0, _, NotNull(), NotNull()))
        .WillRepeatedly(DoAll(
            SetArrayArgument<3>(
                stack_frames,
                stack_frames + arraysize(stack_frames)),
            SetArgPointee<4>(arraysize(stack_frames)),
            Return(JVMTI_ERROR_NONE)));

    SetupGetLineNumberTable(
        method1,
        method1_line_table,
        arraysize(method1_line_table));

    SetupGetLineNumberTable(
        method2,
        method2_line_table,
        arraysize(method2_line_table));

    for (const auto& method_info : method_info_table) {
      EXPECT_CALL(
          jvmti_,
          GetMethodName(
              method_info.method,
              NotNull(),
              nullptr,
              nullptr))
          .WillRepeatedly(DoAll(
              SetArgPointee<1>(const_cast<char*>(method_info.method_name)),
              Return(JVMTI_ERROR_NONE)));

      EXPECT_CALL(
          jvmti_,
          GetMethodDeclaringClass(method_info.method, NotNull()))
          .WillRepeatedly(DoAll(
              SetArgPointee<1>(method_info.method_class),
              Return(JVMTI_ERROR_NONE)));
    }

    for (const auto& class_info : class_info_table) {
      EXPECT_CALL(
          jvmti_,
          GetClassSignature(class_info.cls, NotNull(), NotNull()))
          .WillRepeatedly(DoAll(
              SetArgPointee<1>(const_cast<char*>(class_info.class_signature)),
              SetArgPointee<2>(const_cast<char*>(class_info.class_generic)),
              Return(JVMTI_ERROR_NONE)));

      EXPECT_CALL(jvmti_, GetSourceFileName(class_info.cls, NotNull()))
          .WillRepeatedly(DoAll(
              SetArgPointee<1>(
                  const_cast<char*>(class_info.source_file_name)),
              Return(JVMTI_ERROR_NONE)));
    }
  }

  void SetupGetLineNumberTable(
      jmethodID method,
      const jvmtiLineNumberEntry* table,
      int size) {
    EXPECT_CALL(jvmti_, GetLineNumberTable(method, NotNull(), NotNull()))
        .WillRepeatedly(DoAll(
            SetArgPointee<1>(size),
            SetArgPointee<2>(const_cast<jvmtiLineNumberEntry*>(table)),
            Return(JVMTI_ERROR_NONE)));
  }

 protected:
  MockJvmtiEnv jvmti_;
  MockJNIEnv jni_;
  GlobalJvmEnv global_jvm_;
  JvmEvalCallStack eval_call_stack_;
};


// Simulate successful reading of the call stack
TEST_F(JvmEvalCallStackTest, Success) {
  std::vector<JvmEvalCallStack::JvmFrame> frames;
  eval_call_stack_.Read(thread, &frames);

  ASSERT_EQ(arraysize(stack_frames), frames.size());
  for (int i = 0; i < arraysize(stack_frames); ++i) {
    EXPECT_EQ(stack_frames[i].method, frames[i].code_location.method);
    EXPECT_EQ(stack_frames[i].location, frames[i].code_location.location);
  }

  for (int i = 0; i < arraysize(expected_frames); ++i) {
    EXPECT_THAT(
        eval_call_stack_.ResolveCallFrameKey(frames[i].frame_info_key),
        FrameInfoEq(expected_frames[i]));
  }
}


// Simulate successful reading of the call stack, where JNI truncates the
// number of frames.
TEST_F(JvmEvalCallStackTest, SuccessMoreFramesThanMaxDepth) {
  absl::FlagSaver fs;

  int32_t kNewMaxStackDepth = 2;
  ASSERT_LT(kNewMaxStackDepth, arraysize(stack_frames));
  absl::SetFlag(&FLAGS_cdbg_max_stack_depth, kNewMaxStackDepth);

  // Hides the default GetStackFrames expectation in SetUp.
  EXPECT_CALL(
      jvmti_,
      GetStackTrace(thread, 0, kNewMaxStackDepth, NotNull(), NotNull()))
      .WillOnce(DoAll(
          SetArrayArgument<3>(
              stack_frames,
              stack_frames + kNewMaxStackDepth),
          SetArgPointee<4>(kNewMaxStackDepth),
          Return(JVMTI_ERROR_NONE)));

  std::vector<JvmEvalCallStack::JvmFrame> frames;
  eval_call_stack_.Read(thread, &frames);

  ASSERT_EQ(kNewMaxStackDepth, frames.size());
  for (int i = 0; i < kNewMaxStackDepth; ++i) {
    EXPECT_EQ(stack_frames[i].method, frames[i].code_location.method);
    EXPECT_EQ(stack_frames[i].location, frames[i].code_location.location);
  }

  for (int i = 0; i < kNewMaxStackDepth; ++i) {
    EXPECT_THAT(
        eval_call_stack_.ResolveCallFrameKey(frames[i].frame_info_key),
        FrameInfoEq(expected_frames[i]));
  }
}


// Simulate failure to GetStackTrace and verify that JvmEvalCallStack::Read
// returns empty array.
TEST_F(JvmEvalCallStackTest, GetStackTrace_Failure) {
  EXPECT_CALL(jvmti_, GetStackTrace(thread, 0, _, NotNull(), NotNull()))
      .WillOnce(Return(JVMTI_ERROR_THREAD_NOT_ALIVE));

  std::vector<JvmEvalCallStack::JvmFrame> frames;
  eval_call_stack_.Read(thread, &frames);
  EXPECT_EQ(0, frames.size());
}


// Verify method+location cache
TEST_F(JvmEvalCallStackTest, LocationCache) {
  std::vector<JvmEvalCallStack::JvmFrame> frames1;
  eval_call_stack_.Read(thread, &frames1);
  ASSERT_EQ(arraysize(stack_frames), frames1.size());

  std::vector<JvmEvalCallStack::JvmFrame> frames2;
  eval_call_stack_.Read(thread, &frames2);

  // Identical call frame keys means that cache worked.
  ASSERT_EQ(frames1.size(), frames2.size());
  for (int i = 0; i < frames1.size(); ++i) {
    EXPECT_THAT(frames1[i], JvmFrameEq(frames2[i]));
  }
}


// Verify cache invalidation upon method unload
TEST_F(JvmEvalCallStackTest, MethodUnload) {
  std::vector<JvmEvalCallStack::JvmFrame> frames1;
  eval_call_stack_.Read(thread, &frames1);
  ASSERT_EQ(arraysize(stack_frames), frames1.size());

  {
    GlobalNoJni no_jni;
    eval_call_stack_.JvmtiOnCompiledMethodUnload(method2);
  }

  std::vector<JvmEvalCallStack::JvmFrame> frames2;
  eval_call_stack_.Read(thread, &frames2);

  ASSERT_EQ(frames1.size(), frames2.size());

  // Cache was invalidated for method2, expect different call frame keys.
  // The cache for method1 is still there, so call frame keys will be
  // identical.
  for (int i = 0; i < frames1.size(); ++i) {
    if (stack_frames[i].method == method2) {
      EXPECT_THAT(frames1[i], JvmFrameNe(frames2[i]));
    } else {
      EXPECT_THAT(frames1[i], JvmFrameEq(frames2[i]));
    }
  }
}


TEST_F(JvmEvalCallStackTest, InjectFrame) {
  EvalCallStack::FrameInfo frame1;
  frame1.class_signature = "sig1";
  frame1.method_name = "method1";

  EvalCallStack::FrameInfo frame2;
  frame2.class_signature = "sig1";
  frame2.method_name = "method1";

  int key1 = eval_call_stack_.InjectFrame(frame1);
  int key2 = eval_call_stack_.InjectFrame(frame2);

  EXPECT_EQ(0, key1);
  EXPECT_EQ(1, key2);

  EXPECT_THAT(eval_call_stack_.ResolveCallFrameKey(key1), FrameInfoEq(frame1));
  EXPECT_THAT(eval_call_stack_.ResolveCallFrameKey(key2), FrameInfoEq(frame2));
}


}  // namespace cdbg
}  // namespace devtools
