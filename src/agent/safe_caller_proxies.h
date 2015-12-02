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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_SAFE_CALLER_PROXIES_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_SAFE_CALLER_PROXIES_H_

#include "common.h"
#include "jvariant.h"
#include "method_call_result.h"
#include "method_caller.h"

namespace devtools {
namespace cdbg {

class SafeMethodCaller;

//
// Function to invoke before certain allowed Java methods are invoked. For
// example while we allow "Object.clone", we need to make sure the code isn't
// duplicating huge arrays.
//

// Block cloning of huge arrays.
MethodCallResult ObjectClonePre(
    SafeMethodCaller* caller,
    jobject unused_instance,
    std::vector<JVariant>* arguments);

// Verify that the destination array is a temporary object and block copying
// huge arrays.
MethodCallResult SystemArraycopyPre(
    SafeMethodCaller* caller,
    jobject unused_instance,
    std::vector<JVariant>* arguments);

// Safely call "toString()" on Object arguments to String.format and
// replace the arguments.
MethodCallResult StringFormatPre(
    SafeMethodCaller* caller,
    jobject unused_instance,
    std::vector<JVariant>* arguments);

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_SAFE_CALLER_PROXIES_H_
