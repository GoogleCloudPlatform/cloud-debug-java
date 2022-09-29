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

#include "byte_source.h"

#include <cstdint>

#include "gtest/gtest.h"

namespace devtools {
namespace cdbg {

TEST(ByteSourceTest, DefaultConstructor) {
  EXPECT_EQ(0, ByteSource().size());
}


TEST(ByteSourceTest, StringConstructor) {
  std::string s = "123";
  ByteSource b(s);
  EXPECT_EQ(3, b.size());
  EXPECT_EQ('1', b.ReadInt8(0));
  EXPECT_EQ('2', b.ReadInt8(1));
  EXPECT_EQ('3', b.ReadInt8(2));
  EXPECT_FALSE(b.is_error());
}


TEST(ByteSourceTest, DataConstructor) {
  EXPECT_EQ(3, ByteSource("123", 3).size());
}


TEST(ByteSourceTest, MoveConstructor) {
  ByteSource b = ByteSource("123", 3);
  EXPECT_EQ(3, b.size());
}


TEST(ByteSourceTest, Assignment) {
  ByteSource b1;
  ByteSource b2("123", 3);

  EXPECT_EQ(0, b1.size());

  b1 = b2;

  EXPECT_EQ(3, b1.size());
}


TEST(ByteSourceTest, MoveAssignment) {
  ByteSource b1;
  ByteSource b2("123", 3);

  EXPECT_EQ(0, b1.size());

  b1 = std::move(b2);

  EXPECT_EQ(3, b1.size());
}


TEST(ByteSourceTest, Sub) {
  ByteSource source("0123456789", 10);

  const struct {
    int offset;
    int size;
    std::string expected;
  } test_cases[] = {
      { 0, 10, "0123456789" },
      { 8, 0, "" },
      { 9, 1, "9" },
      { 5, 2, "56" },
      { -3, 6, "012" },
      { 7, 5, "789" },
      { 10, 0, "" },
      { 10, 1, "" },
      { 10, 100, "" },
      { 12, 5, "" }
  };

  for (const auto& test_case : test_cases) {
    ByteSource sub = source.sub(test_case.offset, test_case.size);

    std::string actual;
    for (int i = 0; i < sub.size(); ++i) {
      actual.push_back(sub.ReadInt8(i));
    }

    EXPECT_FALSE(sub.is_error());
    EXPECT_EQ(test_case.expected, actual)
        << "offset: " << test_case.offset << ", size: " << test_case.size;
  }
}


TEST(ByteSourceTest, ReadInt8Success) {
  ByteSource b("123", 3);
  EXPECT_EQ('1', b.ReadInt8(0));
  EXPECT_EQ('2', b.ReadInt8(1));
  EXPECT_EQ('3', b.ReadInt8(2));
  EXPECT_FALSE(b.is_error());
}


TEST(ByteSourceTest, ReadInt8NegativeLeft) {
  ByteSource b("123", 3);
  EXPECT_EQ(0, b.ReadInt8(-1));
  EXPECT_TRUE(b.is_error());
}


TEST(ByteSourceTest, ReadInt8NegativeRight) {
  ByteSource b("123", 3);
  EXPECT_EQ(0, b.ReadInt8(3));
  EXPECT_TRUE(b.is_error());
}


TEST(ByteSourceTest, ReadInt32NegativeLeft) {
  ByteSource b("123", 3);
  EXPECT_EQ(0, b.ReadInt16BE(-1));
  EXPECT_TRUE(b.is_error());
}


TEST(ByteSourceTest, ReadInt16NegativeRight) {
  ByteSource b("123", 3);
  EXPECT_EQ(0, b.ReadInt16BE(2));
  EXPECT_TRUE(b.is_error());
}


TEST(ByteSourceTest, ReadInt32NegativeRight) {
  ByteSource b("123", 3);
  EXPECT_EQ(0, b.ReadInt32BE(0));
  EXPECT_TRUE(b.is_error());
}


TEST(ByteSourceTest, Endianness16) {
  uint8_t buffer[] = {0x12, 0x13};
  ByteSource b(buffer, sizeof(buffer));
  EXPECT_EQ(0x1213, b.ReadInt16BE(0));
  EXPECT_FALSE(b.is_error());
}


TEST(ByteSourceTest, Endianness32) {
  uint8_t buffer[] = {0x12, 0x13, 0x14, 0x15};
  ByteSource b(buffer, sizeof(buffer));
  EXPECT_EQ(0x12131415, b.ReadInt32BE(0));
  EXPECT_FALSE(b.is_error());
}


TEST(ByteSourceTest, Endianness64) {
  uint8_t buffer[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
  ByteSource b(buffer, arraysize(buffer));
  EXPECT_EQ(0x1122334455667788L, b.ReadInt64BE(0));
  EXPECT_FALSE(b.is_error());
}

}  // namespace cdbg
}  // namespace devtools
