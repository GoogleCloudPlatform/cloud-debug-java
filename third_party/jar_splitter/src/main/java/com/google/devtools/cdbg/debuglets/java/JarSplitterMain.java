/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.devtools.cdbg.debuglets.java;

import java.io.File;
import java.io.IOException;
import java.util.Collections;

public class JarSplitterMain {
  public static void main(String[] args) {
    if (args.length < 2) {
      System.err.println(
          "JarSplitter requires 2 arguments, the jar file to split and the location to place the"
              + " split files.");
      System.exit(1);
    }

    // Taking default values for JarSplitter constructor based on
    // https://github.com/GoogleCloudPlatform/appengine-java-standard/blob/6b724ba9e2a181ba55e07a9daf28105dc11be16d/lib/tools_api/src/main/java/com/google/appengine/tools/admin/Application.java#L1494
    try {
      File jarFile = new File(args[0]);
      File outputDirectory = new File(args[1]);

      // App Engine Standard Java 8 environment has a maximum file size limit of 32M for deployed
      // files.
      int maxJarSize = 32000000;
      if (args.length > 2) {
        maxJarSize = Integer.parseInt(args[2]);
      }

      new com.google.appengine.tools.util.JarSplitter(
              jarFile,
              outputDirectory,
              maxJarSize,
              false, /* replicateManifests */
              4, /* outputDigits */
              Collections.<String>emptySet() /* excludes */)
          .run();
    } catch (IOException e) {
      System.err.println("JarSplitter failed: " + e);
      System.exit(1);
    }
  }
}
