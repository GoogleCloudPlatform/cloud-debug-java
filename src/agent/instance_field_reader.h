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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_INSTANCE_FIELD_READER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_INSTANCE_FIELD_READER_H_

#include "common.h"
#include "model.h"
#include "type_util.h"

namespace devtools {
namespace cdbg {

class JVariant;

// Interface for a reader of member variables. This does not include static
// variables and array access.
class InstanceFieldReader {
 public:
  virtual ~InstanceFieldReader() { }

  // Creates copy of this instance. The caller owns the returned value.
  virtual std::unique_ptr<InstanceFieldReader> Clone() const = 0;

  // Gets the name of the member variable.
  virtual const std::string& GetName() const = 0;

  // Gets the type of the member variable.
  virtual const JSignature& GetStaticType() const = 0;

  // Reads the value of the member variable from "source_object". If the
  // instance field is of an object type, "result" will contain a local
  // reference.
  //
  // If there is an error, false is returned and the error field is populated
  // with an error message.
  virtual bool ReadValue(
      jobject source_object,
      JVariant* result,
      FormatMessageModel* error) const = 0;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_INSTANCE_FIELD_READER_H_
