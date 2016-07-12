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
 * Collects information about the local environment at the breakpoint capture time.
 *
 * The class implementing this interface MUST collect the actual data in constructor, called by
 * the thread hitting the brakpoint. The captured data is then formatted later when
 * {@link BreakpointLabelsProvider#format} is called on anohter thread. A new instance
 * of this class will be created for each breakpoint.
 *
 * TODO(gigid): consider changing to avoid doing work in a constructor.
 *     e.g., introduce a static method to create the object and capture the labels.
 */
interface BreakpointLabelsProvider {
  /**
   * Formats the breakpoint labels per data collected at constructor.
   *
   * This function is called from debugger worker thread.
   *
   * @return list of labels, where even entries are keys and odd entries are values. For example
   * {a:b,c:d} will be represeted as [a,b,c,d]. Flat lists are much easier to marshal in C++.
   */
  String[] format();
}
