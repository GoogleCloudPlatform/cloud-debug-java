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

import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * Java portion of NanoJava interpreter unit test.
 *
 * <p>The unit test is implemented in //tests/agent/nanojava_interpreter_test.cc.
 */
public class NanoJavaInterpreterTestLib {
  @SuppressWarnings("unused")
  private static class StaticFields {
    private static boolean booleanStaticField = true;
    private static byte byteStaticField = 123;
    private static char charStaticField = 1234;
    private static short shortStaticField = -12345;
    private static int intStaticField = 1234567;
    private static long longStaticField = 1234567890L;
    private static float floatStaticField = 3.14f;
    private static double doubleStaticField = 3.1415;
    private static String stringStaticField = "hello static";
  }

  @SuppressWarnings("unused")
  private static class InstanceFields {
    private boolean booleanInstanceField;
    private byte byteInstanceField;
    private char charInstanceField;
    private short shortInstanceField;
    private int intInstanceField;
    private long longInstanceField;
    private float floatInstanceField;
    private double doubleInstanceField;
    private String stringInstanceField;
  }

  private static final int[] INT_ARRAY_1K;
  private static final Map<Integer, String> UNMODIFIABLE_MAP;

  static {
    INT_ARRAY_1K = new int[1000];
    for (int i = 0; i < INT_ARRAY_1K.length; ++i) {
      INT_ARRAY_1K[i] = i;
    }

    Map<Integer, String> map = new HashMap<>();
    for (int i = 0; i < 100; ++i) {
      map.put(i, "number " + i);
    }
    UNMODIFIABLE_MAP = Collections.unmodifiableMap(map);
  }

  @SuppressWarnings("InfiniteRecursion")
  public static String infiniteRecursion() {
    return infiniteRecursion();
  }

  public static String sumArray1K() {
    int count = 0;
    for (int i = 0; i < INT_ARRAY_1K.length; ++i) {
      count += INT_ARRAY_1K[i];
    }

    return Integer.toString(count);
  }

  public static String accessUnmodifiableMap() {
    return UNMODIFIABLE_MAP.get(37);
  }

  public static String arrayToString() {
    int[] array = new int[10];
    for (int i = 0; i < 10; ++i) {
      array[i] = 100 + i;
    }
    return Arrays.toString(array);
  }

  public static String stringFormat() {
    Map<Integer, String> map = new HashMap<>();
    map.put(1, "one");
    map.put(2, "two");
    map.put(3, "three");

    return String.format(
        "Integer = %d, float = %f, flag = %b, string = %s, map = %s",
        12345, 3.14f, true, "this-is-some-string", map);
  }
}
