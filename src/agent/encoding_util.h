#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_ENCODING_UTIL_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_ENCODING_UTIL_H_

#include "common.h"

namespace devtools {
namespace cdbg {

// Encodes buffer into a base 64 string.
std::string Base64Encode(const char* in, size_t in_size);

// Checks whether a buffer is valid utf8. The return value is the number of
// valid utf8 bytes read from the beginning of the buffer.
int ValidateUtf8(const char* in, size_t in_size);

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_ENCODING_UTIL_H_
