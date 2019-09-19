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

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.jar.JarEntry;
import java.util.jar.JarInputStream;

/**
 * Custom class loader over .jar file embedded as a static resource in debuglet .so file.
 *
 * <p>We don't define constructor for {@link EmbedClassLoader}. This makes system class loader its
 * parent.
 */
final class EmbedClassLoader extends ClassLoader {
  private final byte[] jarData;
  Map<String, JarEntry> jarEntries = new HashMap<>();
  /**
   * Class constructor.
   *
   * <p>Default constructor of {@link ClassLoader} makes system class loader its parent.
   *
   * @param jar byte array of JAR file content.
   * @throws IOException if the JAR file is corrupted.
   */
  public EmbedClassLoader(byte[] jarData) throws IOException {
    this.jarData = jarData;
  }

  @Override
  protected Class<?> findClass(String name) throws ClassNotFoundException {
    // The "name" is a Java class name (like "java.lang.String"). JAR file entries
    // are binary names with ".class" suffix (like "java/lang/String.class").
    String resourceName = name.replace('.', '/') + ".class";

    // We have to open the .jar file every time and loop through it. This is not too
    // efficient, but the embedded classes are fairly small and self contained.
    try (JarInputStream jar = new JarInputStream(new ByteArrayInputStream(jarData))) {
      JarEntry entry;
      do {
        entry = jar.getNextJarEntry();
        if ((entry != null) && !entry.isDirectory() && entry.getName().equals(resourceName)) {
          byte[] buffer = new byte[1024];
          int offset = 0;
          int rc;
          while ((rc = jar.read(buffer, offset, buffer.length - offset)) != -1) {
            offset += rc;
            if (offset == buffer.length) {
              byte[] newBuffer = new byte[buffer.length * 2];
              System.arraycopy(buffer, 0, newBuffer, 0, buffer.length);
              buffer = newBuffer;
            }
          }

          return defineClass(name, buffer, 0, offset);
        }
      } while (entry != null);
    } catch (IOException e) {
      throw new ClassNotFoundException("Failed to iterate through embedded JAR", e);
    }

    return super.findClass(name);
  }
}
