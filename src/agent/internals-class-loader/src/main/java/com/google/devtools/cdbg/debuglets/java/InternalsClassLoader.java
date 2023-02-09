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

import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;

/**
 * Implements {@link java.lang.ClassLoader} for debuglet internal functionality.
 *
 * <p>This class is compiled in a separate binary file. The internal functionality has to be
 * completely segregated from the application: the application must not be able to reference
 * debuglet functionality by mistake and vice versa. Furthermore since the debuglet internals uses
 * some open source libraries, it is possible that the application is using different versions of
 * the same libraries.
 */
final class InternalsClassLoader extends URLClassLoader {
  /**
   * Class constructor.
   *
   * @param internalsJarPaths required paths to cdbg_java_agent_internals.jar file. This is an array
   * as under AppEngine Standard Java 8, the jar file is expected to be split into multiple files to
   * work around a maximum file size limit of 32MB.
   */
  public InternalsClassLoader(String[] internalsJarPaths) throws MalformedURLException {
    super(convertJarPathsToURLs(internalsJarPaths), null); // Parent class loader
  }

  @Override
  protected synchronized Class<?> loadClass(String name, boolean resolve)
      throws ClassNotFoundException {
    // Debuglet internals have to be self contained and must not reference any classes
    // in the application. The only exception is for built-in Java classes.
    if (name.startsWith("java.")) {
      return getSystemClassLoader().loadClass(name);
    }

    return super.loadClass(name, resolve);
  }

  static URL[] convertJarPathsToURLs(String[] internalsJarPath) throws MalformedURLException {
    URL[] urls = new URL[internalsJarPath.length];
    for (int i = 0; i < urls.length; ++i) {
      urls[i] = new URL("file:" + internalsJarPath[i]);
    }

    return urls;
  }
}
