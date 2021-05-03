#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_YAML_DATA_VISIBILITY_CONFIG_READER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_YAML_DATA_VISIBILITY_CONFIG_READER_H_

#include "class_path_lookup.h"
#include "common.h"
#include "glob_data_visibility_policy.h"

namespace devtools {
namespace cdbg {

// Loads .yaml visibility configuration.
GlobDataVisibilityPolicy::Config ReadYamlDataVisibilityConfiguration(
    ClassPathLookup* class_path_lookup, std::string* yaml_source);

}  // namespace cdbg
}  // namespace devtools


#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_YAML_DATA_VISIBILITY_CONFIG_READER_H_
