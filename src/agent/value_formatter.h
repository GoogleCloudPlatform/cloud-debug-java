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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_VALUE_FORMATTER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_VALUE_FORMATTER_H_

#include <memory>

#include "common.h"
#include "type_util.h"

namespace devtools {
namespace cdbg {

// Maximum number of characters to print by default. Longer strings are
// truncated.
constexpr int kDefaultMaxStringLength = 256;

// Maximum string length to capture in watched expressions.
constexpr int kExtendedMaxStringLength = 2048;

// Set of methods to format JVariant to a string.
class ValueFormatter {
 public:
  struct Options {
    // Determines whether a string should be wrapped with double quotes.
    bool quote_string = true;

    // Maximum string length to capture. Longer strings are truncated.
    int max_string_length = kDefaultMaxStringLength;
  };

  // Determines whether the stored data can be formatted as a string. For
  // example primitive types and string are values, but Servlet class is not.
  static bool IsValue(const NamedJVariant& data);

  // Checks whether "well_known_jclass" corresponds to an object that we
  // treat as immutable value (like Integer and String).
  static bool IsImmutableValueObject(WellKnownJClass well_known_jclass);

  // Computes approximated amount of data that the value will take when
  // formatted. Includes both name and value, but doesn't count any formatting
  // overhead.
  static int GetTotalDataSize(const NamedJVariant& data);

  // Formats variable value to a string format. "Format" can be called
  // even if this is a reference. In this case the function will set
  // something like "<Object>". The optional "type" is set to the type
  // name of "source" (e.g. "int"). If type not needed, "type" can be nullptr.
  // Returns status message of "Format" operation (can be nullptr).
  static std::unique_ptr<StatusMessageModel> Format(
      const NamedJVariant& source, const Options& options,
      std::string* formatted_value, std::string* type);
};


}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_VALUE_FORMATTER_H_

