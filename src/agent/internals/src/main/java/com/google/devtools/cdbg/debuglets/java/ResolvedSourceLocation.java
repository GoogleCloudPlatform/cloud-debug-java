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

/**
 * Holds the result of resolving source location.
 */
final class ResolvedSourceLocation {
  private final FormatMessage errorMessage;
  private final String classSignature;
  private final String methodName;
  private final String methodDescriptor;
  private final int adjustedLineNumber;

  /**
   * Class constructor for successfully resolved source location.
   */
  public ResolvedSourceLocation(
      String classSignature,
      String methodName,
      String methodDescriptor,
      int adjustedLineNumber) {
    this.errorMessage = null;
    this.classSignature = classSignature;
    this.methodName = methodName;
    this.methodDescriptor = methodDescriptor;
    this.adjustedLineNumber = adjustedLineNumber;
  }

  /**
   * Class constructor for failure to resolve source location.
   */
  public ResolvedSourceLocation(FormatMessage errorMessage) {
    this.errorMessage = errorMessage;
    this.classSignature = null;
    this.methodName = null;
    this.methodDescriptor = null;
    this.adjustedLineNumber = -1;
  }

  /**
   * Returns null if source location was resolved successfully or a parameterized string with
   * error message otherwise.
   *
   * <p>The user of this class should call {@code getErrorMessage} first to determine whether
   * the source location resolution was successful. If it wasn't other methods of this class
   * should not be called.
   */
  public FormatMessage getErrorMessage() {
    return errorMessage;
  }

  /**
   * Class signature (like com/prod/MyClass$MyInnerClass) referenced by the source location.
   */
  public String getClassSignature() {
    return classSignature;
  }

  /**
   * Name of the method containing code referenced by the source location.
   */
  public String getMethodName() {
    return methodName;
  }

  /**
   * Descriptor of the method containing code referenced by the source location.
   */
  public String getMethodDescriptor() {
    return methodDescriptor;
  }

  /**
   * Line number in the source .java file used to compile the class. The line might not be the same
   * one as specified in source location if it references a statement spanning multiple lines.
   */
  public int getAdjustedLineNumber() {
    return adjustedLineNumber;
  }
}
