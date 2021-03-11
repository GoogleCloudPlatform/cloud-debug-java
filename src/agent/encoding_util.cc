#include "encoding_util.h"

#include <cstdint>

namespace devtools {
namespace cdbg {

std::string Base64Encode(const char* in, size_t in_size) {
  static const char kBase64Chars[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  static const char kBase64PadChar = '=';

  int in_i = 0;
  int out_i = 0;
  size_t out_size = ((in_size + 2) / 3) * 4;
  std::string out(out_size, 0);

  while (in_i + 2 < in_size) {
    uint32_t in_bytes = in[in_i] << 16 | in[in_i + 1] << 8 | in[in_i + 2];
    out[out_i++] = kBase64Chars[in_bytes >> 18];
    out[out_i++] = kBase64Chars[(in_bytes >> 12) & 0x3F];
    out[out_i++] = kBase64Chars[(in_bytes >> 6) & 0x3F];
    out[out_i++] = kBase64Chars[in_bytes & 0x3F];
    in_i += 3;
  }

  if (in_i + 1 == in_size) {
    uint32_t in_bytes = in[in_i] << 16;
    out[out_i++] = kBase64Chars[in_bytes >> 18];
    out[out_i++] = kBase64Chars[(in_bytes >> 12) & 0x3F];
    out[out_i++] = kBase64PadChar;
    out[out_i++] = kBase64PadChar;
  } else if (in_i + 2 == in_size) {
    uint32_t in_bytes = in[in_i] << 16 | in[in_i + 1] << 8;
    out[out_i++] = kBase64Chars[in_bytes >> 18];
    out[out_i++] = kBase64Chars[(in_bytes >> 12) & 0x3F];
    out[out_i++] = kBase64Chars[(in_bytes >> 6) & 0x3F];
    out[out_i++] = kBase64PadChar;
  }

  return out;
}

int ValidateUtf8(const char* in, size_t in_size) {
  int valid_bytes_read = 0;
  uint32_t code_point = 0;
  int code_point_length_remaining = 0;
  for (int i = 0; i < in_size; i++) {
    char cur = in[i];
    if (code_point_length_remaining) {
      if ((cur & 0xC0) != 0x80) {
        // Continuation byte not of the form 0b10XXXXXX.
        return valid_bytes_read;
      }

      code_point = (code_point << 6) | (cur & 0x3F);
      code_point_length_remaining--;
      if (code_point_length_remaining == 0) {
        int code_point_length = i + 1 - valid_bytes_read;
        if ((code_point_length == 2 && code_point < 0x80) ||
            (code_point_length == 3 && code_point < 0x800) ||
            (code_point_length == 4 && code_point < 0x10000)) {
          // https://en.wikipedia.org/wiki/UTF-8#Overlong_encodings
          return valid_bytes_read;
        }

        if ((code_point >= 0xD800 && code_point <= 0xDFFF) ||
            code_point > 0x10FFFF) {
          // https://en.wikipedia.org/wiki/UTF-8#Invalid_code_points
          return valid_bytes_read;
        }

        valid_bytes_read = i + 1;
        code_point = 0;
      }
    } else if (!(cur & 0x80)) {
      valid_bytes_read = i + 1;
    } else if ((cur & 0xE0) == 0xC0) {
      code_point = cur & 0x1F;
      code_point_length_remaining = 1;
    } else if ((cur & 0xF0) == 0xE0) {
      code_point = cur & 0x0F;
      code_point_length_remaining = 2;
    } else if ((cur & 0xF8) == 0xf0) {
      code_point = cur & 0x07;
      code_point_length_remaining = 3;
    } else {
      // Unexpected continuation byte or invalid leadoff byte.
      return valid_bytes_read;
    }
  }

  return valid_bytes_read;
}

}  // namespace cdbg
}  // namespace devtools
