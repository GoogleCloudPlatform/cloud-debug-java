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
import static java.nio.charset.StandardCharsets.UTF_8;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import com.google.common.collect.ImmutableMap;
import com.google.gson.JsonElement;
import com.google.gson.JsonParser;
import com.google.gson.JsonSyntaxException;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.UnsupportedEncodingException;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLConnection;
import java.net.URLStreamHandler;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Objects;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

/** Unit test for {@link GcpHubClient}. */
@RunWith(JUnit4.class)
public class GcpHubClientTest {
  static interface MockServer {
    HttpURLConnection openConnection(String path);
  }

  private static final ImmutableMap<String, String> DEFAULT_LABELS =
      ImmutableMap.<String, String>builder()
          .put("module", "default")
          .put("version", "v1")
          .put("minorversion", "12345")
          .buildOrThrow();

  private static final String EXTERNAL_REGISTER_DEBUGGEE_MESSAGE =
      "{\"debuggee\":{"
          + "\"project\":\"123456789\","
          + "\"uniquifier\":\"we-are-not-going-to-compute-that\","
          + "\"description\":\"someone-else-will-generate-this-file-for-us\","
          + "\"agentVersion\":\"google.com/unit-test/@123456\","
          + "\"sourceContexts\":[],"
          + "\"canaryMode\":\"CANARY_MODE_DEFAULT_ENABLED\""
          + "}}";

  /** Useful empty extra debuggee labels map to use for registerDebuggee calls. */
  private static final ImmutableMap<String, String> EMPTY_EXTRA_DEBUGGEE_LABELS =
      ImmutableMap.<String, String>of();

  @Rule public TemporaryFolder folder1 = new TemporaryFolder();
  @Rule public TemporaryFolder folder2 = new TemporaryFolder();
  @Rule public TemporaryFolder folder3 = new TemporaryFolder();
  @Rule public TemporaryFolder folder4 = new TemporaryFolder();
  @Rule public TemporaryFolder folder5 = new TemporaryFolder();
  @Rule public final MockitoRule mocks = MockitoJUnit.rule();

  @Mock private MockServer mockServer;

  private MetadataQuery metadata;

  private URL baseUrl;

  private ClassPathLookup classPathLookup = new ClassPathLookup(false, null);

  /** Body of last RegisterDebuggee request. */
  private String registerDebuggeeRequest;

  @Before
  public void setUp() throws MalformedURLException {
    baseUrl =
        new URL(
            "mock",
            "debuglet.controller",
            80,
            "/",
            new URLStreamHandler() {
              @Override
              protected URLConnection openConnection(URL url) throws IOException {
                String base = "mock://debuglet.controller:80/";
                assertThat(url.toString()).startsWith(base);

                return Objects.requireNonNull(
                    mockServer.openConnection(url.toString().substring(base.length())));
              }
            });

    metadata =
        new MetadataQuery() {
          @Override
          public String getProjectNumber() {
            return "123456789";
          }

          @Override
          public String getProjectId() {
            return "mock-project-id";
          }

          @Override
          public String getAccessToken() {
            return "ya29";
          }

          @Override
          public void shutdown() {}
        };
  }

  @Test
  public void registerDebuggeeSuccess() throws Exception {
    registerDebuggee();
    assertThat(registerDebuggeeRequest)
        .matches(
            "\\{\"debuggee\":\\{"
                + "\"project\":\"123456789\","
                + "\"uniquifier\":\"[0-9A-F]+\","
                + "\"description\":\"mock-project-id-default-v1-12345\","
                + "\"labels\":\\{"
                + "\"minorversion\":\"12345\","
                + "\"module\":\"default\","
                + "\"version\":\"v1\"\\},"
                + "\"agentVersion\":"
                + "\"google.com/java-gcp/@[0-9]+\","
                + "\"sourceContexts\":\\[\\],"
                + "\"canaryMode\":\"CANARY_MODE_UNSPECIFIED\""
                + "\\}\\}");
  }

  @Test
  public void registerDebuggeeWithExtraLabels() throws Exception {
    // Ensure the extra labels provided in the register call coexist with the base labels provided
    // in the constructor.
    ImmutableMap<String, String> extraLabels =
        ImmutableMap.<String, String>of("afoo", "abar", "mkfoo", "mkbar", "sfoo", "sbar");

    registerDebuggee(extraLabels);

    assertThat(registerDebuggeeRequest)
        .matches(
            "\\{\"debuggee\":\\{"
                + "\"project\":\"123456789\","
                + "\"uniquifier\":\"[0-9A-F]+\","
                + "\"description\":\"mock-project-id-default-v1-12345\","
                + "\"labels\":\\{"
                + "\"afoo\":\"abar\","
                + "\"minorversion\":\"12345\","
                + "\"mkfoo\":\"mkbar\","
                + "\"module\":\"default\","
                + "\"sfoo\":\"sbar\","
                + "\"version\":\"v1\"\\},"
                + "\"agentVersion\":"
                + "\"google.com/java-gcp/@[0-9]+\","
                + "\"sourceContexts\":\\[\\],"
                + "\"canaryMode\":\"CANARY_MODE_UNSPECIFIED\""
                + "\\}\\}");
  }

  @Test
  public void registerDebuggeeFromFile() throws Exception {
    File file = folder1.newFile("register-debuggee-message.json");
    writeContent(file, EXTERNAL_REGISTER_DEBUGGEE_MESSAGE);

    try {
      System.setProperty("com.google.cdbg.debuggee.file", file.toString());
      registerDebuggee();
      assertThat(registerDebuggeeRequest).isEqualTo(EXTERNAL_REGISTER_DEBUGGEE_MESSAGE);
    } finally {
      System.clearProperty("com.google.cdbg.debuggee.file");
    }
  }

  @Test
  public void registerDebuggeeFromString() throws Exception {
    try {
      System.setProperty("com.google.cdbg.debuggee.json", EXTERNAL_REGISTER_DEBUGGEE_MESSAGE);
      registerDebuggee();
      assertThat(registerDebuggeeRequest).isEqualTo(EXTERNAL_REGISTER_DEBUGGEE_MESSAGE);
    } finally {
      System.clearProperty("com.google.cdbg.debuggee.json");
    }
  }

  @Test
  public void registerDebuggeeDisabled() throws Exception {
    HttpURLConnection connection = mock(HttpURLConnection.class);
    when(connection.getOutputStream()).thenReturn(new ByteArrayOutputStream());
    when(connection.getInputStream())
        .thenReturn(streamFromString("{\"debuggee\":{\"id\":\"A\",\"isDisabled\":true}}"));

    when(mockServer.openConnection("debuggees/register")).thenReturn(connection);

    assertThat(new GcpHubClient(baseUrl, metadata, classPathLookup, DEFAULT_LABELS)
                   .registerDebuggee(EMPTY_EXTRA_DEBUGGEE_LABELS))
        .isFalse();
  }

  @Test(expected = IOException.class)
  public void registerDebuggeeFailure() throws Exception {
    HttpURLConnection connection = mock(HttpURLConnection.class);
    when(connection.getInputStream()).thenThrow(new IOException());
    when(connection.getOutputStream()).thenReturn(new ByteArrayOutputStream());
    when(connection.getErrorStream()).thenReturn(new ByteArrayInputStream("error".getBytes(UTF_8)));

    when(mockServer.openConnection("debuggees/register")).thenReturn(connection);

    new GcpHubClient(baseUrl, metadata, classPathLookup, DEFAULT_LABELS)
        .registerDebuggee(EMPTY_EXTRA_DEBUGGEE_LABELS);
  }

  @Test
  public void registerDebuggeeDescription() throws Exception {
    // Map labels to expected debuggee description.
    Map<Map<String, String>, String> testCases =
        ImmutableMap.<Map<String, String>, String>builder()
            .put(ImmutableMap.<String, String>of(), "mock-project-id")
            .put(
                ImmutableMap.<String, String>of(
                    GcpEnvironment.DEBUGGEE_DESCRIPTION_SUFFIX_LABEL, "-suffix"),
                "mock-project-id-suffix")
            .put(ImmutableMap.<String, String>of("module", "mymodule"), "mock-project-id-mymodule")
            .put(ImmutableMap.<String, String>of("version", "v1"), "mock-project-id-v1")
            .put(
                ImmutableMap.<String, String>builder()
                    .put("module", "mymodule")
                    .put("version", "v1")
                    .buildOrThrow(),
                "mock-project-id-mymodule-v1")
            .put(
                ImmutableMap.<String, String>builder()
                    .put("module", "mymodule")
                    .put("version", "v1")
                    .put("minorversion", "37849523874")
                    .buildOrThrow(),
                "mock-project-id-mymodule-v1-37849523874")
            .buildOrThrow();

    for (Entry<Map<String, String>, String> testCase : testCases.entrySet()) {
      ByteArrayOutputStream requestStream = new ByteArrayOutputStream();
      HttpURLConnection connection = mock(HttpURLConnection.class);
      when(connection.getOutputStream()).thenReturn(requestStream);
      when(connection.getInputStream())
          .thenReturn(streamFromString("{\"debuggee\":{\"id\":\"whatever\"}}"));

      when(mockServer.openConnection("debuggees/register")).thenReturn(connection);

      assertThat(new GcpHubClient(baseUrl, metadata, classPathLookup, testCase.getKey())
                     .registerDebuggee(EMPTY_EXTRA_DEBUGGEE_LABELS))
          .isTrue();

      JsonElement request = JsonParser.parseString(new String(requestStream.toByteArray(), UTF_8));
      assertThat(
              request
                  .getAsJsonObject()
                  .get("debuggee")
                  .getAsJsonObject()
                  .get("description")
                  .getAsJsonPrimitive()
                  .getAsString())
          .isEqualTo(testCase.getValue());
    }
  }

  @Test
  public void registerDebuggeeWithSourceContext() throws Exception {
    // These source context files are directly on the class path.
    writeContent(folder1.newFile("source-context.json"), "{\"whatIsThis\":\"value1\"}");
    writeContent(folder2.newFile("source-context.json"), "{\"whatIsThis\":\"value2\"}");

    // This source context file has the same contents as another file, and should be discarded.
    writeContent(folder3.newFile("source-context.json"), "{\"whatIsThis\":\"value2\"}");

    // This source context file is not in the class path, but in a recognized directory relative to
    // the class path.
    folder4.newFolder("WEB-INF");
    folder4.newFolder("WEB-INF/classes");
    writeContent(
        folder4.newFile("WEB-INF/classes/source-context.json"), "{\"whatIsThis\":\"value3\"}");

    // This source context file is at the root directory of a foo.jar file which is on the class
    // path.
    File fooJar = folder5.newFile("foo.jar");
    writeContentToZipFile(fooJar, "source-context.json", "{\"whatIsThis\":\"value4\"}");

    // This source context file is at a recognized directory inside the bar.jar file which is on the
    // class path.
    File barJar = folder5.newFile("bar.jar");
    writeContentToZipFile(
        barJar, "WEB-INF/classes/source-context.json", "{\"whatIsThis\":\"value5\"}");

    classPathLookup =
        new ClassPathLookup(
            false,
            new String[] {
              folder1.getRoot().toString(),
              folder2.getRoot().toString(),
              folder3.getRoot().toString(),
              folder4.getRoot().toString(),
              folder5.getRoot().toString(),
              fooJar.toString(),
              barJar.toString()
            });

    ByteArrayOutputStream requestStream = new ByteArrayOutputStream();
    HttpURLConnection connection = mock(HttpURLConnection.class);
    when(connection.getOutputStream()).thenReturn(requestStream);
    when(connection.getInputStream())
        .thenReturn(streamFromString("{\"debuggee\":{\"id\":\"whatever\"}}"));

    when(mockServer.openConnection("debuggees/register")).thenReturn(connection);

    assertThat(new GcpHubClient(baseUrl, metadata, classPathLookup, DEFAULT_LABELS)
                   .registerDebuggee(EMPTY_EXTRA_DEBUGGEE_LABELS))
        .isTrue();

    JsonElement request = JsonParser.parseString(new String(requestStream.toByteArray(), UTF_8));
    assertThat(
            request
                .getAsJsonObject()
                .get("debuggee")
                .getAsJsonObject()
                .get("sourceContexts")
                .toString())
        .isEqualTo(
            "["
                + "{\"whatIsThis\":\"value1\"},"
                + "{\"whatIsThis\":\"value2\"},"
                + "{\"whatIsThis\":\"value3\"},"
                + "{\"whatIsThis\":\"value4\"},"
                + "{\"whatIsThis\":\"value5\"}"
                + "]");
  }

  @Test(expected = Exception.class)
  public void registerDebuggeeResponseNotJson() throws Exception {
    HttpURLConnection connection = mock(HttpURLConnection.class);
    when(connection.getOutputStream()).thenReturn(new ByteArrayOutputStream());
    when(connection.getInputStream()).thenReturn(streamFromString("this is not a JSON"));

    when(mockServer.openConnection("debuggees/register")).thenReturn(connection);

    new GcpHubClient(baseUrl, metadata, classPathLookup, DEFAULT_LABELS)
        .registerDebuggee(EMPTY_EXTRA_DEBUGGEE_LABELS);
  }

  @Test(expected = Exception.class)
  public void registerDebuggeeResponseNotObject() throws Exception {
    HttpURLConnection connection = mock(HttpURLConnection.class);
    when(connection.getOutputStream()).thenReturn(new ByteArrayOutputStream());
    when(connection.getInputStream()).thenReturn(streamFromString("[]"));

    when(mockServer.openConnection("debuggees/register")).thenReturn(connection);

    new GcpHubClient(baseUrl, metadata, classPathLookup, DEFAULT_LABELS)
        .registerDebuggee(EMPTY_EXTRA_DEBUGGEE_LABELS);
  }

  @Test(expected = Exception.class)
  public void registerDebuggeeEmptyDebuggeeId() throws Exception {
    HttpURLConnection connection = mock(HttpURLConnection.class);
    when(connection.getOutputStream()).thenReturn(new ByteArrayOutputStream());
    when(connection.getInputStream()).thenReturn(streamFromString("{\"debuggee\":{\"id\":\"\"}}"));

    when(mockServer.openConnection("debuggees/register")).thenReturn(connection);

    new GcpHubClient(baseUrl, metadata, classPathLookup, DEFAULT_LABELS)
        .registerDebuggee(EMPTY_EXTRA_DEBUGGEE_LABELS);
  }

  @Test
  public void registerDebuggeeMutableDebuggeeId() throws Exception {
    GcpHubClient hubClient = registerDebuggee();

    HttpURLConnection connection = mock(HttpURLConnection.class);
    when(connection.getOutputStream()).thenReturn(new ByteArrayOutputStream());
    when(connection.getInputStream())
        .thenReturn(streamFromString("{\"debuggee\":{\"id\":\"different-id\"}}"));

    when(mockServer.openConnection("debuggees/register")).thenReturn(connection);

    assertThat(hubClient.registerDebuggee(EMPTY_EXTRA_DEBUGGEE_LABELS)).isTrue();
    assertThat(hubClient.getDebuggeeId()).isEqualTo("different-id");
  }

  @Test
  public void registerDebuggeeCanaryModePerperties() throws Exception {
    // Verify that setting the canary mode properties correctly would change the default value of
    // canaryMode. Note that the different combinations of the properties would be verified in the
    // corresponding unit test.
    try {
      System.setProperty("com.google.cdbg.breakpoints.enable_canary", "true");
      System.setProperty("com.google.cdbg.breakpoints.allow_canary_override", "true");
      registerDebuggee();
      assertThat(registerDebuggeeRequest)
          .matches(".*\"canaryMode\":\"CANARY_MODE_DEFAULT_ENABLED\".*");
    } finally {
      System.clearProperty("com.google.cdbg.breakpoints.enable_canary");
      System.clearProperty("com.google.cdbg.breakpoints.allow_canary_override");
    }
  }

  @Test
  public void listActiveBreakpointsSuccess() throws Exception {
    HubClient hubClient = registerDebuggee();
    HubClient.ListActiveBreakpointsResult result;

    HttpURLConnection connection = mock(HttpURLConnection.class);
    when(connection.getInputStream()).thenReturn(streamFromString("{\"nextWaitToken\":\"ABC\"}"));
    when(mockServer.openConnection(
            "debuggees/test-debuggee-id/breakpoints"
                + "?successOnTimeout=true&waitToken=init&agentId=test-agent-id"))
        .thenReturn(connection);

    result = hubClient.listActiveBreakpoints();
    assertThat(result.getIsTimeout()).isFalse();
    assertThat(result.getFormat()).isEqualTo("json");
    assertThat(result.getActiveBreakpoints()).isEmpty();

    when(connection.getInputStream())
        .thenReturn(
            streamFromString(
                "{\"breakpoints\":[{\"id\":\"A\"},{\"id\":\"B\"}],\"nextWaitToken\":\"DEF\"}"));
    when(mockServer.openConnection(
            "debuggees/test-debuggee-id/breakpoints"
                + "?successOnTimeout=true&waitToken=ABC&agentId=test-agent-id"))
        .thenReturn(connection);

    result = hubClient.listActiveBreakpoints();
    assertThat(result.getIsTimeout()).isFalse();

    byte[][] breakpoints = result.getActiveBreakpoints();
    assertThat(breakpoints).hasLength(2);

    assertThat(JsonParser.parseString(new String(breakpoints[0], UTF_8)).toString())
        .isEqualTo("{\"id\":\"A\"}");
    assertThat(JsonParser.parseString(new String(breakpoints[1], UTF_8)).toString())
        .isEqualTo("{\"id\":\"B\"}");
  }

  @Test
  public void listActiveBreakpointsWaitExpired() throws Exception {
    HubClient hubClient = registerDebuggee();

    HttpURLConnection connection = mock(HttpURLConnection.class);
    when(connection.getInputStream()).thenReturn(streamFromString("{\"waitExpired\":\"true\"}"));
    when(mockServer.openConnection(
            "debuggees/test-debuggee-id/breakpoints"
                + "?successOnTimeout=true&waitToken=init&agentId=test-agent-id"))
        .thenReturn(connection);

    HubClient.ListActiveBreakpointsResult result = hubClient.listActiveBreakpoints();
    assertThat(result.getIsTimeout()).isTrue();
  }

  @Test(expected = IOException.class)
  public void listActiveBreakpointsFailure() throws Exception {
    HubClient hubClient = registerDebuggee();

    HttpURLConnection connection = mock(HttpURLConnection.class);
    when(connection.getInputStream()).thenThrow(new IOException());
    when(connection.getErrorStream()).thenReturn(new ByteArrayInputStream("error".getBytes(UTF_8)));
    when(connection.getResponseCode()).thenReturn(408);

    when(mockServer.openConnection(
            "debuggees/test-debuggee-id/breakpoints"
                + "?successOnTimeout=true&waitToken=init&agentId=test-agent-id"))
        .thenReturn(connection);

    hubClient.listActiveBreakpoints();
  }

  @Test(expected = JsonSyntaxException.class)
  public void listActiveBreakpointsBadResponse() throws Exception {
    HubClient hubClient = registerDebuggee();

    HttpURLConnection connection = mock(HttpURLConnection.class);
    when(connection.getInputStream()).thenReturn(streamFromString("not a valid json"));

    when(mockServer.openConnection(
            "debuggees/test-debuggee-id/breakpoints"
                + "?successOnTimeout=true&waitToken=init&agentId=test-agent-id"))
        .thenReturn(connection);

    hubClient.listActiveBreakpoints();
  }

  @Test
  public void transmitBreakpointSuccess() throws Exception {
    HubClient hubClient = registerDebuggee();

    ByteArrayOutputStream requestStream = new ByteArrayOutputStream();
    HttpURLConnection connection = mock(HttpURLConnection.class);
    when(connection.getOutputStream()).thenReturn(requestStream);
    when(connection.getInputStream()).thenReturn(streamFromString(""));

    when(mockServer.openConnection("debuggees/test-debuggee-id/breakpoints/bpid"))
        .thenReturn(connection);

    hubClient.transmitBreakpointUpdate(
        "json", "bpid", "{\"whatIsThis\":\"breakpoint\"}".getBytes(UTF_8));

    JsonElement request = JsonParser.parseString(new String(requestStream.toByteArray(), UTF_8));
    assertThat(request.toString()).isEqualTo("{\"breakpoint\":{\"whatIsThis\":\"breakpoint\"}}");
  }

  @Test(expected = UnsupportedOperationException.class)
  public void transmitBreakpointUpdateBadFormat() throws Exception {
    HubClient hubClient = registerDebuggee();
    hubClient.transmitBreakpointUpdate("protobuf", "bpid", new byte[0]);
  }

  @Test
  public void transmitBreakpointTransientError() throws Exception {
    for (int statusCode : new int[] {408, 500}) {
      HubClient hubClient = registerDebuggee();

      ByteArrayOutputStream requestStream = new ByteArrayOutputStream();
      HttpURLConnection connection = mock(HttpURLConnection.class);
      when(connection.getOutputStream()).thenReturn(requestStream);
      when(connection.getInputStream()).thenThrow(new IOException());
      when(connection.getErrorStream()).thenReturn(new ByteArrayInputStream("err".getBytes(UTF_8)));
      when(connection.getResponseCode()).thenReturn(statusCode);

      when(mockServer.openConnection("debuggees/test-debuggee-id/breakpoints/bpid"))
          .thenReturn(connection);

      IOException transmitException = null;
      try {
        hubClient.transmitBreakpointUpdate(
            "json", "bpid", "{\"whatIsThis\":\"breakpoint\"}".getBytes(UTF_8));
      } catch (IOException e) {
        transmitException = e;
      }

      assertThat(transmitException).isNotNull();
    }
  }

  @Test
  public void transmitBreakpointPermanentError() throws Exception {
    for (int statusCode : new int[] {400, 403, 404}) {
      HubClient hubClient = registerDebuggee();

      ByteArrayOutputStream requestStream = new ByteArrayOutputStream();
      HttpURLConnection connection = mock(HttpURLConnection.class);
      when(connection.getOutputStream()).thenReturn(requestStream);
      when(connection.getInputStream()).thenThrow(new IOException());
      when(connection.getResponseCode()).thenReturn(statusCode);
      when(connection.getResponseMessage()).thenReturn("something failed");
      when(connection.getErrorStream()).thenReturn(new ByteArrayInputStream("err".getBytes(UTF_8)));

      when(mockServer.openConnection("debuggees/test-debuggee-id/breakpoints/bpid"))
          .thenReturn(connection);

      hubClient.transmitBreakpointUpdate(
          "json", "bpid", "{\"whatIsThis\":\"breakpoint\"}".getBytes(UTF_8));
    }
  }

  /** Common code to get the debuggee registered for all test cases requiring that. */
  private GcpHubClient registerDebuggee() throws Exception {
    return registerDebuggee(EMPTY_EXTRA_DEBUGGEE_LABELS);
  }

  /**
   * Common code to get the debuggee registered with extra labels provided in the registerDebuggee
   * call.
   */
  private GcpHubClient registerDebuggee(Map<String, String> extraLabels)
      throws Exception {
    ByteArrayOutputStream registerDebuggeeBodyStream = new ByteArrayOutputStream();
    HttpURLConnection connection = mock(HttpURLConnection.class);
    when(connection.getOutputStream()).thenReturn(registerDebuggeeBodyStream);
    when(connection.getInputStream())
        .thenReturn(
            streamFromString(
                "{\"debuggee\":{\"id\":\"test-debuggee-id\",\"idDisabled\":false},"
                    + "\"agentId\":\"test-agent-id\"}"));

    when(mockServer.openConnection("debuggees/register")).thenReturn(connection);

    GcpHubClient hubClient = new GcpHubClient(baseUrl, metadata, classPathLookup, DEFAULT_LABELS);
    assertThat(hubClient.registerDebuggee(extraLabels)).isTrue();
    assertThat(hubClient.getDebuggeeId()).isEqualTo("test-debuggee-id");
    assertThat(hubClient.getAgentId()).isEqualTo("test-agent-id");

    registerDebuggeeRequest = registerDebuggeeBodyStream.toString(UTF_8.name());

    return hubClient;
  }

  private void writeContent(File file, String content)
      throws FileNotFoundException, UnsupportedEncodingException {
    try (PrintWriter writer = new PrintWriter(file, "UTF-8")) {
      writer.write(content);
    }
  }

  /**
   * Creates a zip file with a single file inside it.
   *
   * @param file a file whose contents will be populated as a zip file
   * @param filePath the path of the file that will be put inside the zip file
   * @param content the data that will be put into the inner file
   */
  private void writeContentToZipFile(File file, String filePath, String content)
      throws IOException {
    ZipOutputStream out = new ZipOutputStream(new FileOutputStream(file));

    // First, add all directories in the file path to the zip file.
    // E.g., for filePath=a/b/c add: "a", "a/b".
    int index = filePath.indexOf('/');
    while (index != -1) {
      out.putNextEntry(new ZipEntry(filePath.substring(0, index)));
      index = filePath.indexOf('/', index + 1);
    }

    // Then add the file "a/b/c" to the zip file.
    out.putNextEntry(new ZipEntry(filePath));

    // Write the contents of the file.
    out.write(content.getBytes(UTF_8));
    out.close();
  }

  private ByteArrayInputStream streamFromString(String s) {
    return new ByteArrayInputStream(s.getBytes(UTF_8));
  }
}
