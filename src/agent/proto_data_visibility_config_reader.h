#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_PROTO_DATA_VISIBILITY_CONFIG_READER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_PROTO_DATA_VISIBILITY_CONFIG_READER_H_

#include "class_path_lookup.h"
#include "common.h"
#include "file_data_visibility_policy.h"

namespace devtools {
namespace cdbg {

// Loads application specific data visibility configuration from .JAR files.
// The application doesn't have to use "InvisibleForDebugging" annotations.
// In such cases this function will return an empty configuration.
FileDataVisibilityPolicy::Config ReadProtoDataVisibilityConfiguration(
    ClassPathLookup* class_path_lookup);

}  // namespace cdbg
}  // namespace devtools


#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_PROTO_DATA_VISIBILITY_CONFIG_READER_H_
