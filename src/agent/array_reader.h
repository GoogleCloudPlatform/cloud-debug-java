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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_ARRAY_READER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_ARRAY_READER_H_

#include <memory>
#include "common.h"
#include "model_util.h"

namespace devtools {
namespace cdbg {

class JVariant;
class FormatMessageModel;

// Reads array element.
class ArrayReader {
 public:
  virtual ~ArrayReader() { }

  // Reads "source[index]". In case of error sets "error_message".
  virtual ErrorOr<JVariant> ReadValue(
      const JVariant& source,
      const JVariant& index) const = 0;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_ARRAY_READER_H_



