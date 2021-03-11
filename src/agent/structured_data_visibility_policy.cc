#include "structured_data_visibility_policy.h"

#include <cstdint>
#include <sstream>

#include "jni_utils.h"

namespace devtools {
namespace cdbg {

namespace {

class ClassImpl : public DataVisibilityPolicy::Class {
 public:
  // Does not take ownership over "class_config", which must outlive this
  // instance.
  ClassImpl(
      bool parent_invisible,
      const StructuredDataVisibilityPolicy::Config::Class* class_config)
      : class_invisible_(parent_invisible || class_config->invisible),
        class_config_(class_config) {
  }

  bool IsFieldVisible(const std::string& name,
                      int32_t field_modifiers) override {
    if (class_invisible_) {
      return false;
    }

    // We assume that the number of fields annotated as "InvisibleForDebugging"
    // will be typically small enough in each class (less than 5). In this case
    // iterating through a vector is more efficient than a map.
    for (const auto& field : class_config_->fields) {
      if (field.name == name) {
        return !field.invisible;
      }
    }

    // Fields not explicitly mentioned in the configuration are visible
    // by default.
    return true;
  }

  bool IsFieldDataVisible(const std::string& name, int32_t field_modifiers,
                          std::string* reason) override {
    return true;
  }

  bool IsMethodVisible(const std::string& method_name,
                       const std::string& method_signature,
                       int32_t method_modifiers) override {
    if (class_invisible_) {
      return false;
    }

    // This functionality is not yet exposed through annotations.
    return true;
  }

  bool IsVariableVisible(const std::string& method_name,
                         const std::string& method_signature,
                         const std::string& variable_name) override {
    if (class_invisible_) {
      return false;
    }

    // We are linearly scanning all the methods that have variables annotated as
    // "InvisibleForDebugging". If this annotation becomes popular, this might
    // not be good enough.
    for (const auto& method : class_config_->methods) {
      if ((method.name == method_name) &&
          (method.signature == method_signature)) {
        // We assume that the number of local variables and arguments annotated
        // as "InvisibleForDebugging" is small enough (less than 5). In this
        // case iterating through a vector is more efficient than a map.
        for (const auto& variable : method.variables) {
          if (variable.name == variable_name) {
            return !variable.invisible;
          }
        }

        break;
      }
    }

    // Variables not explicitly mentioned in the configuration are visible
    // by default.
    return true;
  }

  bool IsVariableDataVisible(const std::string& method_name,
                             const std::string& method_signature,
                             const std::string& variable_name,
                             std::string* reason) override {
    return true;
  }

 private:
  // True if one of the parent classes or the parent package is marked
  // as debugger invisible. This effectively makes this class debugger
  // invisible as well.
  const bool class_invisible_;

  // Visibility configuration of a single Java class. Not owned by this class.
  const StructuredDataVisibilityPolicy::Config::Class* const class_config_;

  DISALLOW_COPY_AND_ASSIGN(ClassImpl);
};

}  // namespace

const StructuredDataVisibilityPolicy::Config::Class g_default_class_config = {};


std::unique_ptr<DataVisibilityPolicy::Class>
StructuredDataVisibilityPolicy::GetClassVisibility(jclass cls) {
  std::string signature = GetClassSignature(cls);
  if ((signature.size() < 3) ||
      (signature.front() != 'L') || (signature.back() != ';')) {
    return nullptr;  // Invalid class signature.
  }

  size_t package_sep_pos = signature.find_last_of('/');
  size_t class_name_pos = package_sep_pos + 1;
  if (package_sep_pos == std::string::npos) {
    package_sep_pos = 1;  // Top level class (i.e. package name is empty).
    class_name_pos = 1;
  }

  std::string package_name = signature.substr(1, package_sep_pos - 1);
  auto it_package = config_.packages.find(package_name);
  if (it_package == config_.packages.end()) {
    return nullptr;  // No configuration in this package.
  }

  bool parent_invisible = it_package->second.invisible;

  std::stringstream ss(signature.substr(
      class_name_pos,
      signature.size() - class_name_pos - 1));  // Skip over the trailing ';'.
  std::string class_name;
  const auto* parent_map = &it_package->second.classes;
  const Config::Class* class_config = nullptr;
  while (std::getline(ss, class_name, '$')) {
    if (class_config != nullptr) {
      parent_invisible = parent_invisible || class_config->invisible;
    }

    auto it = parent_map->find(class_name);
    if (it == parent_map->end()) {
      if (!parent_invisible) {
        return nullptr;  // No configuration for this class.
      }

      class_config = &g_default_class_config;
      break;
    }

    class_config = &it->second;
    parent_map = &class_config->nested_classes;
  }

  // This should never happen, but just in case...
  if (class_config == nullptr) {
    return nullptr;
  }

  return std::unique_ptr<Class>(new ClassImpl(parent_invisible, class_config));
}

}  // namespace cdbg
}  // namespace devtools
