#include "named_jvariant_test_util.h"

using testing::NotNull;
using testing::Return;
using testing::Invoke;

namespace devtools {
namespace cdbg {

NamedJvariantTestUtil::NamedJvariantTestUtil(MockJNIEnvFull* jni) : jni_(jni) {}


NamedJvariantTestUtil::~NamedJvariantTestUtil() {}


void NamedJvariantTestUtil::CopyNamedJVariant(
    const std::vector<NamedJVariant>& source,
    std::vector<NamedJVariant>* destination) {
  *destination = std::vector<NamedJVariant>(source.size());
  for (int i = 0; i < source.size(); ++i) {
    (*destination)[i].name = source[i].name;
    (*destination)[i].value = JVariant(source[i].value);
    (*destination)[i].well_known_jclass = source[i].well_known_jclass;
    (*destination)[i].status = source[i].status;
  }
}


void NamedJvariantTestUtil::AddNamedJVariant(
    string name,
    JVariant* value,
    WellKnownJClass well_known_jclass,
    StatusMessageModel status,
    std::vector<NamedJVariant>* variables) {
  std::vector<NamedJVariant> newVariables(variables->size() + 1);
  for (int i = 0; i < variables->size(); ++i) {
    newVariables[i].name = std::move((*variables)[i].name);
    newVariables[i].value.swap(&(*variables)[i].value);
    newVariables[i].well_known_jclass = (*variables)[i].well_known_jclass;
    newVariables[i].status = (*variables)[i].status;
  }

  int new_index = newVariables.size() - 1;
  newVariables[new_index].name = std::move(name);
  newVariables[new_index].value.swap(value);
  newVariables[new_index].well_known_jclass = well_known_jclass;
  newVariables[new_index].status = status;

  *variables = std::move(newVariables);
}


void NamedJvariantTestUtil::AddStringVariable(
    const string& name,
    const string& value,
    std::vector<NamedJVariant>* variables) {
  std::vector<jchar> jstring_buffer(value.begin(), value.end());
  jstring jstr = reinterpret_cast<jstring>(&jstring_buffer[0]);

  EXPECT_CALL(*jni_, GetStringLength(jstr))
      .WillRepeatedly(Return(jstring_buffer.size()));

  EXPECT_CALL(*jni_, GetStringUTFRegion(jstr, 0, value.size(), NotNull()))
      .WillRepeatedly(
          Invoke([this, value](jstring ref, jsize start, jsize len, char* buf) {
            len += start;
            for (int i = start; i < len; ++i) {
              *buf++ = value[i];
            }
          }));

  JVariant jstr_variant = JVariant::GlobalRef(jstr);
  AddNamedJVariant(name, &jstr_variant, WellKnownJClass::String, {}, variables);

  jstring_buffers_.push_back(std::move(jstring_buffer));
}


void NamedJvariantTestUtil::AddRefVariable(
    const string& name,
    jobject ref,
    std::vector<NamedJVariant>* variables) {
  JVariant ref_variant = JVariant::GlobalRef(ref);
  AddNamedJVariant(name, &ref_variant, WellKnownJClass::Unknown, {}, variables);
}

}  // namespace cdbg
}  // namespace devtools
