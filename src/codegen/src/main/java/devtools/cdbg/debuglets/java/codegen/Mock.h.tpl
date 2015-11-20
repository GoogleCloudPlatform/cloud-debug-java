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

<#assign guard = "DEVTOOLS_CDBG_DEBUGLETS_JAVA_${guardFileName}">
#ifndef ${guard}
#define ${guard}

#include "jni_proxy_${fileNameClass}.h"
#include "testing/base/public/gmock.h"

//
// Generated code. Don't edit.
//

<#list namespaces as namespace>
namespace ${namespace} {
</#list>

class Mock${normalizedClassName} : public ${normalizedClassName}Class {
 public:
  MOCK_METHOD0(GetClass, jclass());

<#list constructors as constructor>
  MOCK_METHOD${constructor.arguments?size}(${constructor.methodName},
      ::devtools::cdbg::ExceptionOr< ::devtools::cdbg::JniLocalRef >(
<#list constructor.arguments as argument>
          ${argument.type.nativeArgumentType}<#if argument_has_next>,</#if>
</#list>
      ));

</#list>
<#list methods as method>
<#assign count = method.arguments?size>
<#if !method.isStatic()>
<#assign count = count + 1>
</#if>
  MOCK_METHOD${count}(${method.name}, ${method.returnType.nativeReturnType}(
<#if !method.isStatic()>
      jobject<#if method.hasArguments>,</#if>  // instance
</#if>
<#list method.arguments as argument>
      ${argument.type.nativeArgumentType}<#if argument_has_next>,</#if>
</#list>
      ));
<#if method_has_next>

</#if>
</#list>
};

<#list namespaces as namespace>
}  // namespace ${namespace}
</#list>

#endif // ${guard}
