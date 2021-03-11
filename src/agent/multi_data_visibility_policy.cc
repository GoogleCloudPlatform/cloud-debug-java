#include "multi_data_visibility_policy.h"

#include <cstdint>

namespace devtools {
namespace cdbg {

namespace {

// Implementation of DataVisibilityPolicy::Class that is intended to hold
// two or more DataVisibilityPolicy::Class child objects and return a
// combination of thier results.
//
// The logic also supports the zero and one child case logically, but adds
// some ineffieincies in regards to memory use and (slightly) cpu time.
class ClassImpl : public DataVisibilityPolicy::Class {
 public:
  bool IsFieldVisible(const std::string& name,
                      int32_t field_modifiers) override {
    for (const auto& policy : class_list_) {
      if (!policy->IsFieldVisible(name, field_modifiers)) {
        return false;
      }
    }

    return true;
  }

  bool IsFieldDataVisible(const std::string& name, int32_t field_modifiers,
                          std::string* reason) override {
    for (const auto& policy : class_list_) {
      if (!policy->IsFieldDataVisible(name, field_modifiers, reason)) {
        return false;
      }
    }

    return true;
  }

  bool IsMethodVisible(const std::string& method_name,
                       const std::string& method_signature,
                       int32_t method_modifiers) override {
    for (const auto& policy : class_list_) {
      if (!policy->IsMethodVisible(
          method_name,
          method_signature,
          method_modifiers)) {
        return false;
      }
    }

    return true;
  }

  bool IsVariableVisible(const std::string& method_name,
                         const std::string& method_signature,
                         const std::string& variable_name) override {
    for (const auto& policy : class_list_) {
      if (!policy->IsVariableVisible(
          method_name,
          method_signature,
          variable_name)) {
        return false;
      }
    }

    return true;
  }

  bool IsVariableDataVisible(const std::string& method_name,
                             const std::string& method_signature,
                             const std::string& variable_name,
                             std::string* reason) override {
    for (const auto& policy : class_list_) {
      if (!policy->IsVariableDataVisible(
          method_name,
          method_signature,
          variable_name,
          reason)) {
        return false;
      }
    }

    return true;
  }

  // A list of class policies to consider.  Made public to simplify the
  // GetClassVisibility() logic below (e.g. avoids additional logic that
  // creates a separate vector and moves it's data into this class).
  std::vector<std::unique_ptr<DataVisibilityPolicy::Class>> class_list_;
};

}  // namespace


MultiDataVisibilityPolicy::MultiDataVisibilityPolicy(
    std::unique_ptr<DataVisibilityPolicy> policy1,
    std::unique_ptr<DataVisibilityPolicy> policy2) {
  policy_list_.push_back(std::move(policy1));
  policy_list_.push_back(std::move(policy2));
}


MultiDataVisibilityPolicy::MultiDataVisibilityPolicy(
    std::unique_ptr<DataVisibilityPolicy> policy1,
    std::unique_ptr<DataVisibilityPolicy> policy2,
    std::unique_ptr<DataVisibilityPolicy> policy3) {
  policy_list_.push_back(std::move(policy1));
  policy_list_.push_back(std::move(policy2));
  policy_list_.push_back(std::move(policy3));
}


std::unique_ptr<DataVisibilityPolicy::Class>
MultiDataVisibilityPolicy::GetClassVisibility(jclass cls) {
  // A few common cases avoid the need to store the ClassImpl wrapper, saving
  // time and memory.
  //
  // 1) If every policy returns nullptr, this can return nullptr too - no need
  //    to create a ClassImpl wrapper.  This is a *very common case*.
  // 2) If everyone but one policy returns nullptr, the non-null
  //    unique_ptr can be returned directly without the need for a ClassImpl
  //    wrapper
  std::unique_ptr<ClassImpl> class_impl(new ClassImpl());

  for (const auto& policy : policy_list_) {
    std::unique_ptr<DataVisibilityPolicy::Class> policy_class =
        policy->GetClassVisibility(cls);

    if (policy_class != nullptr) {
      class_impl->class_list_.push_back(std::move(policy_class));
    }
  }

  if (class_impl->class_list_.empty()) {
    // No active policies, this class is always whitelisted.
    return nullptr;
  }

  if (class_impl->class_list_.size() == 1) {
    // Only one policy, no need to wrap it.
    return std::move(class_impl->class_list_[0]);
  }

  // Multiple policies need to be considered
  return std::move(class_impl);
}

bool MultiDataVisibilityPolicy::HasSetupError(std::string* error) const {
  for (const auto& policy : policy_list_) {
    if (policy->HasSetupError(error)) {
      return true;
    }
  }

  return false;
}

}  // namespace cdbg
}  // namespace devtools
