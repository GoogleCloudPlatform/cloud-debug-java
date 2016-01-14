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

#include <numeric>
#include "jni_utils.h"

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


static void FormatJavaString(
    const NamedJVariant& source,
    const ValueFormatter::Options& options,
    string* formatted_value) {
  jobject ref = nullptr;
  if (!source.value.get<jobject>(&ref)) {
    DCHECK(false);
    *formatted_value = "<unavailable>";
    return;
  }

  jstring jstr = static_cast<jstring>(ref);

  if (jstr == nullptr) {
    *formatted_value = kNull;
    return;
  }

  jint len = jni()->GetStringLength(jstr);
  if (len < 0) {
    LOG(ERROR) << "Bad string length: " << len;
    *formatted_value = "<malformed string>";
    return;
  }

  bool truncated = false;
  if (len > options.max_string_length) {
    len = options.max_string_length;
    truncated = true;
  }

  const char* suffix = nullptr;
  int suffix_length = 0;  // Does not include '\0'.
  if (options.quote_string) {
    if (truncated) {
      suffix = kTruncatedStringSuffix;
      suffix_length = arraysize(kTruncatedStringSuffix) - 1;
    } else {
      suffix = kNormalStringSuffix;
      suffix_length = arraysize(kNormalStringSuffix) - 1;
    }
  } else {
    if (truncated) {
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

  // Find the end of the string. Java strings can include 0, but modified UTF-8
  // encoding keeps it as two non-zero bytes:
  // http://docs.oracle.com/javase/1.5.0/docs/guide/jni/spec/types.html
  len = strlen(&((*formatted_value)[0]));

  // Copy the suffix onto the allocated space and resize back to the full size.
  std::copy(suffix, suffix + suffix_length, formatted_value->begin() + len);
  formatted_value->resize(len + suffix_length);
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
    return
      name_size +
      data.status.description.format.size() +
      std::accumulate(
          data.status.description.parameters.begin(),
          data.status.description.parameters.end(),
          0,
          [] (int accumulated_total, const string& parameter) {
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


void ValueFormatter::Format(
    const NamedJVariant& source,
    const Options& options,
    string* formatted_value,
    string* type) {
  // Format Java string.
  if (IsJavaString(source)) {
    FormatJavaString(source, options, formatted_value);
    if (type != nullptr) {
      if (source.value.has_non_null_object()) {
        *type = "String";
      } else {
        type->clear();
      }
    }
    return;
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
}


}  // namespace cdbg
}  // namespace devtools
