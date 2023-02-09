/*
 * Copyright 2014 Google LLC
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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.lang.reflect.Method;
import java.net.MalformedURLException;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/**
 * Verifies {@code InternalsClassLoader} capability to load classes from an externally supplied .jar
 * file. Checks that these classes can coexist with classes that have same name that application
 * defined.
 */
@RunWith(JUnit4.class)
public class InternalsClassLoaderTest {
  /**
   * This class is only used as an input to the product code. It doesn't have any test related
   * logic.
   */
  public static class TestDataSingle1 {
    public static int getMyInt() {
      return myInt;
    }

    public static void setMyInt(int newValue) {
      myInt = newValue;
    }

    public static TestDataSingle2 getSecond() {
      return second;
    }

    private static int myInt = 31;
    private static TestDataSingle2 second = new TestDataSingle2();
  }

  /**
   * This class is only used as an input to the product code. It doesn't have any test related
   * logic.
   */
  public static class TestDataSingle2 {
    public String getMyString() {
      return myString;
    }

    public void setMyString(String newValue) {
      myString = newValue;
    }

    private static String myString = "rabbit";
  }

  private ClassLoader classLoader;

  @Before
  public void setUp() throws MalformedURLException {
    File testsJarFile =
        TestDataPath.get(
            "__main__/src/agent/internals-class-loader/src/test/java/com/google/devtools/cdbg/debuglets/java",
            "InternalsClassLoaderTest.jar");
    classLoader = new InternalsClassLoader(new String[]{testsJarFile.getAbsolutePath()});
  }

  @Test
  public void staticMethodGet() throws Exception {
    TestDataSingle1.setMyInt(57);

    Class<?> classTestDataSingle1 = classLoader.loadClass(TestDataSingle1.class.getName());
    assertNotNull(classTestDataSingle1);

    Method methodGetMyInt = classTestDataSingle1.getMethod("getMyInt");
    assertNotNull(methodGetMyInt);

    assertEquals(57, TestDataSingle1.getMyInt());
    assertEquals(31, methodGetMyInt.invoke(null));
  }

  @Test
  public void staticMethodSetGet() throws Exception {
    TestDataSingle1.getSecond().setMyString("bunny");

    Class<?> classTestDataSingle1 = classLoader.loadClass(TestDataSingle1.class.getName());
    assertNotNull(classTestDataSingle1);

    Class<?> classTestDataSingle2 = classLoader.loadClass(TestDataSingle2.class.getName());
    assertNotNull(classTestDataSingle2);

    Method methodGetSecond = classTestDataSingle1.getMethod("getSecond");
    assertNotNull(methodGetSecond);

    Method methodGetMyString = classTestDataSingle2.getMethod("getMyString");
    assertNotNull(methodGetMyString);

    Object externalSecond = methodGetSecond.invoke(null);
    assertNotNull(externalSecond);

    assertEquals("bunny", TestDataSingle1.getSecond().getMyString());
    assertEquals("rabbit", methodGetMyString.invoke(externalSecond));

    assertTrue(TestDataSingle1.getSecond() != externalSecond);
  }

  @Test
  public void crossClassLoaderCast() throws Exception {
    Class<?> classTestDataSingle1 = classLoader.loadClass(TestDataSingle1.class.getName());
    assertNotNull(classTestDataSingle1);

    Method methodGetSecond = classTestDataSingle1.getMethod("getSecond");
    assertNotNull(methodGetSecond);

    assertThrows(
        ClassCastException.class,
        () -> {
          @SuppressWarnings("unused")
          TestDataSingle2 s = (TestDataSingle2) methodGetSecond.invoke(null);
        });
  }
}
