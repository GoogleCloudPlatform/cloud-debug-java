/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.devtools.cdbg.debuglets.java;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import com.google.devtools.build.runfiles.Runfiles;
import java.io.File;
import java.io.IOException;

/**
 * Helper class for tests that need to open a test file.
 *
 * <p>Under Bazel, we are using the 'data' build attribute to specify test files that should be
 * included in the in the runfiles area of the test sandbox. These are files the tests themselves
 * need access to for opening and reading as part of the test execution. Given these files end up in
 * a sandboxed environment where the base root path is not known, we leverage the Bazel Runfiles
 * utility to locate the file so we can obtain the full path.
 *
 * <p>See:
 * https://github.com/bazelbuild/bazel/blob/4e4c77ae96955138006af15664074123d6a4f51d/src/tools/runfiles/java/com/google/devtools/build/runfiles/Runfiles.java#L27
 * The required build dependency is "@bazel_tools//tools/java/runfiles:runfiles"
 */
public class TestDataPath {
  // To note, the '__main__' here is the workspace name. This is the default
  // when it's not explicitly set in the base WORKSPACE file.
  public static final String BASE_PATH =
      "__main__/src/agent/internals/src/test/java/com/google/devtools/cdbg/debuglets/java";

  public static File get(String fileName) {
    File file = null;

    try {
      file = new File(Runfiles.create().rlocation(BASE_PATH + "/" + fileName));
    } catch (IOException e) {
      System.err.println("Runfiles rlocation failed: " + e);
    }

    assertNotNull(file);
    assertTrue(file.exists());
    return file;
  }
}
