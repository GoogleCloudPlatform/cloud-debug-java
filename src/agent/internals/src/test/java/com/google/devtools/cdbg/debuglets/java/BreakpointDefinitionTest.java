/*
 * Copyright 2014 Google LLC
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

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class BreakpointDefinitionTest {

  @Test
  public void resolveLineNumber_findsMultiLineTag() throws BreakpointTagException {
    // Note: This test will fail if lines are added/removed above it.
    // If that happens, update the line number in the assertion.
    assertThat(BreakpointDefinition.resolveLineNumber("BreakpointDefinitionTest.java", "MULTILINE"))
        .isEqualTo(35); /* BPTAG: MULTILINE */
  }

  @Test
  public void resolveLineNumber_findsSingleLineTag() throws BreakpointTagException {
    // Note: This test will fail if lines are added/removed above it.
    // If that happens, update the line number in the assertion.
    assertThat(
            BreakpointDefinition.resolveLineNumber("BreakpointDefinitionTest.java", "SINGLELINE"))
        .isEqualTo(44); // BPTAG: SINGLELINE
  }

  @Test
  public void resolveLineNumber_invalidTag() throws BreakpointTagException {
    assertThrows(
        BreakpointTagException.class,
        () -> BreakpointDefinition.resolveLineNumber("BreakpointDefinitionTest.java", "INVALID"));
  }
}
