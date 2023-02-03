/*
 * Copyright 2014 Google LLC
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

import com.google.common.io.Files;
import com.google.common.io.LineReader;
import com.google.devtools.clouddebugger.v3.Breakpoint;
import com.google.devtools.clouddebugger.v3.SourceLocation;
import java.io.BufferedReader;
import java.io.File;
import java.io.IOError;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;

/**
 * Helper class to resolve location in the test to a breakpoint definition that Java debuglet
 * accepts. The problem that {@code BreakpointDefinition} is solving is how to specify line numbers.
 * Line numbers are very volatile and get changed following small code changes. As a result it is
 * impractical to hardcode line numbers in code. The solution is to copy all source files as data
 * (see BUILD file). Each line of source code where we want to set a breakpoint is annotated with a
 * specially crafted comment that obviously doesn't affect the code (see example below). Given the
 * source file and a tag name, the {@code BreakpointDefinition} reads the source file and finds the
 * line with this special comment. Then it defines the Breakpoint message with that file name and
 * line number.
 */
// Example of a source line with tag:
//    System.out.println("Hello");  /* BPTAG: PRINTHELLO */
public class BreakpointDefinition {
  /**
   * Prefix for file names in breakpoint definition. The path has to contain the entire package
   * path, but it doesn't have to be rooted there.
   */
  public static final String DEFAULT_BASE_PATH =
      "src/agent/internals/src/test/java/com/google/devtools/cdbg/debuglets/java";

  /** Global counter of breakpoints to assign unique ID for every new breakpoint definition. */
  private static int breakpointIdCounter = 1;

  /** Creates a breakpoint builder with the specified location and allocates it a unique ID. */
  public static Breakpoint.Builder createBuilder(String fileName, String tag)
      throws BreakpointTagException {
    Breakpoint.Builder builder = Breakpoint.newBuilder();
    builder.setId(buildUniqueBreakpointId());
    builder.setLocation(resolveLocation(fileName, tag));

    return builder;
  }

  /** Generates a unique breakpoint ID. */
  public static String buildUniqueBreakpointId() {
    return "BP_" + breakpointIdCounter++;
  }

  /** Gets the line number on which the specified breakpoint tag is defined. */
  public static int resolveLineNumber(String fileName, String tag) throws BreakpointTagException {
    try {
      File sourceFile = TestDataPath.get(fileName);
      BufferedReader reader = Files.newReader(sourceFile, StandardCharsets.UTF_8);

      try {
        LineReader lineReader = new LineReader(reader);
        String line;
        int lineNumber = 1;
        ArrayList<Integer> matches = new ArrayList<>();
        while ((line = lineReader.readLine()) != null) {
          if (line.contains("/* BPTAG: " + tag + " */") || line.endsWith("// BPTAG: " + tag)) {
            matches.add(lineNumber);
          }

          lineNumber++;
        }

        switch (matches.size()) {
          case 1:
            return matches.get(0);
          case 0:
            throw new BreakpointTagException(fileName, tag);
          default:
            throw new BreakpointTagException(fileName, tag, matches);
        }
      } finally {
        reader.close();
      }
    } catch (IOException | IOError e) {
      System.err.println("JCB ERROR: " + e);
      throw new BreakpointTagException(fileName);
    }
  }

  private static SourceLocation resolveLocation(String filePath, String tag)
      throws BreakpointTagException {
    SourceLocation.Builder locationBuilder = SourceLocation.newBuilder();

    int pos = filePath.lastIndexOf('/');
    if (pos == -1) {
      locationBuilder.setPath(DEFAULT_BASE_PATH + '/' + filePath);
      locationBuilder.setLine(resolveLineNumber(filePath, tag));
    } else {
      locationBuilder.setPath(filePath);
      locationBuilder.setLine(resolveLineNumber(filePath.substring(pos + 1), tag));
    }

    return locationBuilder.build();
  }
}
