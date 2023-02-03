/*
 * Copyright 2023 Google LLC
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

import com.google.common.base.Joiner;
import java.util.List;

/**
 * Exception class thrown by the test case when the breakpoint tag can't be resolved to a line
 * number. This can happen when either the source .Java file can't be opened or when the tag string
 * can't be found in the source file.
 */
public class BreakpointTagException extends Exception {
  public BreakpointTagException(String fileName) {
    super(String.format("Source file %s not found", fileName));
  }

  public BreakpointTagException(String fileName, String tag) {
    super(String.format("Breakpoint tag %s not found in %s", tag, fileName));
  }

  public BreakpointTagException(String fileName, String tag, List<Integer> matches) {
    super(
        String.format(
            "Duplicate breakpoint tag %s in file %s (lines: %s)",
            tag, fileName, Joiner.on(", ").join(matches)));
  }
}
