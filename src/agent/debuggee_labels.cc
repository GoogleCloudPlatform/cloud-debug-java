#include "debuggee_labels.h"

namespace devtools {
namespace cdbg {

constexpr char DebuggeeLabels::kBlocklistSourceLabel[];
constexpr char DebuggeeLabels::kBlocklistSourceDeprecatedFile[];
constexpr char DebuggeeLabels::kBlocklistSourceFile[];
constexpr char DebuggeeLabels::kBlocklistSourceNone[];

void DebuggeeLabels::Set(const std::string& name, const std::string& value) {
  labels_[name] = value;
}

JniLocalRef DebuggeeLabels::Get() const {
  ExceptionOr<JniLocalRef> jni_labels = jniproxy::HashMap()->NewObject();

  if (jni_labels.HasException()) {
    // This means an error occurred trying to allocate the HashMap in the JVM.
    return JniLocalRef();
  }

  for (const auto& label : labels_) {
    auto rc = jniproxy::HashMap()->put(jni_labels.GetData().get(),
                                       JniToJavaString(label.first).get(),
                                       JniToJavaString(label.second).get());

    if (rc.HasException()) {
      return JniLocalRef();
    }
  }

  return jni_labels.Release(ExceptionAction::IGNORE);
}


}  // namespace cdbg
}  // namespace devtools
