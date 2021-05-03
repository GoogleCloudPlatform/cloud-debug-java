#include "debuggee_labels.h"

namespace devtools {
namespace cdbg {

constexpr char DebuggeeLabels::kBlocklistSourceLabel[];
constexpr char DebuggeeLabels::kBlocklistSourceDeprecatedFile[];
constexpr char DebuggeeLabels::kBlocklistSourceFile[];
constexpr char DebuggeeLabels::kBlocklistSourceNone[];

void DebuggeeLabels::Set(const std::string& /* name */,
                         const std::string& /* value */) {
  // TODO: Implement in follow on CL.
}

}  // namespace cdbg
}  // namespace devtools
