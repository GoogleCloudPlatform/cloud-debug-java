#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_METHOD_CALLER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_METHOD_CALLER_H_

#include "method_caller.h"
#include "gmock/gmock.h"

namespace devtools {
namespace cdbg {

class MockMethodCaller : public MethodCaller {
 public:
  // Transform this function to a version that's easier to mock.
  ErrorOr<JVariant> Invoke(
      const ClassMetadataReader::Method& metadata,
      const JVariant& source,
      std::vector<JVariant> arguments) override {
    std::string s;
    s += "class = ";
    s += metadata.class_signature.object_signature;
    s += ", method name = ";
    s += metadata.name;
    s += ", method signature = ";
    s += metadata.signature;
    s += ", source = ";
    s += source.ToString(false);
    s += ", arguments = (";
    for (int i = 0; i < arguments.size(); ++i) {
      if (i > 0) {
        s += ", ";
      }
      s += arguments[i].ToString(false);
    }
    s += ')';

    ErrorOr<const JVariant*> rc = Invoke(s);
    if (rc.is_error()) {
      return rc.error_message();
    }

    return JVariant(*rc.value());
  }

  MOCK_METHOD(ErrorOr<const JVariant*>, Invoke, (const std::string&));
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_METHOD_CALLER_H_
