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

// Needed to wrap the calls to the native logging methods which may fail for unit tests of classes
// that use the AgentLogger, since there won't be a .so loaded that provides the native methods.
import java.lang.UnsatisfiedLinkError;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.Arrays;
import java.util.IllegalFormatException;

/**
 * Static class to log diagnostic messages.
 *
 * <p>We want to have a single log file for messages coming from C++ code as well as from Java code.
 * Hence this class just sends log messages to the C++ code, that logs messages with glog library.
 *
 * <p>This class has to be public because it is used by classes loaded by another class loader.
 */
public final class AgentLogger {
  public static native void info(String message);

  public static native void warn(String message);

  public static native void severe(String message);

  public static void infofmt(Throwable thrown, String message, Object... args) {
    try {
      info(formatSafely(thrown, message, args));
    }
    catch (UnsatisfiedLinkError e) {
    }
  }

  public static void warnfmt(Throwable thrown, String message, Object... args) {
    try {
      warn(formatSafely(thrown, message, args));
    }
    catch (UnsatisfiedLinkError e) {
    }
  }

  public static void severefmt(Throwable thrown, String message, Object... args) {
    try {
      severe(formatSafely(thrown, message, args));
    }
    catch (UnsatisfiedLinkError e) {
    }
  }

  public static void infofmt(String message, Object... args) {
    try {
      info(formatSafely(message, args));
    }
    catch (UnsatisfiedLinkError e) {
    }
  }

  public static void warnfmt(String message, Object... args) {
    try {
      warn(formatSafely(message, args));
    }
    catch (UnsatisfiedLinkError e) {
    }
  }

  public static void severefmt(String message, Object... args) {
    try {
      severe(formatSafely(message, args));
    }
    catch (UnsatisfiedLinkError e) {
    }
  }

  /**
   * Safely formats a message string with exception and gaurantees not to throw an exception, but
   * instead returns a slightly different message.
   *
   * @param thrown exception to format
   * @param fmt the format string
   * @param args array of parameters for the format string
   */
  private static String formatSafely(Throwable thrown, String fmt, Object... args) {
    return formatSafely(fmt, args) + "\n" + formatSafely(thrown);
  }

  /**
   * Safely formats a string with {@link String#format(String, Object[])}, and gaurantees not to
   * throw an exception, but instead returns a slightly different message.
   *
   * @param fmt the format string
   * @param args array of parameters for the format string
   */
  private static String formatSafely(String fmt, Object... args) {
    try {
      try {
        return String.format(fmt, args);
      } catch (IllegalFormatException e) {
        return String.format(
            "Failed to format message: \"%s\", args: %s",
            fmt, (args != null) ? Arrays.toString(args) : "null");
      }
    } catch (Exception e) {
      // such as a failure during toString() on one of the arguments
      return String.format("Failed to format message: \"%s\"", fmt);
    }
  }

  /**
   * Safely formats the exception with call stack and gaurantees not to throw an exception, but
   * instead returns a slightly different message.
   *
   * @param thrown exception to format
   */
  private static String formatSafely(Throwable thrown) {
    try {
      StringWriter stringWriter = new StringWriter();
      PrintWriter printWriter = new PrintWriter(stringWriter);
      thrown.printStackTrace(printWriter);
      printWriter.flush();
      return stringWriter.toString();
    } catch (Exception e) {
      return "Failed to format thrown exception";
    }
  }
}
