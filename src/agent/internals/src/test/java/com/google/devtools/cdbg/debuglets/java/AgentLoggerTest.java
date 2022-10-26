/*
 * Copyright 2022 Google LLC
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

import java.io.IOException;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class AgentLoggerTest {
  private static native String pull();

  static {
    System.loadLibrary("test_logger");
  }

  @Test
  public void info() {
    AgentLogger.info("Hello");
    assertThat(pull()).isEqualTo("[INFO] Hello\n");
  }

  @Test
  public void warn() {
    AgentLogger.warn("Hello");
    assertThat(pull()).isEqualTo("[WARNING] Hello\n");
  }

  @Test
  public void severe() {
    AgentLogger.severe("Hello");
    assertThat(pull()).isEqualTo("[ERROR] Hello\n");
  }

  @Test
  public void infofmt() {
    AgentLogger.infofmt("%d + %s", 15, "orange");
    assertThat(pull()).isEqualTo("[INFO] 15 + orange\n");
  }

  @Test
  public void warnfmt() {
    AgentLogger.warnfmt("%d + %s", 15, "orange");
    assertThat(pull()).isEqualTo("[WARNING] 15 + orange\n");
  }

  @Test
  public void severefmt() {
    AgentLogger.severefmt("%d + %s", 15, "orange");
    assertThat(pull()).isEqualTo("[ERROR] 15 + orange\n");
  }

  @Test
  public void infofmtWithException() {
    AgentLogger.infofmt(new IOException(), "three = %d", 3);
    assertThat(pull())
        .containsMatch("^\\[INFO\\] three = 3\njava.io.IOException\n.*infofmtWithException");
  }

  @Test
  public void warnfmtWithException() {
    AgentLogger.warnfmt(new IOException(), "four = %d", 4);
    assertThat(pull())
        .containsMatch("^\\[WARNING\\] four = 4\njava.io.IOException\n.*warnfmtWithException");
  }

  @Test
  public void severefmtWithException() {
    AgentLogger.severefmt(new IOException(), "five = %d", 5);
    assertThat(pull())
        .containsMatch("^\\[ERROR\\] five = 5\njava.io.IOException\n.*severefmtWithException");
  }

  @Test
  public void nullFormatString() {
    AgentLogger.warnfmt(null, 15, "apple");
    assertThat(pull()).isEqualTo("[WARNING] Failed to format message: \"null\"\n");
  }

  @Test
  public void badFormatString() {
    AgentLogger.warnfmt("a = %d", "no");
    assertThat(pull()).isEqualTo("[WARNING] Failed to format message: \"a = %d\", args: [no]\n");
  }

  @Test
  public void nullException() {
    Throwable t = null;
    AgentLogger.severefmt(t, "a = %d", 4);
    assertThat(pull()).isEqualTo("[ERROR] a = 4\nFailed to format thrown exception\n");
  }
}
