#include "glob_data_visibility_policy.h"

#include <fnmatch.h>

#include <algorithm>
#include <cstdint>

#include "type_util.h"

namespace devtools {
namespace cdbg {

static constexpr char kReasonIsBlocked[] = "blocked by admin";

namespace {

// A simple implementation of DataVisibilityPolicy::Class which always
// reports that methods and fields have their data hidden.
class BlockedClassImpl : public DataVisibilityPolicy::Class {
 public:
  explicit BlockedClassImpl(const std::string& reason) : reason_(reason) {}

  bool IsFieldVisible(const std::string& name,
                      int32_t field_modifiers) override {
    return true;
  }

  bool IsFieldDataVisible(const std::string& name, int32_t field_modifiers,
                          std::string* reason) override {
    *reason = reason_;
    return false;
  }

  bool IsMethodVisible(const std::string& method_name,
                       const std::string& method_signature,
                       int32_t method_modifiers) override {
    return false;
  }

  bool IsVariableVisible(const std::string& method_name,
                         const std::string& method_signature,
                         const std::string& variable_name) override {
    return true;
  }

  bool IsVariableDataVisible(const std::string& method_name,
                             const std::string& method_signature,
                             const std::string& variable_name,
                             std::string* reason) override {
    *reason = reason_;
    return false;
  }

 private:
  const std::string reason_;
};

}  // namespace

GlobDataVisibilityPolicy::GlobDataVisibilityPolicy() {
  config_.parse_error = "Internal Error: visibility policy not initialized.";
}

bool GlobDataVisibilityPolicy::HasSetupError(std::string* error) const {
  if (config_.parse_error.empty()) {
    return false;
  }

  *error = config_.parse_error;
  return true;
}

std::unique_ptr<DataVisibilityPolicy::Class>
GlobDataVisibilityPolicy::GetClassVisibility(jclass cls) {
  if (!config_.parse_error.empty()) {
    // There was a parsing error while trying to load the debugger config.
    return std::unique_ptr<Class>(
        new BlockedClassImpl(config_.parse_error));
  }

  std::string signature = GetClassSignature(cls);
  if ((signature.size() < 3) || (signature.front() != 'L') ||
      (signature.back() != ';')) {
    return nullptr;  // Invalid class signature.
  }

  std::string path = TypeNameFromJObjectSignature(signature);
  // replace $ with . in paths.  Without this replacement, someone
  // could try and blocklist all class members with a rule like
  //
  // com.foo.MyClass.*
  //
  // but miss inner classes
  //
  // com.foo.MyClass$Inner (visible)
  std::replace(path.begin(), path.end(), '$', '.');

  // If this class matches an exception, it can not be blocked
  if (config_.blocklist_exceptions.Matches(path)) {
    return nullptr;
  }

  // Blocklist this class if it matches a pattern
  if (config_.blocklists.Matches(path)) {
    return std::unique_ptr<Class>(
        new BlockedClassImpl(kReasonIsBlocked));
  }

  // Nothing was matched
  return nullptr;
}

// Returns true if path can be matched by the wildcard pattern.
static bool WildcardMatches(const std::string& path,
                            const std::string& pattern) {
  constexpr int kFlags = 0;
  return fnmatch(pattern.c_str(), path.c_str(), kFlags) == 0;
}

// GenericMatches works in all cases but uses a slow O(n) algorithm.  Due to
// it's performance, it's intended to process the generic_patterns_ set.
static bool GenericMatches(const std::string& path,
                           const std::set<std::string>& generic_match) {
  for (const std::string& pattern : generic_match) {
    if (WildcardMatches(path, pattern)) {
      return true;
    }
  }
  return false;
}

// Returns true if a path does not match anything is exact_inverse_patterns or
// inverse_patterns.
static bool InverseMatches(const std::string& path,
                           const std::set<std::string>& exact_inverse_patterns,
                           const std::set<std::string>& inverse_patterns) {
  if (inverse_patterns.empty() && exact_inverse_patterns.empty()) {
    // inverse matches are not being used
    return false;
  }

  return exact_inverse_patterns.find(path) == exact_inverse_patterns.end() &&
         !GenericMatches(path, inverse_patterns);
}

// PrefixMatches uses a fast O(log n) algorithm to match globs that
// end with *.
//
// path is the string to match
//
// prefix_match is a sorted set of prefixes with all redundant aliases
// removed (i.e. with RemoveRedundantPrefixes).  Each pattern is implicitly
// assumed to end with a *.  There should not be any '*' characters in the
// actual data.
static bool PrefixMatches(const std::string& path,
                          const std::vector<std::string>& prefix_match) {
  // Find the lower bound, but only consider the matching prefixes in
  // each comparison
  auto lower_bound = std::lower_bound(
      prefix_match.begin(), prefix_match.end(), path,
      [](const std::string& prefix, const std::string& path) {
        const size_t min_size = std::min(prefix.length(), path.length());
        return path.compare(0, min_size, prefix) > 0;
      });

  // No lower bound was found, path must be lower than all existing prefixes
  if (lower_bound == prefix_match.end()) {
    return false;
  }

  // Return true only if the found lower bound actually is a prefix of path
  return std::equal(lower_bound->begin(), lower_bound->end(), path.begin());
}

// Removes redundant prefixes in place.  Removing these is necessary for
// PrefixMatches() to function correctly.
//
// The prefixes vector must be sorted before calling this function.
//
// For example, if searching for AB inside [A AA B], the AA redundant prefix
// would incorrectly pivot the search toward B.  Removing the redundant
// prefix resolves the issue.
static void RemoveRedundantPrefixes(std::vector<std::string>* prefixes) {
  if (prefixes->size() <= 1) {
    // Nothing to do
    return;
  }
  int base = 0;
  for (int compare = 1; compare < prefixes->size(); ++compare) {
    const bool keep_prefix =
        !std::equal(prefixes->at(base).begin(), prefixes->at(base).end(),
                    prefixes->at(compare).begin());

    if (keep_prefix) {
      // prefix becomes the new base
      ++base;
      // if compare and base are not together, the string
      // needs to be moved back
      if (compare > base) {
        prefixes->at(base) = std::move(prefixes->at(compare));
      }
    }
    // else keep base where it is and skip this prefix.
    // The next keep_prefix will overwrite it
  }
  prefixes->resize(base + 1);
}

void GlobDataVisibilityPolicy::GlobSet::Prepare() {
  std::sort(prefix_patterns_.begin(), prefix_patterns_.end());
  RemoveRedundantPrefixes(&prefix_patterns_);
  prepared_ = true;
}

bool GlobDataVisibilityPolicy::GlobSet::Matches(const std::string& path) const {
  DCHECK(prepared_) << "Match() called before Prepare()";
  if (exact_patterns_.find(path) != exact_patterns_.end()) {
    return true;
  }

  if (PrefixMatches(path, prefix_patterns_)) {
    return true;
  }

  if (InverseMatches(path, exact_inverse_patterns_, inverse_patterns_)) {
    return true;
  }

  return GenericMatches(path, generic_patterns_);
}

void GlobDataVisibilityPolicy::GlobSet::Add(const std::string& glob_pattern) {
  prepared_ = false;

  if (glob_pattern.empty()) {
    return;
  }

  const size_t index = glob_pattern.find('*');

  // Check for an inverted pattern (patterns that start with a !)
  if (glob_pattern[0] == '!') {
    // Drop the leading ! character
    const std::string inverse_pattern = glob_pattern.substr(1);

    if (index == std::string::npos) {
      // For exact match, also add the path extension.  This allows a user
      // to say
      //
      // !com.package
      //
      // without the * and refer to an entire package path.
      exact_inverse_patterns_.insert(inverse_pattern);
      inverse_patterns_.insert(inverse_pattern + ".*");
    } else {
      inverse_patterns_.insert(inverse_pattern);
    }

    return;
  }

  if (index == std::string::npos) {
    exact_patterns_.insert(glob_pattern);

    // All patterns that don't end with * must have '.*'
    // added for consistent hierarchy propagation
    //
    // Otherwise, someone could, for example, blocklist a class:
    //
    // foo.bar.MyClass
    //
    // but methods, and variables in that class would still be visible:
    //
    // foo.bar.MyClass.MyVariable (visible)
    // foo.bar.MyClass.MyMethod (visible)
    // foo.far.MyClass$InnerClass (visible)
    //
    // which is both surprising and not useful.
    prefix_patterns_.push_back(glob_pattern + ".");
  } else if (index == glob_pattern.length() - 1) {
    prefix_patterns_.push_back(
        glob_pattern.substr(0, glob_pattern.length() - 1));
  } else {
    generic_patterns_.insert(glob_pattern);
  }
}

}  // namespace cdbg
}  // namespace devtools
