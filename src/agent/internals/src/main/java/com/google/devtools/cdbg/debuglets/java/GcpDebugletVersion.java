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
 * Defines the version of the Java Cloud Debugger agent.
 */
public final class GcpDebugletVersion {
  /**
   * Major version of the debugger.

   * All agents of the same major version are compatible with each other. In other words an
   * application can mix different agents with the same major version within the same debuggee.
   */
  public static final int MAJOR_VERSION = 2;

  /** Minor version of the agent. */
  public static final int MINOR_VERSION = 21;

  /**
   * Debugger agent version string in the format of MAJOR.MINOR.
   */
  public static final String VERSION = String.format("%d.%d", MAJOR_VERSION, MINOR_VERSION);

  /**
   * Main function to print the version string.
   */
  public static void main(String[] args) {
    System.out.println(VERSION);
  }
}
