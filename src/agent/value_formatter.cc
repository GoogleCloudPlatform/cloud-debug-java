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

#include "value_formatter.h"

#include <cstdint>
#include <numeric>
#include <queue>

#include "jni_utils.h"
#include "model_util.h"

namespace devtools {
namespace cdbg {

static constexpr char kNull[] = "null";

// Normal string suffix used when the string is not truncated.
static constexpr char kNormalStringSuffix[] = "\"";
static constexpr char kNormalStringSuffixNoQuotes[] = "";

// String suffix indicating that not all characters were captured.
static constexpr char kTruncatedStringSuffix[] = " ...\"";
static constexpr char kTruncatedStringSuffixNoQuotes[] = " ...";

static bool IsJavaString(const NamedJVariant& data) {
  return (data.value.type() == JType::Object) &&
         ValueFormatter::IsImmutableValueObject(data.well_known_jclass);
}


// The code below replaces all occurrences of "\xC0\x80" in *data_ptr with
// "\\u0000". It is optimized for performance: works in-place, and runs O(n).
static void ScrubEmbeddedZeroCharacters(std::string* data_ptr) {
  std::string& data = *data_ptr;

  // Make a pass to identify all embedded zeros (2 bytes).
  std::vector<int> zeros;
  for (int i = 0; i + 1 < data.size(); ++i) {
    // Check if the next two characters represent an embedded zero.
    if (static_cast<uint8_t>(data[i]) == 0xC0 &&
        static_cast<uint8_t>(data[i + 1]) == 0x80) {
      zeros.push_back(i);
      ++i;  // Skip the next byte (0x80).
      continue;
    }
  }

  // Nothing to replace. Zero copy exit path.
  if (zeros.empty())
    return;

  // Expand the 'data' string to its new size:
  //  * Each 2-byte embedded zero is replaced by 6 bytes.
  int old_size = data.size();
  data.resize(data.size() + (6 - 2) * zeros.size());

  // Iterate from the last byte of the 'data' string towards the first byte, and
  // transform it in place.
  // Example:
  //  Input:
  //    data == "abc" + \0 + "def" == "abc\xC0\x80def"
  //    length = 8
  //    zeros.size() = 1
  //    zeros.back() = 3
  //
  //  Loop Iteration 0:
  //    old_index = 7
  //    new index = 7 + (6 - 2) * 1 = 11
  //    data[11] = data[7]
  //    data == "abc\xC0\x80def***f"
  //
  //  Loop Iteration 1:
  //    old_index = 6
  //    new index = 6 + (6 - 2) * 1 = 10
  //    data[10] = data[6]
  //    data == "abc\xC0\x80def**ef"
  //
  //  Loop Iteration 2:
  //    old_index = 5
  //    new index = 5 + (6 - 2) * 1 = 9
  //    data == "abc\xC0\x80def*def"
  //
  //  Loop Iteration 3:
  //    old_index = 4
  //    new index = 4 + (6 - 2) * 1 = 8
  //    data[3] = '\\';
  //    data[4] = 'u';
  //    ...
  //    data[8] = '0';
  //    data == "abc\\u0000def"
  //    zeros.pop()
  //
  //  Loop Iteration 4:
  //    zeros is empty, return.
  //
  for (int old_index = old_size - 1; old_index >= 0; --old_index) {
    // No need to copy the remainder bytes as they are the same as original.
    if (zeros.empty())
      return;

    // We shrink the 'zeros' vector so that we can easily find the new location
    // of each byte in the input. Each byte shifts by a number of bytes
    // proportional to the remaining size of 'zeros' vector.
    int new_index = old_index + (6 - 2) * zeros.size();

    // Replace the bytes for embedded zero.
    if (old_index == zeros.back() + 1) {  // data[i] == 0x80
      // Put "\\u0000" to the output.
      data[new_index - 5] = '\\';
      data[new_index - 4] = 'u';
      data[new_index - 3] = '0';
      data[new_index - 2] = '0';
      data[new_index - 1] = '0';
      data[new_index - 0] = '0';
      zeros.pop_back();
      --old_index;  // Skip the next byte (0xc0).
      continue;
    }

    // Ordinary UTF8 character. Just copy to new location.
    data[new_index] = data[old_index];
  }
}

// The code below replaces all occurrences of unicode supplementary characters
// in *data_ptr with a different encoding of the same characters. It is
// optimized for performance: works in-place, and runs O(n).
static void ScrubSupplementaryCharacters(std::string* data_ptr) {
  std::string& data = *data_ptr;

  // Make a pass to identify all supplementary characters (6 bytes).
  std::queue<std::pair<int, int32_t>> supplementaries;
  for (int i = 0; i + 5 < data.size(); ++i) {
    // Check if the next 6 bytes represent a supplementary character.
    // Modified UTF8 encodes a supplementary character as a UTF16 high/low
    // surrogate pair encoded in UTF8.
    //  The high surrogate is in range [0xD800, 0xDBFF].
    //  The low surrogate is in range [0xDCFF, 0xDFFF].
    //
    // When such a 16-bit value is encoded in UTF8, it looks like this:
    //   Byte 1: 1110xxxx
    //   Byte 2: 10xxxxxx
    //   Byte 3: 10xxxxxx
    //
    // Since the top 6 bits of high/low surrogates are constant, and only the
    // low 10 bits are variable, the UTF8 encoding of these high/low surrogates
    // look like this:
    //  High Surrogate: [0xD800, 0xDBFF]
    //    Byte 1: 11101101
    //    Byte 2: 1010xxxx
    //    Byte 3: 10xxxxxx
    //  Low Surrogate: [0xDCFF, 0xDFFF]
    //    Byte 1: 11101101
    //    Byte 2: 1011xxxx
    //    Byte 3: 10xxxxxx
    if ((data[i + 0] & 0xFF) == 0xED &&   // 11101101
        (data[i + 1] & 0xF0) == 0xA0 &&   // 1010xxxx
        (data[i + 2] & 0xC0) == 0x80 &&   // 10xxxxxx
        (data[i + 3] & 0xFF) == 0xED &&   // 11101101
        (data[i + 4] & 0xF0) == 0xB0 &&   // 1011xxxx
        (data[i + 5] & 0xC0) == 0x80) {   // 10xxxxxx
      // Extract the values of the high/low surrogates.
      uint16_t high_surrogate = ((data[i + 0] & 0x0F) << 12) |  // 1110xxxx
                                ((data[i + 1] & 0x3F) << 6) |   // 10xxxxxx
                                ((data[i + 2] & 0x3F));         // 10xxxxxx
      uint16_t low_surrogate = ((data[i + 3] & 0x0F) << 12) |   // 1110xxxx
                               ((data[i + 4] & 0x3F) << 6) |    // 10xxxxxx
                               ((data[i + 5] & 0x3F));          // 10xxxxxx

      // Extract the unicode value from the high/low surrogates. Given a
      // unicode point, the high/low surrogate pair can be obtained using these
      // steps:
      //  * 0x010000 is subtracted from the unicode code point.
      //  * The top ten bits are added to 0xD800 to give the high surrogate.
      //  * The low ten bits are added to 0xDC00 to give the low surrogate.
      // See https://en.wikipedia.org/wiki/UTF-16#U.2B10000_to_U.2B10FFFF for
      // details on how to convert Unicode points to UTF16 surrogates.
      //
      // Here, we apply the inverse of the above steps to recover the unicode
      // point from the high/low surrogates.
      uint16_t top_ten_bits = high_surrogate - 0xD800;
      uint16_t low_ten_bits = low_surrogate - 0xDC00;
      int32_t unicode_value = 0x10000 + ((top_ten_bits << 10) | low_ten_bits);
      supplementaries.push(std::pair<int, int32_t>(i, unicode_value));
      i += 5;  // Skip the next 5 bytes of the supplementary character.
      continue;
    }
  }

  // Nothing to replace. Zero copy exit path.
  if (supplementaries.empty())
    return;

  // Iterate from the first byte of the 'data' string towards the last byte, and
  // transform it in place. (Small optimization: We can skip transforming the
  // first N bytes as they will not change).
  int num_supplementaries_replaced = 0;
  for (int old_index = supplementaries.front().first;
       old_index + 5 < data.size();
       ++old_index) {
    // Each character shifts by a number of bytes proportional to the number of
    // supplementary characters replaced so far.
    int new_index = old_index + (4 - 6) * num_supplementaries_replaced;

    // Replace supplementary character in the input.
    if (!supplementaries.empty() &&
        old_index == supplementaries.front().first) {
      int32_t value = supplementaries.front().second;

      // Split the unicode point into its bit ranges that will be needed when
      // encoding it in UTF8.
      uint8_t bits_5_0 = value & 0x3F;
      uint8_t bits_11_6 = (value >> 6) & 0x3F;
      uint8_t bits_17_12 = (value >> 12) & 0x3F;
      uint8_t bits_20_18 = (value >> 18) & 0x07;

      // Encode the unicode point using a 4-byte UTF8 sequence. The standard
      // says:
      //    Byte 1   Byte 2   Byte 3   Byte 4
      //   11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
      // See https://en.wikipedia.org/wiki/UTF-8#Description for details.
      data[new_index + 0] = 0xF0 | bits_20_18;
      data[new_index + 1] = 0x80 | bits_17_12;
      data[new_index + 2] = 0x80 | bits_11_6;
      data[new_index + 3] = 0x80 | bits_5_0;
      supplementaries.pop();
      ++num_supplementaries_replaced;
      old_index += 5;  // Skip the next 5 bytes of the supplementary character.
      continue;
    }

    // Ordinary UTF8 character. Just copy to new location.
    data[new_index] = data[old_index];
  }

  // Shrink the 'data' string to its new size:
  //  * Each 6-byte supplementary character is replaced by 4-bytes.
  data.resize(data.size() + (4 - 6) * supplementaries.size());
}

// Converts Modified UTF8 Java string to a partially equivalent standard UTF8
// representation. The input must be a valid Modified UTF8 string.
//
// Modified UTF8 has two differences from standard UTF8:
//
//  1. Embedded zero characters:
//  Modified UTF8 allows embedded zeros, and represents them with a two
//  byte sequence: (0xC0, 0x80). We replace this two byte sequence with the 4
//  byte sequence "\\u0000". Our goal with this format is to enable the user to
//  identify these embedded zero characters from the UI, if needed.
//
//  2. Supplementary characters:
//  Supplementary characters are Unicode points in range U+10000 to U+10FFFF. In
//  Modified UTF8, these characters are first converted into UTF16 surrogate
//  pairs, and then the resulting two 16-bit numbers are encoded in UTF8,
//  thereby taking 6 bytes total. On the other hand, standard (non-Modified)
//  UTF8 directly encodes these supplementary characters, which takes 4 bytes.
//  We convert these characters from Modified UTF8 encoding into standard UTF8
//  encoding.
static void ScrubModifiedUtf8(std::string* data_ptr) {
  ScrubEmbeddedZeroCharacters(data_ptr);
  ScrubSupplementaryCharacters(data_ptr);
}

static std::unique_ptr<StatusMessageModel> FormatJavaString(
    const NamedJVariant& source, const ValueFormatter::Options& options,
    std::string* formatted_value) {
  jobject ref = nullptr;
  if (!source.value.get<jobject>(&ref)) {
    DCHECK(false);
    *formatted_value = "<unavailable>";
    return nullptr;
  }

  jstring jstr = static_cast<jstring>(ref);

  if (jstr == nullptr) {
    *formatted_value = kNull;
    return nullptr;
  }

  const jint full_len = jni()->GetStringLength(jstr);
  int len = full_len;
  if (len < 0) {
    LOG(ERROR) << "Bad string length: " << len;
    *formatted_value = "<malformed string>";
    return nullptr;
  }

  bool is_truncated = false;
  if (len > options.max_string_length) {
    len = options.max_string_length;
    is_truncated = true;
  }

  const char* suffix = nullptr;
  int suffix_length = 0;  // Does not include '\0'.
  if (options.quote_string) {
    if (is_truncated) {
      suffix = kTruncatedStringSuffix;
      suffix_length = arraysize(kTruncatedStringSuffix) - 1;
    } else {
      suffix = kNormalStringSuffix;
      suffix_length = arraysize(kNormalStringSuffix) - 1;
    }
  } else {
    if (is_truncated) {
      suffix = kTruncatedStringSuffixNoQuotes;
      suffix_length = arraysize(kTruncatedStringSuffixNoQuotes) - 1;
    } else {
      suffix = kNormalStringSuffixNoQuotes;
      suffix_length = arraysize(kNormalStringSuffixNoQuotes) - 1;
    }
  }

  // Allocate the string using 4x of the unicode string length.
  formatted_value->resize(4 * len + 1 + suffix_length);

  if (options.quote_string) {
    // Wrap the string with double quotes to give a clue that it's a string.
    (*formatted_value)[0] = '"';
  }

  // Throws StringIndexOutOfBoundsException on index overflow.
  jni()->GetStringUTFRegion(
      jstr,
      0,
      len,
      &((*formatted_value)[options.quote_string ? 1 : 0]));
  JniCheckNoException("GetStringUTFRegion");

  // Java uses Modified UTF8. We must convert it into strict UTF8.
  ScrubModifiedUtf8(formatted_value);

  // Find the end of the string.
  len = strlen(&((*formatted_value)[0]));

  // Copy the suffix onto the allocated space and resize back to the full size.
  std::copy(suffix, suffix + suffix_length, formatted_value->begin() + len);
  formatted_value->resize(len + suffix_length);

  if (!is_truncated) {
    return nullptr;
  }

  // If strings are truncated, we report this through a status message.
  // We can detect watch expressions string by checking
  // options.max_string_length that is set to kExtendedMaxStringLength
  // in CaptureDataCollector::FormatVariable().
  bool is_watch_expression =
      (options.max_string_length == kExtendedMaxStringLength);
  return StatusMessageBuilder()
          .set_info()
          .set_refers_to(StatusMessageModel::Context::VARIABLE_VALUE)
          .set_description({
            is_watch_expression ?
                FormatTrimmedExpressionString : FormatTrimmedLocalString,
            { std::to_string(full_len) }})
          .build();
}

bool ValueFormatter::IsImmutableValueObject(
    WellKnownJClass well_known_jclass) {
  return (well_known_jclass == WellKnownJClass::String);
}


bool ValueFormatter::IsValue(const NamedJVariant& data) {
  // Consider unavailable data as value since it's definitely not a reference.
  if (data.status.is_error) {
    return true;
  }

  // Primitive types are values.
  if (data.value.type() != JType::Object) {
    return true;
  }

  // Java string is immutable we treat it as a value type for formatting
  // purposes. If the referenced object is null, the whole thing is still
  // a value.
  if (IsJavaString(data)) {
    return true;
  }

  // "null" is also a value, since it can't be explored any further.
  return !data.value.has_non_null_object();
}


int ValueFormatter::GetTotalDataSize(const NamedJVariant& data) {
  const int name_size = data.name.size();

  // Include size of error message if evaluation failed.
  if (!data.status.description.format.empty()) {
    return name_size + data.status.description.format.size() +
           std::accumulate(
               data.status.description.parameters.begin(),
               data.status.description.parameters.end(), 0,
               [](int accumulated_total, const std::string& parameter) {
                 return accumulated_total + parameter.size();
               });
  }

  // Compute length of a string.
  if (!IsJavaString(data)) {
    return name_size + 8;  // 8 characters is good enough approximation.
  }

  jobject ref = nullptr;
  if (data.value.get<jobject>(&ref) && (ref != nullptr)) {
    // 2 characters for the wrapping double quotes + number of characters to
    // take from the Java string.
    //
    // We don't account for the fact that Options::max_string_length might
    // be larger than kDefaultMaxStringLength. We can live with it since
    // the longer limit is only used in a handful of places and the extra
    // length has a very small impact on the total size of the captured buffer.
    return name_size +
           2 +
           std::min<int>(
               kDefaultMaxStringLength,
               jni()->GetStringLength(static_cast<jstring>(ref)));
  }

  return name_size + 4;
}

std::unique_ptr<StatusMessageModel> ValueFormatter::Format(
    const NamedJVariant& source, const Options& options,
    std::string* formatted_value, std::string* type) {
  // Format Java string.
  if (IsJavaString(source)) {
    std::unique_ptr<StatusMessageModel> status_message =
        FormatJavaString(source, options, formatted_value);
    if (type != nullptr) {
      if (source.value.has_non_null_object()) {
        *type = "String";
      } else {
        type->clear();
      }
    }
    return status_message;
  }

  // Format primitive value (or null).
  *formatted_value = source.value.ToString(true);
  if (type != nullptr) {
    if (source.value.type() != JType::Object) {
      *type = TypeNameFromSignature({ source.value.type() });
    } else {  // "source" represents Java "null", which doesn't have a type.
      type->clear();
    }
  }
  return nullptr;
}

}  // namespace cdbg
}  // namespace devtools
