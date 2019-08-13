/**
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "generic_type_evaluator.h"

#include "instance_field_reader.h"
#include "jvariant.h"
#include "messages.h"
#include "model.h"

namespace devtools {
namespace cdbg {

void GenericTypeEvaluator::Evaluate(
    MethodCaller* method_caller,
    const ClassMetadataReader::Entry& class_metadata,
    jobject obj,
    bool is_watch_expression,
    std::vector<NamedJVariant>* result) {

  if (class_metadata.instance_fields.empty() &&
      !class_metadata.instance_fields_omitted) {
    result->clear();
    result->push_back(NamedJVariant::InfoStatus({ ObjectHasNoFields }));

    return;
  }

  *result = std::vector<NamedJVariant>(class_metadata.instance_fields.size());
  for (int i = 0; i < class_metadata.instance_fields.size(); ++i) {
    const InstanceFieldReader& field_reader =
        *class_metadata.instance_fields[i];

    NamedJVariant& field_data = (*result)[i];

    field_data.name = field_reader.GetName();

    FormatMessageModel error;
    if (!field_reader.ReadValue(obj, &field_data.value, &error)) {
      field_data.status.is_error = false;
      field_data.status.refers_to =
          StatusMessageModel::Context::VARIABLE_VALUE;
      field_data.status.description = error;
    } else {
      field_data.well_known_jclass =
          WellKnownJClassFromSignature(field_reader.GetStaticType());
    }

    field_data.value.change_ref_type(JVariant::ReferenceKind::Global);
  }

  if (class_metadata.instance_fields_omitted) {
    // TODO: improve this message for @InvisibleForDebugging case.
    result->push_back(NamedJVariant::InfoStatus({ InstanceFieldsOmitted }));
  }
}

}  // namespace cdbg
}  // namespace devtools


