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

//
// Generated code. Don't edit.
//

#include "${fileNameWithoutExtension}.h"

<#list namespaces as namespace>
namespace ${namespace} {
</#list>

constexpr char ${normalizedClassName}Class::TypeName[];
constexpr char ${normalizedClassName}Class::Signature[];
constexpr char ${normalizedClassName}Class::TypeSignature[];

static ${normalizedClassName}Class* g_proxy_class = nullptr;

class ${normalizedClassName}Impl : public ${normalizedClassName}Class {
 public:
  ${normalizedClassName}Impl() {}

  bool Bind() {
    if (!cls_.FindWithJNI(Signature)) {
      LOG(ERROR) << "Failed to load Java class ${cls.getName()}";
      return false;
    }

    return BindMethods();
  }

  bool BindWithClassLoader(jobject class_loader_obj) {
    if (!cls_.LoadWithClassLoader(class_loader_obj, TypeName)) {
      LOG(ERROR) << "Failed to load Java class ${cls.getName()}";
      return false;
    }

    return BindMethods();
  }

  jclass GetClass() override {
    return cls_.get();
  }

<#list constructors as constructor>
  ::devtools::cdbg::ExceptionOr< ::devtools::cdbg::JniLocalRef > ${constructor.methodName}(
<#list constructor.arguments as argument>
      ${argument.type.nativeArgumentType} ${argument.name}<#if argument_has_next>,</#if>
</#list>
      ) override {
    <#if constructor.subclassConstructor>
    if (!::devtools::cdbg::jni()->IsAssignableFrom(static_cast<jclass>(cls), cls_.get())) {
      LOG(ERROR) << "Class is not assignable to ${cls.getName()}";
      return ::devtools::cdbg::ExceptionOr< ::devtools::cdbg::JniLocalRef >(nullptr, nullptr, nullptr);
    }

    </#if>
    ::devtools::cdbg::JniLocalRef rc(::devtools::cdbg::jni()->NewObject(
        <#if constructor.subclassConstructor>static_cast<jclass>(cls)<#else>cls_.get()</#if>,
        constructor${constructor.id}_<#if constructor.hasArguments>,</#if>
<#list constructor.constructorArguments as argument>
        ${argument.argumentConversion}<#if argument_has_next>,</#if>
</#list>
        ));

    return ::devtools::cdbg::CatchOr(
        "${normalizedClassName}.<init>",
        std::move(rc));
  }

</#list>
<#list methods as method>
  ${method.returnType.nativeReturnType} ${method.name}(
<#if !method.isStatic()>
      jobject instance<#if method.hasArguments>,</#if>
</#if>
<#list method.arguments as argument>
      ${argument.type.nativeArgumentType} ${argument.name}<#if argument_has_next>,</#if>
</#list>
      ) override {
<#if method.returnType.jniType != "void">
    ${method.returnType.jniType} rc =
</#if>
<#if method.isStatic()>
        ::devtools::cdbg::jni()->CallStatic${method.returnType.jniCallMethodTypeName}Method(
            cls_.get(),
<#else>
        ::devtools::cdbg::jni()->Call${method.returnType.jniCallMethodTypeName}Method(
            instance,
</#if>
        method_${method.name}_${method.id}_<#if method.hasArguments>,</#if>
<#list method.arguments as argument>
        ${argument.argumentConversion}<#if argument_has_next>,</#if>
</#list>
        );

    return ::devtools::cdbg::CatchOr(
        "${normalizedClassName}.${method.name}",
        ${method.returnType.getReturnValueConversion("rc")});
  }
<#if method_has_next>

</#if>
</#list>

 private:
  // Loads class methods assuming "cls_" is already available.
  bool BindMethods() {
    <#list constructors as constructor>
      constructor${constructor.id}_ =
          cls_.GetInstanceMethod(
              "<init>",
              "${constructor.signature}");
      if (constructor${constructor.id}_ == nullptr) {
        LOG(ERROR) << "Failed to load Java class ${cls.getName()}"
                   << ", constructor ${constructor.description} not found";
        return false;
      }

    </#list>
    <#list methods as method>
      // Load "${method.description}".
      method_${method.name}_${method.id}_ =
    <#if method.isStatic()>
          cls_.GetStaticMethod(
    <#else>
          cls_.GetInstanceMethod(
    </#if>
              "${method.name}",
              "${method.signature}");
      if (method_${method.name}_${method.id}_ == nullptr) {
        LOG(ERROR) << "Failed to load Java class ${cls.getName()}"
                   << ", method ${method.name} not found";
        return false;
      }

    </#list>
      return true;
  }

 private:
  // Global reference to Java class object.
  ::devtools::cdbg::JavaClass cls_;
<#list constructors as constructor>

  // Method ID of class constructor "${constructor.description}".
  jmethodID constructor${constructor.id}_ { nullptr };
</#list>
<#list methods as method>

  // Method ID of "${method.description}".
  jmethodID method_${method.name}_${method.id}_ { nullptr };
</#list>

  DISALLOW_COPY_AND_ASSIGN(${normalizedClassName}Impl);
};


bool Bind${normalizedClassName}() {
  DCHECK(g_proxy_class == nullptr);

  std::unique_ptr<${normalizedClassName}Impl> proxy_class(new ${normalizedClassName}Impl);
  if (!proxy_class->Bind()) {
    return false;
  }

  g_proxy_class = proxy_class.release();

  return true;
}


bool Bind${normalizedClassName}WithClassLoader(jobject class_loader_obj) {
  DCHECK(g_proxy_class == nullptr);

  std::unique_ptr<${normalizedClassName}Impl> proxy_class(new ${normalizedClassName}Impl);
  if (!proxy_class->BindWithClassLoader(class_loader_obj)) {
    return false;
  }

  g_proxy_class = proxy_class.release();

  return true;
}


void Inject${normalizedClassName}(${normalizedClassName}Class* proxy_class) {
  g_proxy_class = proxy_class;
}



void Cleanup${normalizedClassName}() {
  if (g_proxy_class != nullptr) {
    delete g_proxy_class;
    g_proxy_class = nullptr;
  }
}


${normalizedClassName}Class* ${normalizedClassName}() {
  DCHECK(g_proxy_class != nullptr);
  return g_proxy_class;
}

<#list namespaces as namespace>
}  // namespace ${namespace}
</#list>
