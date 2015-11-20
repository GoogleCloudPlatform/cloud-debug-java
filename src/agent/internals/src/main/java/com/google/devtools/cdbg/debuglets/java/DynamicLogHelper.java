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

import java.util.logging.Level;
import java.util.logging.Logger;

/**
 * Helper class to use java.util.logging.
 * 
 * <p>The main reason this class exists is that native code can't call
 * {@link Logger#getLogger(String)}. The call just fails with {@link NullPointerException}
 * because the caller class is not defined.
 */
final class DynamicLogHelper {
  private static final String CLOUD_DEBUGGER_LOGGER_NAME = "com.google.DynamicLog";

  public static Logger getLogger() {
    return Logger.getLogger(CLOUD_DEBUGGER_LOGGER_NAME);
  }
  
  public static Level getInfoLevel() {
    return Level.INFO;
  }

  public static Level getWarningLevel() {
    return Level.WARNING;
  }

  public static Level getSevereLevel() {
    return Level.SEVERE;
  }
}

