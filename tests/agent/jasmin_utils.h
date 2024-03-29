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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JASMIN_UTILS_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JASMIN_UTILS_H_

#include "src/agent/common.h"

namespace devtools {
namespace cdbg {

//
// C++ wrapper for Jasmin assembler. Jasmin is an assembly for JVM.
// For more details see: http://jasmin.sourceforge.net/
//

// Builds Java class from assembly using Jasmin.
std::string Assemble(const std::string& asm_code);

// Assembles test Java class that only has a single method using Jasmin. We
// never set method arguments, since NanoJava interpreter ignores it anyway.
std::string AssembleMethod(const std::string& return_type,
                           const std::string& method_asm_code);

}  // namespace cdbg
}  // namespace devtools


#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JASMIN_UTILS_H_
