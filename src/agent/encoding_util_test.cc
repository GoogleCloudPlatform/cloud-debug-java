/**
 * Copyright 2018 Google Inc. All Rights Reserved.
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

#include "encoding_util.h"

#include <cstdint>
#include <random>

#include "gmock/gmock.h"

// comes from @com_googlesource_code_re2//:re2, used for rune2char
#include "util/utf.h"

namespace devtools {
namespace cdbg {

// Copied from:
// https://github.com/abseil/abseil-cpp/blob/c2e754829628d1e9b7a16b3389cfdace76950fdf/absl/strings/escaping_test.cc#L281
static struct {
  std::string plaintext;
  std::string cyphertext;
} const base64_tests[] = {
    // Empty string.
    {{"", 0}, {"", 0}},

    // Basic bit patterns;
    // values obtained with "echo -n '...' | uuencode -m test"

    {{"\000", 1}, "AA=="},
    {{"\001", 1}, "AQ=="},
    {{"\002", 1}, "Ag=="},
    {{"\004", 1}, "BA=="},
    {{"\010", 1}, "CA=="},
    {{"\020", 1}, "EA=="},
    {{"\040", 1}, "IA=="},
    {{"\100", 1}, "QA=="},
    {{"\200", 1}, "gA=="},

    {{"\377", 1}, "/w=="},
    {{"\376", 1}, "/g=="},
    {{"\375", 1}, "/Q=="},
    {{"\373", 1}, "+w=="},
    {{"\367", 1}, "9w=="},
    {{"\357", 1}, "7w=="},
    {{"\337", 1}, "3w=="},
    {{"\277", 1}, "vw=="},
    {{"\177", 1}, "fw=="},
    {{"\000\000", 2}, "AAA="},
    {{"\000\001", 2}, "AAE="},
    {{"\000\002", 2}, "AAI="},
    {{"\000\004", 2}, "AAQ="},
    {{"\000\010", 2}, "AAg="},
    {{"\000\020", 2}, "ABA="},
    {{"\000\040", 2}, "ACA="},
    {{"\000\100", 2}, "AEA="},
    {{"\000\200", 2}, "AIA="},
    {{"\001\000", 2}, "AQA="},
    {{"\002\000", 2}, "AgA="},
    {{"\004\000", 2}, "BAA="},
    {{"\010\000", 2}, "CAA="},
    {{"\020\000", 2}, "EAA="},
    {{"\040\000", 2}, "IAA="},
    {{"\100\000", 2}, "QAA="},
    {{"\200\000", 2}, "gAA="},

    {{"\377\377", 2}, "//8="},
    {{"\377\376", 2}, "//4="},
    {{"\377\375", 2}, "//0="},
    {{"\377\373", 2}, "//s="},
    {{"\377\367", 2}, "//c="},
    {{"\377\357", 2}, "/+8="},
    {{"\377\337", 2}, "/98="},
    {{"\377\277", 2}, "/78="},
    {{"\377\177", 2}, "/38="},
    {{"\376\377", 2}, "/v8="},
    {{"\375\377", 2}, "/f8="},
    {{"\373\377", 2}, "+/8="},
    {{"\367\377", 2}, "9/8="},
    {{"\357\377", 2}, "7/8="},
    {{"\337\377", 2}, "3/8="},
    {{"\277\377", 2}, "v/8="},
    {{"\177\377", 2}, "f/8="},

    {{"\000\000\000", 3}, "AAAA"},
    {{"\000\000\001", 3}, "AAAB"},
    {{"\000\000\002", 3}, "AAAC"},
    {{"\000\000\004", 3}, "AAAE"},
    {{"\000\000\010", 3}, "AAAI"},
    {{"\000\000\020", 3}, "AAAQ"},
    {{"\000\000\040", 3}, "AAAg"},
    {{"\000\000\100", 3}, "AABA"},
    {{"\000\000\200", 3}, "AACA"},
    {{"\000\001\000", 3}, "AAEA"},
    {{"\000\002\000", 3}, "AAIA"},
    {{"\000\004\000", 3}, "AAQA"},
    {{"\000\010\000", 3}, "AAgA"},
    {{"\000\020\000", 3}, "ABAA"},
    {{"\000\040\000", 3}, "ACAA"},
    {{"\000\100\000", 3}, "AEAA"},
    {{"\000\200\000", 3}, "AIAA"},
    {{"\001\000\000", 3}, "AQAA"},
    {{"\002\000\000", 3}, "AgAA"},
    {{"\004\000\000", 3}, "BAAA"},
    {{"\010\000\000", 3}, "CAAA"},
    {{"\020\000\000", 3}, "EAAA"},
    {{"\040\000\000", 3}, "IAAA"},
    {{"\100\000\000", 3}, "QAAA"},
    {{"\200\000\000", 3}, "gAAA"},

    {{"\377\377\377", 3}, "////"},
    {{"\377\377\376", 3}, "///+"},
    {{"\377\377\375", 3}, "///9"},
    {{"\377\377\373", 3}, "///7"},
    {{"\377\377\367", 3}, "///3"},
    {{"\377\377\357", 3}, "///v"},
    {{"\377\377\337", 3}, "///f"},
    {{"\377\377\277", 3}, "//+/"},
    {{"\377\377\177", 3}, "//9/"},
    {{"\377\376\377", 3}, "//7/"},
    {{"\377\375\377", 3}, "//3/"},
    {{"\377\373\377", 3}, "//v/"},
    {{"\377\367\377", 3}, "//f/"},
    {{"\377\357\377", 3}, "/+//"},
    {{"\377\337\377", 3}, "/9//"},
    {{"\377\277\377", 3}, "/7//"},
    {{"\377\177\377", 3}, "/3//"},
    {{"\376\377\377", 3}, "/v//"},
    {{"\375\377\377", 3}, "/f//"},
    {{"\373\377\377", 3}, "+///"},
    {{"\367\377\377", 3}, "9///"},
    {{"\357\377\377", 3}, "7///"},
    {{"\337\377\377", 3}, "3///"},
    {{"\277\377\377", 3}, "v///"},
    {{"\177\377\377", 3}, "f///"},

    // Random numbers: values obtained with
    //
    //  #! /bin/bash
    //  dd bs=$1 count=1 if=/dev/random of=/tmp/bar.random
    //  od -N $1 -t o1 /tmp/bar.random
    //  uuencode -m test < /tmp/bar.random
    //
    // where $1 is the number of bytes (2, 3)

    {{"\243\361", 2}, "o/E="},
    {{"\024\167", 2}, "FHc="},
    {{"\313\252", 2}, "y6o="},
    {{"\046\041", 2}, "JiE="},
    {{"\145\236", 2}, "ZZ4="},
    {{"\254\325", 2}, "rNU="},
    {{"\061\330", 2}, "Mdg="},
    {{"\245\032", 2}, "pRo="},
    {{"\006\000", 2}, "BgA="},
    {{"\375\131", 2}, "/Vk="},
    {{"\303\210", 2}, "w4g="},
    {{"\040\037", 2}, "IB8="},
    {{"\261\372", 2}, "sfo="},
    {{"\335\014", 2}, "3Qw="},
    {{"\233\217", 2}, "m48="},
    {{"\373\056", 2}, "+y4="},
    {{"\247\232", 2}, "p5o="},
    {{"\107\053", 2}, "Rys="},
    {{"\204\077", 2}, "hD8="},
    {{"\276\211", 2}, "vok="},
    {{"\313\110", 2}, "y0g="},
    {{"\363\376", 2}, "8/4="},
    {{"\251\234", 2}, "qZw="},
    {{"\103\262", 2}, "Q7I="},
    {{"\142\312", 2}, "Yso="},
    {{"\067\211", 2}, "N4k="},
    {{"\220\001", 2}, "kAE="},
    {{"\152\240", 2}, "aqA="},
    {{"\367\061", 2}, "9zE="},
    {{"\133\255", 2}, "W60="},
    {{"\176\035", 2}, "fh0="},
    {{"\032\231", 2}, "Gpk="},

    {{"\013\007\144", 3}, "Cwdk"},
    {{"\030\112\106", 3}, "GEpG"},
    {{"\047\325\046", 3}, "J9Um"},
    {{"\310\160\022", 3}, "yHAS"},
    {{"\131\100\237", 3}, "WUCf"},
    {{"\064\342\134", 3}, "NOJc"},
    {{"\010\177\004", 3}, "CH8E"},
    {{"\345\147\205", 3}, "5WeF"},
    {{"\300\343\360", 3}, "wOPw"},
    {{"\061\240\201", 3}, "MaCB"},
    {{"\225\333\044", 3}, "ldsk"},
    {{"\215\137\352", 3}, "jV/q"},
    {{"\371\147\160", 3}, "+Wdw"},
    {{"\030\320\051", 3}, "GNAp"},
    {{"\044\174\241", 3}, "JHyh"},
    {{"\260\127\037", 3}, "sFcf"},
    {{"\111\045\033", 3}, "SSUb"},
    {{"\202\114\107", 3}, "gkxH"},
    {{"\057\371\042", 3}, "L/ki"},
    {{"\223\247\244", 3}, "k6ek"},
    {{"\047\216\144", 3}, "J45k"},
    {{"\203\070\327", 3}, "gzjX"},
    {{"\247\140\072", 3}, "p2A6"},
    {{"\124\115\116", 3}, "VE1O"},
    {{"\157\162\050", 3}, "b3Io"},
    {{"\357\223\004", 3}, "75ME"},
    {{"\052\117\156", 3}, "Kk9u"},
    {{"\347\154\000", 3}, "52wA"},
    {{"\303\012\142", 3}, "wwpi"},
    {{"\060\035\362", 3}, "MB3y"},
    {{"\130\226\361", 3}, "WJbx"},
    {{"\173\013\071", 3}, "ews5"},
    {{"\336\004\027", 3}, "3gQX"},
    {{"\357\366\234", 3}, "7/ac"},
    {{"\353\304\111", 3}, "68RJ"},
    {{"\024\264\131", 3}, "FLRZ"},
    {{"\075\114\251", 3}, "PUyp"},
    {{"\315\031\225", 3}, "zRmV"},
    {{"\154\201\276", 3}, "bIG+"},
    {{"\200\066\072", 3}, "gDY6"},
    {{"\142\350\267", 3}, "Yui3"},
    {{"\033\000\166", 3}, "GwB2"},
    {{"\210\055\077", 3}, "iC0/"},
    {{"\341\037\124", 3}, "4R9U"},
    {{"\161\103\152", 3}, "cUNq"},
    {{"\270\142\131", 3}, "uGJZ"},
    {{"\337\076\074", 3}, "3z48"},
    {{"\375\106\362", 3}, "/Uby"},
    {{"\227\301\127", 3}, "l8FX"},
    {{"\340\002\234", 3}, "4AKc"},
    {{"\121\064\033", 3}, "UTQb"},
    {{"\157\134\143", 3}, "b1xj"},
    {{"\247\055\327", 3}, "py3X"},
    {{"\340\142\005", 3}, "4GIF"},
    {{"\060\260\143", 3}, "MLBj"},
    {{"\075\203\170", 3}, "PYN4"},
    {{"\143\160\016", 3}, "Y3AO"},
    {{"\313\013\063", 3}, "ywsz"},
    {{"\174\236\135", 3}, "fJ5d"},
    {{"\103\047\026", 3}, "QycW"},
    {{"\365\005\343", 3}, "9QXj"},
    {{"\271\160\223", 3}, "uXCT"},
    {{"\362\255\172", 3}, "8q16"},
    {{"\113\012\015", 3}, "SwoN"},

    // various lengths, generated by this python script:
    //
    // from string import lowercase as lc
    // for i in range(27):
    //   print '{ %2d, "%s",%s "%s" },' % (i, lc[:i], ' ' * (26-i),
    //                                     lc[:i].encode('base64').strip())

    {{"", 0}, {"", 0}},
    {"a", "YQ=="},
    {"ab", "YWI="},
    {"abc", "YWJj"},
    {"abcd", "YWJjZA=="},
    {"abcde", "YWJjZGU="},
    {"abcdef", "YWJjZGVm"},
    {"abcdefg", "YWJjZGVmZw=="},
    {"abcdefgh", "YWJjZGVmZ2g="},
    {"abcdefghi", "YWJjZGVmZ2hp"},
    {"abcdefghij", "YWJjZGVmZ2hpag=="},
    {"abcdefghijk", "YWJjZGVmZ2hpams="},
    {"abcdefghijkl", "YWJjZGVmZ2hpamts"},
    {"abcdefghijklm", "YWJjZGVmZ2hpamtsbQ=="},
    {"abcdefghijklmn", "YWJjZGVmZ2hpamtsbW4="},
    {"abcdefghijklmno", "YWJjZGVmZ2hpamtsbW5v"},
    {"abcdefghijklmnop", "YWJjZGVmZ2hpamtsbW5vcA=="},
    {"abcdefghijklmnopq", "YWJjZGVmZ2hpamtsbW5vcHE="},
    {"abcdefghijklmnopqr", "YWJjZGVmZ2hpamtsbW5vcHFy"},
    {"abcdefghijklmnopqrs", "YWJjZGVmZ2hpamtsbW5vcHFycw=="},
    {"abcdefghijklmnopqrst", "YWJjZGVmZ2hpamtsbW5vcHFyc3Q="},
    {"abcdefghijklmnopqrstu", "YWJjZGVmZ2hpamtsbW5vcHFyc3R1"},
    {"abcdefghijklmnopqrstuv", "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dg=="},
    {"abcdefghijklmnopqrstuvw", "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnc="},
    {"abcdefghijklmnopqrstuvwx", "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4"},
    {"abcdefghijklmnopqrstuvwxy", "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eQ=="},
    {"abcdefghijklmnopqrstuvwxyz", "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXo="},
};

static struct {
  std::string plaintext;
  std::string cyphertext;
} const base64_extra_tests[] = {
    // Tests added based on a failing capture_data_collector_test to test the
    // specific scenario with a '1' in the most significant bit position.
    {"\xC3\xBC", "w7w="},
    {"\xFF", "/w=="},
    {"\xFF\xFF", "//8="},
    {"\xFF\xFF\xFF", "////"},
    {"\xFF\xFF\xFF\xFF", "/////w=="},
};

TEST(EncodingUtil, Base64Encode) {
  for (const auto& tc : base64_tests) {
    std::string result = Base64Encode(&tc.plaintext[0], tc.plaintext.size());
    EXPECT_EQ(tc.cyphertext, result);
  }

  for (const auto& tc : base64_extra_tests) {
    std::string result = Base64Encode(&tc.plaintext[0], tc.plaintext.size());
    EXPECT_EQ(tc.cyphertext, result);
  }

  // If length is zero, the plaintext pointer must be ignored.
  EXPECT_EQ(Base64Encode(nullptr, 0), "");
}

bool IsGood(int uv) {
  if (uv < 0) {
    return false;
  }
  if (uv > 0x10FFFF) {
    return false;
  }
  if ((0xD800 <= uv) && (uv <= 0xDFFF)) {
    return false;
  }
  return true;
}

int IntToUTF8(int unicode_codepoint, char* next_string) {
  return re2::runetochar(next_string, &unicode_codepoint);
}

// Just for debugging; print 16 bytes per line of hex, then printable ASCII
void PrintHex(const std::string& str) {
  const uint8_t* ptr = reinterpret_cast<const uint8_t*>(str.data());
  int len = str.size();
  for (int i = 0; i < len; i += 16) {
    for (int j = 0; j < 16; j++) {
      if ((i + j) >= len) {
        continue;
      }
      uint8_t c = ptr[i + j];
      fprintf(stderr, "%02x ", c);
      if ((j & 3) == 3) {
        fprintf(stderr, " ");
      }
    }
    fprintf(stderr, "  ");
    for (int j = 0; j < 16; j++) {
      if ((i + j) >= len) {
        continue;
      }
      uint8_t c = ptr[i + j];
      if ((0x20 <= c) && (c <= 0x7e)) {
        fprintf(stderr, "%c", c);
      } else {
        fprintf(stderr, ".");
      }
      if ((j & 3) == 3) {
        fprintf(stderr, " ");
      }
    }
    fprintf(stderr, "\n");
  }
}

// Exhaustive test of all valid UTF-8 code points
bool TestUTF8SpnStructurallyValid1(bool rand) {
  static const int kAllPointsCount = 17 * 64 * 1024;  // 17 planes
  static const int kRealUTF8CharBytes = 4;  // can't use kMaxUTF8CharBytes,
                                            // which is still 3
  std::vector<int> codepoints;
  char* utf8string = new char[kAllPointsCount * kRealUTF8CharBytes];

  // Make a list of all Unicode codepoints
  for (int uv = 0; uv < kAllPointsCount; uv++) {
    if (IsGood(uv)) {
      codepoints.push_back(uv);
    }
  }

  // Optionally permute the list pseudo-randomly.
  if (rand) {
    std::random_device rd;
    std::mt19937 g(rd());

    std::shuffle(codepoints.begin(), codepoints.end(), g);
  }

  // Make a long UTF-8 string of all these code points
  char* next_string = utf8string;
  for (int i = 0; i < codepoints.size(); i++) {
    next_string += IntToUTF8(codepoints[i], next_string);
  }

  // See how much of it is structurally valid
  int utf8stringlength = next_string - utf8string;
  int n = ValidateUtf8(utf8string, utf8stringlength);
  fprintf(stderr, "utf8stringlength %d, n %d\n", utf8stringlength, n);
  if (n != utf8stringlength) {
    // show 16 bytes around failure
    int lo = (n - 8) < 0 ? 0 : (n - 8);
    int hi = (n + 8) > utf8stringlength ? utf8stringlength : (n + 8);
    std::string str2(&utf8string[lo], hi - lo);
    PrintHex(str2);
  }
  delete[] utf8string;

  // Return true if it is all valid
  return (n == utf8stringlength);
}

// Size of test string for pseudo-random coverage
static const int kTestStringLen = 1024;

// Run pseudo-random N-byte strings, more of a fuzz test
uint32_t TestUTF8SpnStructurallyValid2(int span) {
  char test_string[kTestStringLen];

  // Generate random string (but the same every time we run this)
  std::mt19937 g;
  g.seed(1);
  for (int i = 0; i < kTestStringLen; i++) {
    test_string[i] = static_cast<char>(g());
  }

  // Go do N=span bytes at once
  for (int i = 0; i <= (kTestStringLen - span); i += span) {
    ValidateUtf8(&test_string[i], span);
  }

  return 0;
}

TEST(EncodingUtil, ValidateUtf8) {
  // Test simple good strings
  EXPECT_EQ(4, ValidateUtf8("abcd", 4));
  EXPECT_EQ(4, ValidateUtf8("a\0cd", 4));             // NUL
  EXPECT_EQ(4, ValidateUtf8("ab\xc2\x81", 4));        // 2-byte
  EXPECT_EQ(4, ValidateUtf8("a\xe2\x81\x81", 4));     // 3-byte
  EXPECT_EQ(4, ValidateUtf8("\xf2\x81\x81\x81", 4));  // 4

  // Test simple bad strings
  EXPECT_EQ(3, ValidateUtf8("abc\x80", 4));           // bad char
  EXPECT_EQ(3, ValidateUtf8("abc\xc2", 4));           // trunc 2
  EXPECT_EQ(2, ValidateUtf8("ab\xe2\x81", 4));        // trunc 3
  EXPECT_EQ(1, ValidateUtf8("a\xf2\x81\x81", 4));     // trunc 4
  EXPECT_EQ(2, ValidateUtf8("ab\xc0\x81", 4));        // not 1
  EXPECT_EQ(1, ValidateUtf8("a\xe0\x81\x81", 4));     // not 2
  EXPECT_EQ(0, ValidateUtf8("\xf0\x81\x81\x81", 4));  // not 3
  EXPECT_EQ(0, ValidateUtf8("\xf4\xbf\xbf\xbf", 4));  // big
  // surrogate min, max
  EXPECT_EQ(0, ValidateUtf8("\xED\xA0\x80", 3));  // U+D800
  EXPECT_EQ(0, ValidateUtf8("\xED\xBF\xBF", 3));  // U+DFFF

  // non-shortest forms should all return false
  EXPECT_EQ(0, ValidateUtf8("\xc0\x80", 2));
  EXPECT_EQ(0, ValidateUtf8("\xc1\xbf", 2));
  EXPECT_EQ(0, ValidateUtf8("\xe0\x80\x80", 3));
  EXPECT_EQ(0, ValidateUtf8("\xe0\x9f\xbf", 3));
  EXPECT_EQ(0, ValidateUtf8("\xf0\x80\x80\x80", 4));
  EXPECT_EQ(0, ValidateUtf8("\xf0\x83\xbf\xbf", 4));

  EXPECT_TRUE(TestUTF8SpnStructurallyValid1(false));  // all valid
  EXPECT_TRUE(TestUTF8SpnStructurallyValid1(true));   // permuted valid

  // More of a fuzz test, just running manning random values, most of them bad.
  // Bassically if the test returns that's a success.
  EXPECT_EQ(0, TestUTF8SpnStructurallyValid2(16));
  EXPECT_EQ(0, TestUTF8SpnStructurallyValid2(4));

  EXPECT_EQ(0, ValidateUtf8("\xc7\xc8\xcd\xcb", 4));
}

}  // namespace cdbg
}  // namespace devtools
