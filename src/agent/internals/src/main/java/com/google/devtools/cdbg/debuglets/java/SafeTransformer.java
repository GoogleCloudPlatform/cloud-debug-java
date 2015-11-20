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

package com.google.devtools.cdbg.debuglets.java;

import static com.google.devtools.cdbg.debuglets.java.AgentLogger.warnfmt;

import org.objectweb.asm.Type;

import java.io.IOException;
import java.lang.reflect.Method;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.Objects;

/**
 * Ensures safety of application methods invoked by the debuglet.
 *
 * <p>This is a singleton class. It needs to be initialized before it can be used.
 *
 * <p>The debugger does not allow calling methods that can change the state of the application. This
 * would break the fundamental assumption that Cloud Debugger provides a "read-only" experience.
 *
 * <p>The safety is guaranteed by these techniques:
 * <ul>
 * <li>The method is whitelisted as knowingly safe (e.g. {@link String#format}).
 * <li>The method is statically verified to be safe (e.g. simple getters).
 * <li>The method bytecode is transformed into one that asserts no side effects at runtime.
 * </ul>
 *
 * <p>This class is thread safe.
 */
final class SafeTransformer {
  /**
   * Enables bytecode rewrite to verify method safety dynamically.
   */
  private static final boolean ENABLE_SAFE_CALLER =
      Boolean.getBoolean("com.google.cdbg.safecaller.enable");

  /**
   * Whitelist and blacklist of methods.
   */
  private static MethodsFilter config;


  /**
   * Provides Cloud Debugger configuration.
   *
   * <p>TODO(vlif): refactor to make configuration static, so that we don't need to pass it around.
   */
  static void setConfig(MethodsFilter config) {
    SafeTransformer.config = config;
  }

  /**
   * Applies call safety rules to the method.
   *
   * @param cls class object in which we want to invoke a method
   * @param methodName name of the method we want to invoke (e.g. "toString")
   * @param methodSignature signature of the method we want to invoke (e.g. "(IJ)V")
   * @param isStatic true if the origin method is static
   * @return null if the method call is blocked or a class on which the method should be invoked
   *    (this will be either the original class or dynamically generated safe class)
   */
  public static Class<?> apply(
      Class<?> cls,
      String methodName,
      String methodSignature,  // TODO(vlif): remove once MethodAnalyzer is gone.
      boolean isStatic) throws ClassNotFoundException {
    // Check against whitelist and blacklist.
    Boolean isSafe = config.isSafeMethod(Type.getInternalName(cls), methodName, isStatic);
    if (isSafe != null) {
      return isSafe ? cls : null;
    }

    ClassLoader classLoader = classLoaderFromClass(cls);

    // Apply static analysis to the method bytecode.
    // TODO(vlif): remove static analysis altogether once safe caller is enabled by default.
    // Static analysis cover very little and in some scenarios it messes up with safe caller.
    // For example it allows "new int[]", but doesn't register the new array as a temporary object.
    if (!ENABLE_SAFE_CALLER) {
      try {
        if (MethodAnalyzer.analyzeMethod(
              config,
              classLoader.getResourceAsStream(Type.getInternalName(cls) + ".class"),
              methodName,
              methodSignature,
              ENABLE_SAFE_CALLER)) {
          return cls;  // Call method on the original class.
        }
      } catch (IOException e) {
        warnfmt(e, "Static method analysis failed, class name: %s, method name: %s, signature: %s",
            cls.getName(), methodName, methodSignature);
      }
    }


    return null;  // Block the call.
  }

  /**
   * Gets the safe caller class corresponding to the class of specified real object.
   *
   * @return safe class or same class if no wrapping is necessary (e.g. {@link String}).
   */
  public static Class<?> getSafeClass(Class<?> cls) throws ClassNotFoundException {
    return getSafeClass(classLoaderFromClass(cls), cls);
  }

  /**
   * Gets the safe caller class corresponding to the type of array elements.
   */
  public static Class<?> getSafeArrayItemClass(Object array) throws ClassNotFoundException {
    Class<?> cls = Objects.requireNonNull(array.getClass().getComponentType());
    return getSafeClass(classLoaderFromClass(cls), cls);
  }

  /**
   * Returns true if the specified type needs wrapping with proxy objects for safe caller.
   *
   * <p>For example given signature "Lcom/MyClass;" this function will return true, because
   * this application class has a safe class (which would be "L$cdbg_safe_class$/com/MyClass;".
   * At the same time for "Ljava/lang/Integer;" we don't create a proxy class, so this
   * function will return false.
   *
   * <p>This method does not support arrays and primitive types. The input for this class
   * should not be signature of the safe class.
   *
   * @param signature JVMTI signature of an application/JDK class (e.g. "Lcom/MyClass;").
   */
  public static boolean isSafeClassApplicable(String signature) {
    if (!ENABLE_SAFE_CALLER) {
      return false;
    }
return false;
  }

  /**
   * Gets the safe caller class corresponding to the specified real class.
   *
   * @return safe class or same class if no wrapping is necessary (e.g. {@link String}), or null
   *     if safe class is not available.
   * @throws ClassNotFoundException in case of an error
   */
  private static Class<?> getSafeClass(
      ClassLoader classLoader, Class<?> originalClass) throws ClassNotFoundException {
    if (!ENABLE_SAFE_CALLER) {
      return originalClass;
    }

return null;
  }

  /**
   * Gets class loader that loaded the specified class.
   */
  private static ClassLoader classLoaderFromClass(final Class<?> cls) {
    return AccessController.doPrivileged(
        new PrivilegedAction<ClassLoader>() {
          @Override
          public ClassLoader run() {
            ClassLoader classLoader = cls.getClassLoader();
            if (classLoader == null) {
              classLoader = ClassLoader.getSystemClassLoader();
            }

            return classLoader;
          }
        });
  }

  ///////////////////////////////////////////////////////////////////
  // COMPATIBILITY LAYER WITH libjvmstatagent.so in JDK 7_13
  // TODO(vlif): delete once debugger no longer support JDK 7_13
  ///////////////////////////////////////////////////////////////////
  static final class Result {
    private final boolean blocked;

    private Result(boolean blocked) {
      this.blocked = blocked;
    }

    public boolean getIsBlocked() {
      return blocked;
    }

    public boolean getIsTransformed() {
      return false;
    }

    public Method getTransformedMethod() {
      return null;
    }

    int getSafeClassId() {
      return -1;
    }
  }

  private static SafeTransformer instance = new SafeTransformer();

  public static SafeTransformer getInstance() {
    return Objects.requireNonNull(instance);
  }

  @SuppressWarnings("unused")
  public Result apply(Object target, String methodName,
      String methodSignature, boolean deferTransform) {
    throw new UnsupportedOperationException();
  }

  @SuppressWarnings("unused")
  public void ensureSafeClass(int safeClassId) {
    throw new UnsupportedOperationException();
  }

  public Result apply(ClassLoader classLoader, String classSignature, String methodName,
      String methodSignature, boolean isStatic,
      @SuppressWarnings("unused") boolean deferTransform) {
    try {
      Class<?> originalClass = Class.forName(classSignature.replace('/', '.'), false, classLoader);
      Class<?> result = SafeTransformer.apply(originalClass, methodName, methodSignature, isStatic);
      return new Result(originalClass != result);
    } catch (ClassNotFoundException e) {
      return new Result(true);
    }
  }
}
