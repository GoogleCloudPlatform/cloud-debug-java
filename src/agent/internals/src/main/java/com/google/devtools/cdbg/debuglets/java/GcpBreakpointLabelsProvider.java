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

import java.util.ArrayList;
import java.util.List;

/**
 * Collects information about the local environment at the breakpoint capture time.
 */
class GcpBreakpointLabelsProvider implements BreakpointLabelsProvider {
  @Override
  public String[] format() {
    List<String> labels = new ArrayList<>();
    
    // agentversion=MAJOR.MINOR
    labels.add("agentversion");
    labels.add(GcpDebugletVersion.VERSION);
    
    return labels.toArray(new String[0]);
  }
}
