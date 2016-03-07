#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_NAMED_JVARIANT_TEST_UTIL_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_NAMED_JVARIANT_TEST_UTIL_H_

#include <list>
#include <string>
#include <vector>

#include "base/macros.h"
#include "type_util.h"
#include "model_json.h"
#include "util/java/mock_jni_env_full.h"

namespace devtools {
namespace cdbg {

using util::java::test::MockJNIEnvFull;

// Helper class for manipulating NamedJVariant objects in tests.
class NamedJvariantTestUtil {
 public:
  explicit NamedJvariantTestUtil(MockJNIEnvFull* jni);
  virtual ~NamedJvariantTestUtil();

  // Deep copies a vector of NamedJVariants.
  void CopyNamedJVariant(
      const std::vector<NamedJVariant>& source,
      std::vector<NamedJVariant>* destination);

  // Creates a NamedJVariant and adds it into a vector.
  void AddNamedJVariant(
      string name,
      JVariant* value,
      WellKnownJClass well_known_jclass,
      StatusMessageModel status,
      std::vector<NamedJVariant>* variables);

  // Creates a numeric-based NamedJVariant and adds it into a vector.
  template <typename T>
  void AddNumericVariable(
      const string& name,
      T value,
      std::vector<NamedJVariant>* variables) {
    JVariant value_variant = JVariant::Primitive<T>(value);
    AddNamedJVariant(
        name,
        &value_variant,
        WellKnownJClass::Unknown,
        {},
        variables);
  }

  // Creates a string-based NamedJVariant and adds it into a vector.
  void AddStringVariable(
      const string& name,
      const string& value,
      std::vector<NamedJVariant>* variables);

  // Creates a ref-based NamedJVariant and adds it into a vector.
  void AddRefVariable(
      const string& name,
      jobject ref,
      std::vector<NamedJVariant>* variables);

 private:
  // Mock JNI object used to manipulate the NamedJVariants.
  MockJNIEnvFull* jni_;

  // A buffer of all the jstrings created.
  std::list<std::vector<jchar>> jstring_buffers_;

  DISALLOW_COPY_AND_ASSIGN(NamedJvariantTestUtil);
};

}  // namespace cdbg
}  // namespace devtools
#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_NAMED_JVARIANT_TEST_UTIL_H_
