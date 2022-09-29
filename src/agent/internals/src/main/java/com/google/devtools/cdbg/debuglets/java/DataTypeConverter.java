/**
 * Copyright 2022 Google Inc. All Rights Reserved.
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

/**
 * Utility class providing data conversions.
 */
class DataTypeConverter {
  private static final char[] HEX_DIGITS = "0123456789ABCDEF".toCharArray();

  /** Converts the provided byte array into a hex string. */
  public static String printHexBinary(byte[] data) {
    char[] hex = new char[data.length * 2];
    for (int i = 0, j = 0; i < data.length; i++) {
      byte b = data[i];
      hex[j++] = HEX_DIGITS[(b >> 4) & 0xf];
      hex[j++] = HEX_DIGITS[b & 0xf];
    }
    return new String(hex);
  }
}
