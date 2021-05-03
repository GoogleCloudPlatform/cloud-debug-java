#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_DEBUGGEE_LABELS_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_DEBUGGEE_LABELS_H_

#include <map>

#include "jni_proxy_ju_hashmap.h"

namespace devtools {
namespace cdbg {

// Utility class to hold labels to include in the registerDebuggee call that
// goes to the HubClient on the Java side of the agent. This class will hold the
// labels and then will handle generating the Java HashMap that can use used in
// the registerDebuggee call.
class DebuggeeLabels {
 public:
  DebuggeeLabels() = default;

  static constexpr char kBlocklistSourceLabel[] = "blocklistsource";

  // Value for the BlocklistSource label which indicates the deprecated file
  // name and format was used for specifying the blocklist.
  static constexpr char kBlocklistSourceDeprecatedFile[] = "deprecatedfile";

  // Value for the BlocklistSource label which indicates the new blocklist file
  // name and format was used for specifying the blocklist.
  static constexpr char kBlocklistSourceFile[] = "file";

  // Value for the BlocklistSource label which indicates no blocklist was
  // specfified.
  static constexpr char kBlocklistSourceNone[] = "none";

  void Set(const std::string& name, const std::string& value);

  JniLocalRef Get() const;

 private:
  std::map<std::string, std::string> labels_;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_DEBUGGEE_LABELS_H_
