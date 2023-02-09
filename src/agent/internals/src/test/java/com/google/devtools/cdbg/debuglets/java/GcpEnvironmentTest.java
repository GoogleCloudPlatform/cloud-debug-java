/*
 * Copyright 2017 Google LLC
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

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import junitparams.JUnitParamsRunner;
import junitparams.Parameters;
import junitparams.converters.Nullable;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit test for {@link GcpEnvironment}. */
@RunWith(JUnitParamsRunner.class)
public class GcpEnvironmentTest {
  private static class TestEnvironmentStore implements GcpEnvironment.EnvironmentStore {
    Map<String, String> overrides = new HashMap<>();

    @Override
    public String get(String name) {
      if (overrides.containsKey(name)) {
        return overrides.get(name);
      }
      return System.getenv(name);
    }

    public void set(String name, String value) {
      overrides.put(name, value);
    }
  }

  private TestEnvironmentStore environmentStore;
  private GcpEnvironment.EnvironmentStore oldEnvironmentStore;
  String oldModule;
  String oldVersion;

  @Before
  public void setUp() throws Exception {
    environmentStore = new TestEnvironmentStore();
    oldEnvironmentStore = GcpEnvironment.environmentStore;
    GcpEnvironment.environmentStore = environmentStore;
    oldModule = System.getProperty("com.google.cdbg.module");
    oldVersion = System.getProperty("com.google.cdbg.version");
    System.clearProperty("com.google.cdbg.module");
    System.clearProperty("com.google.cdbg.version");
    System.clearProperty("com.google.cdbg.auth.serviceaccount.jsonfile");
    GcpEnvironment.metadataQuery = null;
  }

  @After
  public void cleanup() throws Exception {
    GcpEnvironment.environmentStore = oldEnvironmentStore;
    if (oldModule == null) {
      System.clearProperty("com.google.cdbg.module");
    } else {
      System.setProperty("com.google.cdbg.module", oldModule);
    }
    if (oldVersion == null) {
      System.clearProperty("com.google.cdbg.version");
    } else {
      System.setProperty("com.google.cdbg.version", oldVersion);
    }
  }

  @Test
  public void newGaeEnvironmentVariables() throws Exception {
    environmentStore.set("GAE_SERVICE", "NewService");
    environmentStore.set("GAE_VERSION", "NewVersion");
    environmentStore.set("GAE_DEPLOYMENT_ID", "NewMinor");
    Map<String, String> labels = GcpEnvironment.getDebuggeeLabels();
    assertThat(labels.get(Labels.Debuggee.MODULE)).isEqualTo("NewService");
    assertThat(labels.get(Labels.Debuggee.VERSION)).isEqualTo("NewVersion");
    assertThat(labels.get(Labels.Debuggee.MINOR_VERSION)).isEqualTo("NewMinor");
  }

  @Test
  public void oldGaeEnvironmentVariables() throws Exception {
    environmentStore.set("GAE_MODULE_NAME", "OldService");
    environmentStore.set("GAE_MODULE_VERSION", "OldVersion");
    environmentStore.set("GAE_MINOR_VERSION", "OldMinor");
    Map<String, String> labels = GcpEnvironment.getDebuggeeLabels();
    assertThat(labels.get(Labels.Debuggee.MODULE)).isEqualTo("OldService");
    assertThat(labels.get(Labels.Debuggee.VERSION)).isEqualTo("OldVersion");
    assertThat(labels.get(Labels.Debuggee.MINOR_VERSION)).isEqualTo("OldMinor");
  }

  @Test
  public void propertyOverrides() throws Exception {
    environmentStore.set("GAE_MODULE_NAME", "OldService");
    environmentStore.set("GAE_MODULE_VERSION", "OldVersion");
    environmentStore.set("GAE_MINOR_VERSION", "OldMinor");
    System.setProperty("com.google.cdbg.module", "OverrideModule");
    System.setProperty("com.google.cdbg.version", "OverrideVersion");
    Map<String, String> labels = GcpEnvironment.getDebuggeeLabels();
    assertThat(labels.get(Labels.Debuggee.MODULE)).isEqualTo("OverrideModule");
    assertThat(labels.get(Labels.Debuggee.VERSION)).isEqualTo("OverrideVersion");
    assertThat(labels.get(Labels.Debuggee.MINOR_VERSION)).isEqualTo("OldMinor");
  }

  @Test
  public void cloudRunEnvironmentVariables() throws Exception {
    environmentStore.set("K_SERVICE", "CloudRunService");
    environmentStore.set("K_REVISION", "CloudRunRevision");
    Map<String, String> labels = GcpEnvironment.getDebuggeeLabels();
    assertThat(labels.get(Labels.Debuggee.MODULE)).isEqualTo("CloudRunService");
    assertThat(labels.get(Labels.Debuggee.VERSION)).isEqualTo("CloudRunRevision");
  }

  @Test
  public void gceMetadataQuery() throws Exception {
    assertThat(GcpEnvironment.getMetadataQuery()).isInstanceOf(GceMetadataQuery.class);
  }

  @Test
  public void serviceAccountMetadataQuerySystemProperty() throws Exception {
    System.setProperty("com.google.cdbg.auth.serviceaccount.jsonfile", "/file_does_not_exist.txt");
    Exception ex = assertThrows(SecurityException.class, GcpEnvironment::getMetadataQuery);
    assertThat(ex)
        .hasMessageThat()
        .isEqualTo("Failed to initialize service account authentication");
    assertThat(ex).hasCauseThat().isInstanceOf(IOException.class);
    assertThat(ex)
        .hasCauseThat()
        .hasMessageThat()
        .isEqualTo("/file_does_not_exist.txt (No such file or directory)");
  }

  @Test
  public void serviceAccountMetadataQueryEnvironment() throws Exception {
    environmentStore.set("GOOGLE_APPLICATION_CREDENTIALS", "/file_does_not_exist.txt");
    Exception ex = assertThrows(SecurityException.class, GcpEnvironment::getMetadataQuery);
    assertThat(ex)
        .hasMessageThat()
        .isEqualTo("Failed to initialize service account authentication");
    assertThat(ex).hasCauseThat().isInstanceOf(IOException.class);
    assertThat(ex)
        .hasCauseThat()
        .hasMessageThat()
        .isEqualTo("/file_does_not_exist.txt (No such file or directory)");
  }

  @Test
  @Parameters({
    "null|/foo|null",
    "cdbg_java_agent.so|/foo|/foo",
    "cdbg_java_agent.so|/foo/|/foo",
    "bar/cdbg_java_agent.so|/foo/bar|/foo",
    "bar/cdbg_java_agent.so|/foo/bar/|/foo",
    "cdbg_java_agent.so=use_firebase=true\\,--logtostderr=true|foo|foo",
    "r1/r2/r3/cdbg_java_agent.so=use_firebase=true\\,--logtostderr=true|/w1/w2/w3/r1/r2/r3|/w1/w2/w3",
    "f1/f2/f3/cdbg_java_agent.so|/f1/f2/f3/f1/f2/f3/f1/f2/f3|/f1/f2/f3/f1/f2/f3",
    "r1/foo.so|/w1/r1|/w1",
    "r1/foo.so|/w1/r2|null",
    "r1/agent1.so r2/agent2.so|/w1/r2/|/w1",
    "r2/agent1.so r1/agent2.so|/w1/r2/|/w1",
    "r1/agent1.so=p1=true r2/agent2.so=p2=true|/w1/r2/|/w1",
    "r1/agent1.so=p1=true r2/agent2.so=p2=true|/w1/r2/|/w1",
    "  r1/agent1.so      r2/agent2.so     |/w1/r2/|/w1",
  })
  public void getAppEngineJava8UserDir(
      @Nullable String agentPathOpts, String agentDir, @Nullable String expectedResult) throws Exception {
    environmentStore.set("GAE_AGENTPATH_OPTS", agentPathOpts);
    assertThat(GcpEnvironment.getAppEngineJava8UserDir(agentDir)).isEqualTo(expectedResult);
  }

  private static void setAndTestCanaryMode(
      String enableCanary,
      String allowCanaryOverride,
      GcpEnvironment.DebuggeeCanaryMode expectedCanaryMode)
      throws Exception {
    try {
      if (enableCanary != null) {
        System.setProperty("com.google.cdbg.breakpoints.enable_canary", enableCanary);
      }
      if (allowCanaryOverride != null) {
        System.setProperty(
            "com.google.cdbg.breakpoints.allow_canary_override", allowCanaryOverride);
      }
      assertThat(GcpEnvironment.getDebuggeeCanaryMode()).isEqualTo(expectedCanaryMode);
    } finally {
      if (enableCanary != null) {
        System.clearProperty("com.google.cdbg.breakpoints.enable_canary");
      }
      if (allowCanaryOverride != null) {
        System.clearProperty("com.google.cdbg.breakpoints.allow_canary_override");
      }
    }
  }

  @Test
  public void getCanaryModeWhenPropertiesSet() throws Exception {
    // Verify that when the corresponding properties are set, the canary mode value is not the
    // default "UNSPECIFIED" value.
    setAndTestCanaryMode(
        /*enableCanary=*/ "true",
        /*allowCanaryOverride=*/ "true",
        /*expectedCanaryMode=*/ GcpEnvironment.DebuggeeCanaryMode.CANARY_MODE_DEFAULT_ENABLED);
    setAndTestCanaryMode(
        /*enableCanary=*/ "true",
        /*allowCanaryOverride=*/ "false",
        /*expectedCanaryMode=*/ GcpEnvironment.DebuggeeCanaryMode.CANARY_MODE_ALWAYS_ENABLED);
    setAndTestCanaryMode(
        /*enableCanary=*/ "true",
        /*allowCanaryOverride=*/ null,
        /*expectedCanaryMode=*/ GcpEnvironment.DebuggeeCanaryMode.CANARY_MODE_ALWAYS_ENABLED);
    setAndTestCanaryMode(
        /*enableCanary=*/ "false",
        /*allowCanaryOverride=*/ "true",
        /*expectedCanaryMode=*/ GcpEnvironment.DebuggeeCanaryMode.CANARY_MODE_DEFAULT_DISABLED);
    setAndTestCanaryMode(
        /*enableCanary=*/ "false",
        /*allowCanaryOverride=*/ "false",
        /*expectedCanaryMode=*/ GcpEnvironment.DebuggeeCanaryMode.CANARY_MODE_ALWAYS_DISABLED);
  }

  @Test
  public void getCanaryModeWhenPropertiesNotSet() throws Exception {
    // Verify that when the any of the properties are not set, the canary mode value is the default
    // "UNSPECIFIED" value.
    setAndTestCanaryMode(
        /*enableCanary=*/ null,
        /*allowCanaryOverride=*/ "true",
        /*expectedCanaryMode=*/ GcpEnvironment.DebuggeeCanaryMode.CANARY_MODE_UNSPECIFIED);
    setAndTestCanaryMode(
        /*enableCanary=*/ null,
        /*allowCanaryOverride=*/ null,
        /*expectedCanaryMode=*/ GcpEnvironment.DebuggeeCanaryMode.CANARY_MODE_UNSPECIFIED);
  }
}
