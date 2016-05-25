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

#include "${includeDirectory}common.h"
#include "${includeDirectory}jni_utils.h"

//
// Generated code. Don't edit.
//

<#list namespaces as namespace>
namespace ${namespace} {
</#list>

class ${normalizedClassName}Class {
 public:
  static constexpr char TypeName[] = "${cls.getName()}";
  static constexpr char Signature[] = "${classSignature}";
  static constexpr char TypeSignature[] = "L${classSignature};";

  virtual ~${normalizedClassName}Class() {}

  // C++ equivalent of "${cls.getName()}.class" Java construct.
  virtual jclass GetClass() = 0;
<#list constructors as constructor>

  // Creates a new object "${constructor.description}".
  virtual ::devtools::cdbg::ExceptionOr< ::devtools::cdbg::JniLocalRef > ${constructor.methodName}(
<#list constructor.arguments as argument>
      ${argument.type.nativeArgumentType} ${argument.name}<#if argument_has_next>,</#if>
</#list>
      ) = 0;
</#list>
<#list methods as method>

  // Invokes "${method.description}".
  virtual ${method.returnType.nativeReturnType} ${method.name}(
<#if !method.isStatic()>
      jobject instance<#if method.hasArguments>,</#if>
</#if>
<#list method.arguments as argument>
      ${argument.type.nativeArgumentType} ${argument.name}<#if argument_has_next>,</#if>
</#list>
      ) = 0;
<#if method_has_next>
</#if>
</#list>
};

// Loads ${cls.getName()} Java class and attaches to its methods. If this is
// the first time the class is being used, static constructor will be called.
bool Bind${normalizedClassName}();

// Loads ${cls.getName()} using the specified class loader. If this is the
// first time the class is being used, static constructor will be called.
bool Bind${normalizedClassName}WithClassLoader(jobject class_loader_obj);

// Dependency injection for unit test purposes.
void Inject${normalizedClassName}(${normalizedClassName}Class* proxy_class);

// Releases the resources allocated in Bind.
void Cleanup${normalizedClassName}();

// Gets the singleton instance of the proxy.
${normalizedClassName}Class* ${normalizedClassName}();

<#list namespaces as namespace>
}  // namespace ${namespace}
</#list>

#endif  // ${guard}
