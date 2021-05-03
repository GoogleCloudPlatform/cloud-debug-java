#include "yaml_data_visibility_config_reader.h"

#include "debuggee_labels.h"
#include "jni_proxy_yamlconfigparser.h"
#include "jni_utils.h"

namespace devtools {
namespace cdbg {

// Config file to search for via ClassLookupPath.readApplicationResource, which
// is currently documented in ClassPathLookup.java
static constexpr char kResourcePath[] = "debugger-blocklist.yaml";
static constexpr char kResourcePathDeprecated[] = "debugger-blacklist.yaml";

// Reads the debugger yaml config.
//
// Returns true (success) if:
//   - Data for one file was found.  Sets config to file contents
//   - No config file is found. Sets config to ""
// Returns false (error) if there were multiple configurations found.
static bool ReadYamlConfig(ClassPathLookup* class_path_lookup,
                           std::string* config_file_name,
                           std::string* blocklist_source, std::string* config,
                           std::string* error) {
  std::set<std::string> files =
      class_path_lookup->ReadApplicationResource(kResourcePath);

  if (!files.empty()) {
    *config_file_name = kResourcePath;
    *blocklist_source = DebuggeeLabels::kBlocklistSourceFile;
  }

  if (files.size() > 1) {
    LOG(ERROR) << "Multiple " << kResourcePath << " files found."
               << "  Found " << files.size() << " files.";
    // TODO Move to messages.h
    *error =
        "Multiple debugger-blocklist.yaml files found in the search path. "
        "Please contact your system administrator.";
    return false;
  }

  // TODO: Finalize the conversion to blocklist, this block can be
  // removed.
  if (files.empty()) {
    files = class_path_lookup->ReadApplicationResource(kResourcePathDeprecated);

    if (!files.empty()) {
      *config_file_name = kResourcePathDeprecated;
      *blocklist_source = DebuggeeLabels::kBlocklistSourceDeprecatedFile;
      LOG(WARNING) << "The use of debugger-blacklist.yaml has been deprecated, "
                      "please use debugger-blocklist instead";
    }

    if (files.size() > 1) {
      LOG(ERROR) << "Multiple " << kResourcePathDeprecated << " files found."
                 << "  Found " << files.size() << " files.";
      *error =
          "Multiple debugger-blacklist.yaml files found in the search path. "
          "Please contact your system administrator.";
      return false;
    }
  }

  if (files.empty()) {
    // No configuration file was provided
    LOG(INFO) << kResourcePath << " was not found.  Using default settings.";
    *config = "";
    *blocklist_source = DebuggeeLabels::kBlocklistSourceNone;
  } else {
    *config = *files.begin();
  }

  return true;
}

// Parses the string yaml_config that contains a yaml configuration.
// Adds data to config.
//
// Does not alter config if a parsing error occurs.
//
// Returns true if parse was successful, false otherwise.
static bool ParseYamlConfig(const std::string& yaml_config,
                            const std::string& config_file_name,
                            GlobDataVisibilityPolicy::Config* config,
                            std::string* error) {
  // Gather all needed data here.  Do not alter config until all data has been
  // collected without error.
  ExceptionOr<JniLocalRef> config_parser =
      jniproxy::YamlConfigParser()->NewObject(yaml_config);
  if (config_parser.HasException()) {
    LOG(ERROR) << "Exception creating YAML config parser object: "
               << FormatException(config_parser.GetException());
    *error = "Errors parsing " + config_file_name +
             ". Please contact your system administrator.";
    return false;
  }

  ExceptionOr<JniLocalRef> blocklist_patterns =
      jniproxy::YamlConfigParser()->getBlocklistPatterns(
          config_parser.GetData().get());

  if (blocklist_patterns.HasException()) {
    LOG(ERROR) << "Exception getting blocklist patterns: "
               << FormatException(blocklist_patterns.GetException());
    *error =
        "Error building blocklist patterns. "
        "Please contact your system administrator.";
    return false;
  }

  ExceptionOr<JniLocalRef> blocklist_exception_patterns =
      jniproxy::YamlConfigParser()->getBlocklistExceptionPatterns(
          config_parser.GetData().get());

  if (blocklist_exception_patterns.HasException()) {
    LOG(ERROR) << "Exception getting blocklist exception patterns: "
               << FormatException(blocklist_exception_patterns.GetException());
    *error =
        "Error building blocklist exception patterns. "
        "Please contact your system administrator.";
    return false;
  }

  // The code below, which does change the config, should have no error paths.
  // Otherwise we might leave the caller with a partially-modified
  // configuration.
  std::vector<std::string> blocklist_patterns_cpp =
      JniToNativeStringArray(blocklist_patterns.GetData().get());

  for (const std::string& glob_pattern : blocklist_patterns_cpp) {
    config->blocklists.Add(glob_pattern);
  }

  std::vector<std::string> blocklist_exception_patterns_cpp =
      JniToNativeStringArray(blocklist_exception_patterns.GetData().get());

  for (const std::string& glob_pattern : blocklist_exception_patterns_cpp) {
    config->blocklist_exceptions.Add(glob_pattern);
  }

  return true;
}

GlobDataVisibilityPolicy::Config ReadYamlDataVisibilityConfiguration(
    ClassPathLookup* class_path_lookup, std::string* blocklist_source) {
  GlobDataVisibilityPolicy::Config config;

  std::string yaml_config;
  std::string config_file_name;
  std::string error;
  if (!ReadYamlConfig(class_path_lookup, &config_file_name,
                      blocklist_source, &yaml_config, &error)) {
    config.parse_error = error;
    return config;
  }

  if (!yaml_config.empty()) {
    if (!ParseYamlConfig(yaml_config, config_file_name, &config, &error)) {
      config.parse_error = error;
      return config;
    }
  }

  // Prepare both blocklists and blocklist_exceptions for lookup processing.
  config.blocklists.Prepare();
  config.blocklist_exceptions.Prepare();
  return config;
}

}  // namespace cdbg
}  // namespace devtools
