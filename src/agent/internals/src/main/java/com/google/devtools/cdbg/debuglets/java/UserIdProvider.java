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
 * Collects information about the identity of the end user at the breakpoint capture time.
 *
 * The class implementing this interface MUST collect the actual data in constructor, called by
 * the thread hitting the breakpoint. The captured data is then formatted later when
 * {@link UserIdProvider#format} is called on another thread. A new instance of this class will
 * be created for each breakpoint.
 */
interface UserIdProvider {
  /**
   * Formats the end user identity data collected at constructor.
   *
   * This function is called from debugger worker thread.
   *
   * @return a pair where the first entry is the kind of identity (e.g., GAIA id) and the second
   * entry is the actual identity data (e.g., user name), or null if end user id was not captured.
   */
  String[] format();
}
