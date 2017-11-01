#include "glob_data_visibility_policy.h"

#include <fnmatch.h>
#include <algorithm>
#include "type_util.h"

namespace devtools {
namespace cdbg {

static constexpr char kReasonIsBlacklisted[] = "blacklisted by config";

namespace {

class ClassImpl : public DataVisibilityPolicy::Class {
 public:
  // Does not take ownership over "class_config", which must outlive this
  // instance.
  ClassImpl(
      const string& class_name,
      const GlobDataVisibilityPolicy::Config* config)
      : class_name_(class_name),
        config_(config) {
  }

  bool IsFieldVisible(
      const string& field_name,
      int32 field_modifiers) override {
    return true;
  }

  bool IsFieldDataVisible(
      const string& field_name,
      int32 field_modifiers,
      string* reason) override {
    return IsVisible(field_name, reason);
  }

  bool IsMethodVisible(
      const string& method_name,
      const string& method_signature,
      int32 method_modifiers) override {
    string unused_reason;
    return IsVisible(method_name, &unused_reason);
  }

  bool IsVariableVisible(
      const string& method_name,
      const string& method_signature,
      const string& variable_name) override {
    return true;
  }

  bool IsVariableDataVisible(
      const string& method_name,
      const string& method_signature,
      const string& variable_name,
      string* reason) override {
    return IsVisible(method_name + "." + variable_name, reason);
  }

 private:
  // Returns true if this class with the given suffix is visible.
  // Example:
  //   class_name_ = com.foo.MyClass
  //   suffix = myMethod
  //   checks: com.foo.MyClass.myMethod
  bool IsVisible(const string& suffix, string* reason) {
    string path = class_name_ + "." + suffix;
    if (config_->blacklists.Matches(path) &&
        !config_->blacklist_exceptions.Matches(path)) {
      *reason = kReasonIsBlacklisted;
      return false;
    }

    return true;
  }

 private:
  // Name of this class
  const string class_name_;

  // read-only pointer back to the global glob config
  const GlobDataVisibilityPolicy::Config* config_;
};


// A simple implementation of DataVisibilityPolicy::Class which always
// reports that methods and fields have their data hidden.
class BlacklistedClassImpl : public DataVisibilityPolicy::Class {
 public:
  BlacklistedClassImpl(const string& reason) :
    reason_(reason) { }

  bool IsFieldVisible(const string& name, int32 field_modifiers) override {
    return true;
  }

  bool IsFieldDataVisible(
      const string& name,
      int32 field_modifiers,
      string* reason) override {
    *reason = reason_;
    return false;
  }

  bool IsMethodVisible(
      const string& method_name,
      const string& method_signature,
      int32 method_modifiers) override {
    return false;
  }

  bool IsVariableVisible(
      const string& method_name,
      const string& method_signature,
      const string& variable_name) override {
    return true;
  }

  bool IsVariableDataVisible(
      const string& method_name,
      const string& method_signature,
      const string& variable_name,
      string* reason) override {
    *reason = reason_;
    return false;
  }

 private:
  const string reason_;
};

}  // namespace

std::unique_ptr<DataVisibilityPolicy::Class>
GlobDataVisibilityPolicy::GetClassVisibility(jclass cls) {
  if (!config_.parse_error.empty()) {
    // There was a parsing error while trying to load the debugger config.
    return std::unique_ptr<Class>(
        new BlacklistedClassImpl(config_.parse_error));
  }

  string signature = GetClassSignature(cls);
  if ((signature.size() < 3) ||
      (signature.front() != 'L') || (signature.back() != ';')) {
    return nullptr;  // Invalid class signature.
  }

  string path = TypeNameFromJObjectSignature(signature);
  // replace $ with . in paths.  Without this replacement, someone
  // could try and blacklist all class members with a rule like
  //
  // com.foo.MyClass.*
  //
  // but miss inner classes
  //
  // com.foo.MyClass$Inner (visible)
  std::replace(path.begin(), path.end(), '$', '.');

  // If neither this class nor it's members can ever be blacklisted,
  // return nullptr as an optimization.
  if (!config_.blacklists.PrefixCanMatch(path)) {
    return nullptr;
  }

  // If this class matches an exception, return null as an optimization
  if (config_.blacklist_exceptions.Matches(path)) {
    return nullptr;
  }

  // If the class will always be blacklisted, return a BlacklistedClassImpl,
  // which skips checks, saving time and memory for visibility checks on this
  // class.
  if (config_.blacklists.Matches(path) &&
      !config_.blacklist_exceptions.PrefixCanMatch(path)) {
    return std::unique_ptr<Class>(
        new BlacklistedClassImpl(kReasonIsBlacklisted));
  }

  return std::unique_ptr<Class>(new ClassImpl(path, &config_));
}


// Returns true if path can be matched by the wildcard pattern.
static bool WildcardMatches(const string& path, const string& pattern) {
  constexpr int kFlags = 0;
  return fnmatch(pattern.c_str(), path.c_str(), kFlags) == 0;
}


// GenericMatches works in all cases but uses a slow O(n) algorithm.  Due to
// it's performance, it's intended to process the generic_patterns_ set.
static bool GenericMatches(
    const string& path,
    const std::set<string>& generic_match) {

  for (const string& pattern : generic_match) {
    if (WildcardMatches(path, pattern)) {
      return true;
    }
  }
  return false;
}

// Returns true if a path does not match anything is exact_inverse_patterns or
// inverse_patterns.
static bool InverseMatches(
    const string& path,
    const std::set<string>& exact_inverse_patterns,
    const std::set<string>& inverse_patterns) {
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
static bool PrefixMatches(
    const string& path,
    const std::vector<string>& prefix_match) {
  // Find the lower bound, but only consider the matching prefixes in
  // each comparison
  auto lower_bound = std::lower_bound(
      prefix_match.begin(),
      prefix_match.end(),
      path,
      [] (const string& prefix, const string& path) {
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
static void RemoveRedundantPrefixes(std::vector<string>* prefixes) {
  if (prefixes->size() <= 1) {
    // Nothing to do
    return;
  }
  int base = 0;
  for (int compare = 1; compare < prefixes->size(); ++compare) {
    const bool keep_prefix = !std::equal(
        prefixes->at(base).begin(),
        prefixes->at(base).end(),
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


bool GlobDataVisibilityPolicy::GlobSet::Matches(const string& path) const {
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


void GlobDataVisibilityPolicy::GlobSet::Add(const string& glob_pattern) {
  prepared_ = false;

  if (glob_pattern.empty()) {
    return;
  }

  const size_t index = glob_pattern.find('*');

  // Check for an inverted pattern (patterns that start with a !)
  if (glob_pattern[0] == '!') {
    // Drop the leading ! character
    const string inverse_pattern = glob_pattern.substr(1);

    if (index == string::npos) {
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

  if (index == string::npos) {
    exact_patterns_.insert(glob_pattern);

    // All patterns that don't end with * must have '.*'
    // added for consistent heirarchy propagation
    //
    // Otherwise, someone could, for example, blacklist a class:
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


// Returns true if it's possible for prefix* to match pattern.
static bool PrefixCanMatchPattern(
    const string& prefix,
    const string& pattern) {
  for (int i = 0; i < pattern.length() && i < prefix.length(); ++i) {
    // It's possible for prefix to match pattern if either of the following
    // are true:
    //
    // 1) prefix is a prefix of pattern
    // 2) prefix[:n] is a prefix of pattern where pattern[n] is a '*'
    if (pattern[i] == '*') {
      return true;
    }

    if (pattern[i] != prefix[i]) {
      return false;
    }
  }

  return true;
}


bool GlobDataVisibilityPolicy::GlobSet::PrefixCanMatch(
    const string& prefix) const {
  if (exact_patterns_.find(prefix) != exact_patterns_.end()) {
    return true;
  }

  // TODO(mattwach): This can probably be done with more efficiency,
  // but delay it for it's own CL.
  for (const string& pattern : prefix_patterns_) {
    if (PrefixCanMatchPattern(prefix, pattern + '*')) {
      return true;
    }
  }

  for (const string& pattern : generic_patterns_) {
    if (PrefixCanMatchPattern(prefix, pattern)) {
      return true;
    }
  }

  // For inverse patterns.  If a prefix does not match any inverse
  // pattern then it's a prefix that can match
  for (const string& pattern : inverse_patterns_) {
    if (!PrefixCanMatchPattern(prefix, pattern)) {
      return true;
    }
  }

  return false;
}


}  // namespace cdbg
}  // namespace devtools
