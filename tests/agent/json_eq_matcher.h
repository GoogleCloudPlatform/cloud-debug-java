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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JSON_EQ_MATCHER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JSON_EQ_MATCHER_H_

#include "gmock/gmock.h"
#include "json/json.h"
#include "src/agent/model_json.h"

namespace devtools {
namespace cdbg {

// Helper method to canonicalize two JSON strings, so that they can be
// compared as strings without being worried about extra spaces or order of
// fields.
std::string CanonicalizeJson(std::string json_string) {
  // We use single quotes in unit tests since they are less cumbersome to
  // inline in C++ code. Convert it back, so that the string becomes a proper
  // JSON.
  std::replace(json_string.begin(), json_string.end(), '\'', '\"');

  // Parse JSON string.
  Json::Value root;
  Json::Reader reader;
  if (!reader.parse(json_string, root)) {
    ADD_FAILURE() << "JSON string could not be parsed: "
                  << reader.getFormattedErrorMessages()
                  << std::endl << json_string;
    return std::string();
  }

  // Pretty-print JSON string.
  Json::StyledWriter writer;
  return writer.write(root);
}

void ExpectJsonEq(std::string expected_json_string,
                  std::string actual_json_string) {
  expected_json_string = CanonicalizeJson(std::move(expected_json_string));
  actual_json_string = CanonicalizeJson(std::move(actual_json_string));

  EXPECT_TRUE(expected_json_string == actual_json_string)
      << "********* Expected *********"
      << std::endl << expected_json_string
      << "********** Actual **********"
      << std::endl << actual_json_string;
}

void ExpectJsonEq(
    const BreakpointModel& expected_breakpoint,
    const BreakpointModel* actual_breakpoint) {
  ASSERT_NE(nullptr, actual_breakpoint);

  const std::string expected_json_string =
      BreakpointToPrettyJson(expected_breakpoint).data;

  const std::string actual_json_string =
      BreakpointToPrettyJson(*actual_breakpoint).data;

  EXPECT_TRUE(expected_json_string == actual_json_string)
      << "********* Expected *********"
      << std::endl << expected_json_string
      << "********** Actual **********"
      << std::endl << actual_json_string;
}


// Helper class to compare JSON strings in unit-tests. For simplicity
// it doesn't distinguish between single quotes and double quotes.
class JsonEqMatcher : public testing::MatcherInterface<const std::string&> {
 public:
  explicit JsonEqMatcher(std::string expected)
      : expected_(std::move(expected)) {}

  bool MatchAndExplain(
      const std::string& actual,
      testing::MatchResultListener* /* listener */) const override {
    return CanonicalizeJson(actual) == CanonicalizeJson(expected_);
  }

  void DescribeTo(::std::ostream* os) const override {
    *os << expected_;
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << expected_;
  }

 private:
  const std::string expected_;
};

inline testing::Matcher<const std::string&> JsonEq(std::string expected) {
  return MakeMatcher(new JsonEqMatcher(std::move(expected)));
}

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JSON_EQ_MATCHER_H_
