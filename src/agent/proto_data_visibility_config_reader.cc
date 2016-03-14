#include "proto_data_visibility_config_reader.h"

#include <set>
#include "java/com/google/devtools/cdbg/annotations/visibility.proto.h"
#include "net/proto2/public/text_format.h"

namespace devtools {
namespace cdbg {

using proto2::TextFormat;

static constexpr char kResourcePath[] =
    "META-INF/metadata/cloud-debugger-invisible.gcl";

// Imports proto-defined data visibility configuration of a method into
// an instance of "DataVisibilityPolicy::Config".
static void ImportMethod(
    const devtools::cdbg::configuration::Method& proto_method,
    FileDataVisibilityPolicy::Config::Method* method) {
  method->name = proto_method.name();
  method->signature = proto_method.signature();

  method->variables.resize(proto_method.variables_size());
  for (int i = 0; i < proto_method.variables_size(); ++i) {
    method->variables[i].name = proto_method.variables(i).name();
    method->variables[i].invisible = proto_method.variables(i).invisible();
  }
}


// Merges proto-defined data visibility configuration of a tree of classes
// into an instance of "DataVisibilityPolicy::Config".
static void MergeClass(
    const devtools::cdbg::configuration::Class& proto_class,
    FileDataVisibilityPolicy::Config::Class* config) {
  // "invisible" field.
  config->invisible = config->invisible || proto_class.invisible();

  // "fields" field.
  for (const auto& proto_field : proto_class.fields()) {
    config->fields.resize(config->fields.size() + 1);
    config->fields.back().name = proto_field.name();
    config->fields.back().invisible = proto_field.invisible();
  }

  // "methods" field.
  for (const auto& proto_method : proto_class.methods()) {
    config->methods.resize(config->fields.size() + 1);
    ImportMethod(proto_method, &config->methods.back());
  }

  // "nested_classes" field.
  for (const auto& proto_nested_class : proto_class.nested_classes()) {
    MergeClass(
        proto_nested_class,
        &config->nested_classes[proto_nested_class.name()]);
  }
}


// Merges proto-defined data visibility configuration of a package into
// an instance of "DataVisibilityPolicy::Config". Theoretically different
// classes within the same package may reside in different .JAR files, so
// we have to actually merge the configuration we assembled so far with the
// data about the specified package.
static void MergePackage(
    const devtools::cdbg::configuration::Package& proto_package,
    FileDataVisibilityPolicy::Config* config) {
  // Convert binary name to internal name by replacing '.' to '/'.
  string internal_name = proto_package.binary_name();
  std::replace(internal_name.begin(), internal_name.end(), '.', '/');

  auto& package = config->packages[internal_name];

  // "invisible" field.
  package.invisible = package.invisible || proto_package.invisible();

  // "classes" field.
  for (const auto& proto_class : proto_package.classes()) {
    MergeClass(proto_class, &package.classes[proto_class.name()]);
  }
}


FileDataVisibilityPolicy::Config ReadProtoDataVisibilityConfiguration(
    ClassPathLookup* class_path_lookup) {
  FileDataVisibilityPolicy::Config config;

  std::set<string> files =
      class_path_lookup->ReadApplicationResource(kResourcePath);
  for (const string& proto_ascii : files) {
    devtools::cdbg::configuration::Root root;
    if (!TextFormat::ParseFromString(proto_ascii, &root)) {
      LOG(WARNING) << "Failed to parse data visibility configuration file";
      continue;
    }

    for (const auto& package : root.packages()) {
      MergePackage(package, &config);
    }
  }

  return config;
}

}  // namespace cdbg
}  // namespace devtools
