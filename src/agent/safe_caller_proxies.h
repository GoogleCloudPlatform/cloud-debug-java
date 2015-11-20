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
