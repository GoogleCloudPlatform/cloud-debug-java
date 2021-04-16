/**
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * <p>Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 *
 * <p>http://www.apache.org/licenses/LICENSE-2.0
 *
 * <p>Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.google.devtools.cdbg.debuglets.java;

import java.util.Arrays;

/** Exception thrown when the executed user code proves to be unsafe. */
public final class SafeCallerException extends Error {
  /** Error message format. */
  private final String format;

  /** Parameters substituting $n placeholders in {@link SafeCallerException#format}. */
  private final String[] parameters;

  public SafeCallerException(String format, String[] parameters) {
    super(format + ((parameters == null) ? "" : (" " + Arrays.toString(parameters))));
    this.format = format;
    this.parameters = parameters;
  }

  /** Gets the error message format. */
  public String getFormat() {
    return format;
  }

  /** Gets the parameters substituting $n placeholders in {@link getFormat}. */
  public String[] getParameters() {
    return parameters;
  }

  public static void throwInternalError(String file, int line) {
    throw new SafeCallerException(
        "Internal error at $0:$1", new String[] {file, Integer.toString(line)});
  }

  public static void throwMethodNotAllowed(String method) {
    throw new SafeCallerException("Method call $0 is not allowed", new String[] {method});
  }

  public static void throwChangingStaticFieldNotAllowed(String field) {
    throw new SafeCallerException(
        "Method $0 is not safe (attempted to change static field $1)",
        new String[] {getCallerName(), field});
  }

  public static void throwMethodBlocked(String method) {
    throw new SafeCallerException("Calling $0 is not allowed", new String[] {method});
  }

  public static void throwInvokeDynamicNotSupported(String method) {
    throw new SafeCallerException(
        "Method $0 blocked (INVOKEDYNAMIC not supported)", new String[] {method});
  }

  public static void throwMultiANewArrayOpcodeNotSupported() {
    throw new SafeCallerException(
        "Method $0 blocked (MULTIANEWARRAY not supported)", new String[] {getCallerName()});
  }

  public static void throwCopyArrayTooLarge(int length) {
    throw new SafeCallerException(
        "Method $0 blocked (attempted to copy $1 elements of an array)",
        new String[] {getCallerName(), Integer.toString(length)});
  }

  public static Throwable createNativeMethodNotAllowed(String method) {
    return new SafeCallerException("Native method call $0 is not allowed", new String[] {method});
  }

  /** Determines the binary name of the caller omitting internally generated safe caller methods. */
  private static String getCallerName() {
    return "<unknown>";
  }
}
