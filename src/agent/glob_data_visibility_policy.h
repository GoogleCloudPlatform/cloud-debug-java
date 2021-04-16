/**
 * Copyright 2017 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_GLOB_DATA_VISIBILITY_POLICY_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_GLOB_DATA_VISIBILITY_POLICY_H_

#include <set>

#include "data_visibility_policy.h"

namespace devtools {
namespace cdbg {

// Specifies lists of glob patterns that can act as blocklists and blocklist
// exceptions.
class GlobDataVisibilityPolicy : public DataVisibilityPolicy {
 public:
  class GlobSet {
   public:
    // Adds a new pattern
    void Add(const std::string& glob_pattern);

    // Prepares the globset for matching.  Must be called once between calling
    // Add and calling Match().
    void Prepare();

    // Returns true if at least one pattern in this GlobSet matches the given
    // path.
    bool Matches(const std::string& path) const;

    // Returns true if at least one pattern in this GlobSet matches the prefix*
    // pattern.
    //
    // For example, say the prefix is foo.bar.MyClass
    //
    // Presence any of the following glob patterns would return a "true" result
    //
    // foo.bar.MyClass
    // foo.bar.MyClass.xyz
    // *
    // *anything
    // foo*anything
    //
    // whereas presence of the following patterns can never match any path
    // prefixed with foo.bar.MyClass
    //
    // foo.bar.YourClass
    // java.util.*
    bool PrefixCanMatch(const std::string& prefix) const;

    // Returns true if this GlobSet contains zero patterns.
    bool Empty() const {
      return exact_patterns_.empty() && prefix_patterns_.empty() &&
             generic_patterns_.empty() && inverse_patterns_.empty() &&
             exact_inverse_patterns_.empty();
    }

   private:
    // Patterns that do not contain a *.  These can be resolved with a direct
    // lookup.
    std::set<std::string> exact_patterns_;

    // Patterns that start with a string and end with a single *.  These can
    // be efficiently resolved in O(log n) time
    //
    // Note: the '*' is not present in the data below.
    // Note: This is a vector because std::lower_bound, used in the prefix match
    // algorithm, requires a random-access container for O(log n) efficiency.
    std::vector<std::string> prefix_patterns_;

    // Patterns that contain one or more * characters and do not qualify for
    // membership in prefix_patterns_ above.  Optimizing these further is
    // possible, but seeminly in trade for additional complexity.  These
    // patterns are also expected to be more rarely used than the other cases.
    std::set<std::string> generic_patterns_;

    // Patterns that are inverted.  e.g. while all other patterns
    // would consider a* matching apple and b* not matching apple, these
    // inverse patterns are the opposite (a* does not "inverse match" apple,
    // b* does "inverse match" apple).
    //
    // exact_inverse_patterns_ do not contain any glob characters.
    // inverse_patterns_ contain at least one *.  For a symbol to be considered
    // a match, it has to not be found in exact_inverse_patterns_ and not match
    // anything in inverse_patterns_.
    std::set<std::string> exact_inverse_patterns_;
    std::set<std::string> inverse_patterns_;

    // Set to true if the GlobSet is ready for calls to Match().
    bool prepared_ = true;
  };

  // Configuration
  struct Config {
    GlobSet blocklists;
    GlobSet blocklist_exceptions;
    // This string is left empty if there was no parsing error.
    std::string parse_error;
  };

  // Initalizes with a configuration that blocks everything.  Call setConfig()
  // to change the configuration.
  explicit GlobDataVisibilityPolicy();

  void SetConfig(Config config) { config_ = std::move(config); }

  std::unique_ptr<Class> GetClassVisibility(jclass cls) override;

  bool HasSetupError(std::string* error) const override;

 private:
  Config config_;

  DISALLOW_COPY_AND_ASSIGN(GlobDataVisibilityPolicy);
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_GLOB_DATA_VISIBILITY_POLICY_H_
