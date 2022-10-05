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
import static org.junit.Assert.assertThrows;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.ArgumentMatchers.startsWith;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.auth.oauth2.GoogleCredentials;
import com.google.common.collect.ImmutableMap;
import com.google.firebase.FirebaseApp;
import com.google.firebase.FirebaseOptions;
import com.google.firebase.database.DataSnapshot;
import com.google.firebase.database.DatabaseError;
import com.google.firebase.database.DatabaseReference;
import com.google.firebase.database.FirebaseDatabase;
import com.google.firebase.database.ValueEventListener;
import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonParser;
import com.google.gson.ToNumberPolicy;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.UnsupportedEncodingException;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.concurrent.TimeUnit;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;

@RunWith(JUnit4.class)
public final class FirebaseClientTest {
  private static final ImmutableMap<String, String> DEFAULT_LABELS =
      ImmutableMap.<String, String>builder()
          .put("module", "default")
          .put("version", "v1")
          .put("minorversion", "12345")
          .buildOrThrow();

  /** Useful empty extra debuggee labels map to use for registerDebuggee calls. */
  private static final ImmutableMap<String, String> EMPTY_EXTRA_DEBUGGEE_LABELS =
      ImmutableMap.<String, String>of();

  @Rule public TemporaryFolder folder1 = new TemporaryFolder();
  @Rule public TemporaryFolder folder2 = new TemporaryFolder();
  @Rule public TemporaryFolder folder3 = new TemporaryFolder();
  @Rule public TemporaryFolder folder4 = new TemporaryFolder();
  @Rule public TemporaryFolder folder5 = new TemporaryFolder();
  @Rule public TemporaryFolder folder6 = new TemporaryFolder();
  @Rule public final MockitoRule mocks = MockitoJUnit.rule();

  @Mock private FirebaseClient.FirebaseStaticWrappers mockFirebaseStaticWrappers;
  @Mock private FirebaseApp mockFirebaseApp;
  @Mock private FirebaseDatabase mockFirebaseDatabase;
  @Mock private GoogleCredentials mockGoogleCredentials;

  private FirebaseClient.TimeoutConfig timeouts =
      new FirebaseClient.TimeoutConfig(100, TimeUnit.MILLISECONDS);
  private MetadataQuery metadata;

  private ClassPathLookup classPathLookup = new ClassPathLookup(false, null);

  /**
   * Last registered debuggee.
   *
   * <p>Every time the registerDebuggee helper is used this value is updated.
   */
  private FirebaseClient.Debuggee registeredDebuggee;

  /**
   * The listener the FirebaseClient registered for receiving breakpoint updates.
   *
   * <p>This value will be set by a each call to the registerDebuggee helper method, can can be used
   * to inject a breakpoint update into the client.
   */
  private ValueEventListener breakpointUpdateListener;

  @Before
  public void setUp() {
    metadata =
        new MetadataQuery() {
          @Override
          public String getProjectNumber() {
            return "123456789";
          }

          @Override
          public GoogleCredentials getGoogleCredential() {
            return mockGoogleCredentials;
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

    when(mockFirebaseStaticWrappers.initializeApp(
            (FirebaseOptions) notNull(), eq("SnapshotDebugger")))
        .thenReturn(mockFirebaseApp);
  }

  @Test
  public void initializeFirebaseAppSuccessOnCdbgDbInstance() throws Exception {
    // We can simply use registerDebuggee() here as it calls
    // firebaseClient.initializeFirebaseApp() for us, less mocking to setup for this this.
    registerDebuggee();

    ArgumentCaptor<FirebaseOptions> capturedFirebaseOptions =
        ArgumentCaptor.forClass(FirebaseOptions.class);
    verify(mockFirebaseStaticWrappers).initializeApp(capturedFirebaseOptions.capture(), any());
    assertThat(capturedFirebaseOptions.getValue().getDatabaseUrl())
        .isEqualTo("https://mock-project-id-cdbg.firebaseio.com");
  }

  @Test
  public void initializeFirebaseAppSuccessOnDefaultRtdbInstance() throws Exception {
    FirebaseApp mockFirebaseApp1 = mock(FirebaseApp.class);
    FirebaseApp mockFirebaseApp2 = mock(FirebaseApp.class);
    FirebaseDatabase mockFirebaseDb1 = mock(FirebaseDatabase.class);
    FirebaseDatabase mockFirebaseDb2 = mock(FirebaseDatabase.class);
    DatabaseReference mockSchemaVersionDbRef1 = mock(DatabaseReference.class);
    DatabaseReference mockSchemaVersionDbRef2 = mock(DatabaseReference.class);

    setOnCancelledDbGet(mockSchemaVersionDbRef1, "Get Failed");
    setResponseDbGet(mockSchemaVersionDbRef2, "2");

    when(mockFirebaseStaticWrappers.initializeApp(
            (FirebaseOptions) notNull(), eq("SnapshotDebugger")))
        .thenReturn(mockFirebaseApp1, mockFirebaseApp2);

    when(mockFirebaseStaticWrappers.getDbInstance(mockFirebaseApp1)).thenReturn(mockFirebaseDb1);
    when(mockFirebaseStaticWrappers.getDbInstance(mockFirebaseApp2)).thenReturn(mockFirebaseDb2);
    when(mockFirebaseDb1.getReference(eq("cdbg/schema_version")))
        .thenReturn(mockSchemaVersionDbRef1);
    when(mockFirebaseDb2.getReference(eq("cdbg/schema_version")))
        .thenReturn(mockSchemaVersionDbRef2);

    FirebaseClient firebaseClient =
        new FirebaseClient(
            metadata, classPathLookup, DEFAULT_LABELS, mockFirebaseStaticWrappers, timeouts);
    firebaseClient.initializeFirebaseApp();

    ArgumentCaptor<FirebaseOptions> capturedFirebaseOptions =
        ArgumentCaptor.forClass(FirebaseOptions.class);

    // Since the first firebase db query failed, the code is expected to call delete on the app that
    // failed before it attempts to use the next URL.
    verify(mockFirebaseApp1).delete();

    verify(mockFirebaseApp2, never()).delete();
    verify(mockFirebaseStaticWrappers, times(2))
        .initializeApp(capturedFirebaseOptions.capture(), any());
    assertThat(capturedFirebaseOptions.getValue().getDatabaseUrl())
        .isEqualTo("https://mock-project-id-default-rtdb.firebaseio.com");
  }

  @Test
  public void initializeFirebaseAppSuccessOnManuallyConfiguredDb() throws Exception {
    try {
      System.setProperty("com.google.cdbg.firebase_db_url", "https://fake-db-url.com");

      // We can simply use registerDebuggee() here as it calls
      // firebaseClient.initializeFirebaseApp() for us, less mocking to setup for this this.
      registerDebuggee();

      ArgumentCaptor<FirebaseOptions> capturedFirebaseOptions =
          ArgumentCaptor.forClass(FirebaseOptions.class);
      verify(mockFirebaseStaticWrappers).initializeApp(capturedFirebaseOptions.capture(), any());
      assertThat(capturedFirebaseOptions.getValue().getDatabaseUrl())
          .isEqualTo("https://fake-db-url.com");
    } finally {
      System.clearProperty("com.google.cdbg.firebase_db_url");
    }
  }

  @Test
  public void initializeFirebaseAppThrowsOnFailure() throws Exception {
    DatabaseReference mockSchemaVersionDbRef = mock(DatabaseReference.class);
    when(mockFirebaseStaticWrappers.getDbInstance(mockFirebaseApp))
        .thenReturn(mockFirebaseDatabase);
    when(mockFirebaseDatabase.getReference(eq("cdbg/schema_version")))
        .thenReturn(mockSchemaVersionDbRef);

    // Don't set any responses for the schema_version gets, should cause an exception to be thrown
    // since a good DB instance couldn't be found.

    FirebaseClient firebaseClient =
        new FirebaseClient(
            metadata, classPathLookup, DEFAULT_LABELS, mockFirebaseStaticWrappers, timeouts);
    Exception ex = assertThrows(Exception.class, () -> firebaseClient.initializeFirebaseApp());
    assertThat(ex)
        .hasMessageThat()
        .isEqualTo(
            "Failed to initialize FirebaseApp, attempted URLs:"
                + " [https://mock-project-id-cdbg.firebaseio.com,"
                + " https://mock-project-id-default-rtdb.firebaseio.com]");
  }

  @Test
  public void initializeFirebaseAppDoesNothingAfterSuccessfulCall() throws Exception {
    // We can simply use registerDebuggee() here as it calls
    // firebaseClient.initializeFirebaseApp() for us, less mocking to setup for this this.
    FirebaseClient firebaseClient = registerDebuggee();

    reset(mockFirebaseStaticWrappers);
    reset(mockFirebaseDatabase);

    firebaseClient.initializeFirebaseApp();
    verify(mockFirebaseStaticWrappers, never()).initializeApp(any(), any());
    verify(mockFirebaseStaticWrappers, never()).getDbInstance(any());
  }

  @Test
  public void registerDebuggeeSuccess() throws Exception {
    registerDebuggee();
    assertThat(registeredDebuggee.id).matches("d-[0-9a-f]{8}");
    assertThat(registeredDebuggee.uniquifier).matches("[0-9A-F]+");
    assertThat(registeredDebuggee.description).isEqualTo("mock-project-id-default-v1-12345");
    assertThat(registeredDebuggee.labels).containsEntry("projectid", "mock-project-id");
    assertThat(registeredDebuggee.labels).containsEntry("module", "default");
    assertThat(registeredDebuggee.labels).containsEntry("version", "v1");
    assertThat(registeredDebuggee.labels).containsEntry("minorversion", "12345");
    assertThat(registeredDebuggee.agentVersion).matches("cloud-debug-java/v[0-9]+.[0-9]+");
    assertThat(registeredDebuggee.sourceContexts).isEmpty();
  }

  @Test
  public void registerDebuggeeWithExtraLabels() throws Exception {
    // Ensure the extra labels provided in the register call coexist with the base labels provided
    // in the constructor.
    ImmutableMap<String, String> extraLabels =
        ImmutableMap.<String, String>of("afoo", "abar", "mkfoo", "mkbar", "sfoo", "sbar");

    registerDebuggee(DEFAULT_LABELS, extraLabels);
    assertThat(registeredDebuggee.id).matches("d-[0-9a-f]{8,8}");
    assertThat(registeredDebuggee.uniquifier).matches("[0-9A-F]+");
    assertThat(registeredDebuggee.description).isEqualTo("mock-project-id-default-v1-12345");
    assertThat(registeredDebuggee.labels).containsEntry("projectid", "mock-project-id");
    assertThat(registeredDebuggee.labels).containsEntry("module", "default");
    assertThat(registeredDebuggee.labels).containsEntry("version", "v1");
    assertThat(registeredDebuggee.labels).containsEntry("minorversion", "12345");
    assertThat(registeredDebuggee.labels).containsEntry("afoo", "abar");
    assertThat(registeredDebuggee.labels).containsEntry("mkfoo", "mkbar");
    assertThat(registeredDebuggee.labels).containsEntry("sfoo", "sbar");
    assertThat(registeredDebuggee.agentVersion).matches("cloud-debug-java/v[0-9]+.[0-9]+");
    assertThat(registeredDebuggee.sourceContexts).isEmpty();
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
      registerDebuggee(testCase.getKey(), EMPTY_EXTRA_DEBUGGEE_LABELS);
      assertThat(registeredDebuggee.description).isEqualTo(testCase.getValue());
    }
  }

  @Test
  public void registerDebuggeeWithSourceContext() throws Exception {
    // These source context files are directly on the class path.
    writeContent(folder1.newFile("source-context.json"), quotes("{'whatIsThis':'value1'}"));
    writeContent(folder2.newFile("source-context.json"), quotes("{'whatIsThis':'value2'}"));

    // This source context file has the same contents as another file, and should be discarded.
    writeContent(folder3.newFile("source-context.json"), quotes("{'whatIsThis':'value2'}"));

    // This source context file is not in the class path, but in a recognized directory relative to
    // the class path.
    folder4.newFolder("WEB-INF");
    folder4.newFolder("WEB-INF/classes");
    writeContent(
        folder4.newFile("WEB-INF/classes/source-context.json"), quotes("{'whatIsThis':'value3'}"));

    // This source context file is at the root directory of a foo.jar file which is on the class
    // path.
    File fooJar = folder5.newFile("foo.jar");
    writeContentToZipFile(fooJar, "source-context.json", quotes("{'whatIsThis':'value4'}"));

    // This source context file is at a recognized directory inside the bar.jar file which is on the
    // class path.
    File barJar = folder5.newFile("bar.jar");
    writeContentToZipFile(
        barJar, "WEB-INF/classes/source-context.json", quotes("{'whatIsThis':'value5'}"));

    // This source context file contains invalid JSON, should not be in the result.
    writeContent(folder6.newFile("source-context.json"), "foo:");

    classPathLookup =
        new ClassPathLookup(
            false,
            new String[] {
              folder1.getRoot().toString(),
              folder2.getRoot().toString(),
              folder3.getRoot().toString(),
              folder4.getRoot().toString(),
              folder5.getRoot().toString(),
              folder6.getRoot().toString(),
              fooJar.toString(),
              barJar.toString()
            });

    registerDebuggee();
    assertThat(registeredDebuggee.sourceContexts)
        .containsExactly(
            ImmutableMap.of("whatIsThis", "value1"),
            ImmutableMap.of("whatIsThis", "value2"),
            ImmutableMap.of("whatIsThis", "value3"),
            ImmutableMap.of("whatIsThis", "value4"),
            ImmutableMap.of("whatIsThis", "value5"));
  }

  @Test
  public void registerDebuggeeThrowsOnInitializeFirebaseAppFailure() throws Exception {
    DatabaseReference mockSchemaVersionDbRef = mock(DatabaseReference.class);
    when(mockFirebaseStaticWrappers.getDbInstance(mockFirebaseApp))
        .thenReturn(mockFirebaseDatabase);
    when(mockFirebaseDatabase.getReference(eq("cdbg/schema_version")))
        .thenReturn(mockSchemaVersionDbRef);

    // Don't set any responses for the schema_version gets, should cause an exception to be thrown
    // since a good DB instanced couldn't be found.

    FirebaseClient firebaseClient =
        new FirebaseClient(
            metadata, classPathLookup, DEFAULT_LABELS, mockFirebaseStaticWrappers, timeouts);
    Exception ex =
        assertThrows(
            Exception.class, () -> firebaseClient.registerDebuggee(EMPTY_EXTRA_DEBUGGEE_LABELS));
    assertThat(ex)
        .hasMessageThat()
        .isEqualTo(
            "Failed to initialize FirebaseApp, attempted URLs:"
                + " [https://mock-project-id-cdbg.firebaseio.com,"
                + " https://mock-project-id-default-rtdb.firebaseio.com]");
  }

  @Test
  public void registerDebuggeeThrowsOnSetDebuggeeTimeout() throws Exception {
    DatabaseReference mockSchemaVersionDbRef = mock(DatabaseReference.class);
    DatabaseReference mockDebuggeesDbRef = mock(DatabaseReference.class);

    when(mockFirebaseStaticWrappers.getDbInstance(mockFirebaseApp))
        .thenReturn(mockFirebaseDatabase);
    when(mockFirebaseDatabase.getReference(eq("cdbg/schema_version")))
        .thenReturn(mockSchemaVersionDbRef);
    when(mockFirebaseDatabase.getReference(startsWith("cdbg/debuggees/")))
        .thenReturn(mockDebuggeesDbRef);

    setResponseDbGet(mockSchemaVersionDbRef, "2");
    // NOTE, don't set a response for the debuggees reference, this will cause the set to timeout as
    // desired by the test.

    FirebaseClient firebaseClient =
        new FirebaseClient(
            metadata, classPathLookup, DEFAULT_LABELS, mockFirebaseStaticWrappers, timeouts);
    Exception ex =
        assertThrows(
            Exception.class, () -> firebaseClient.registerDebuggee(EMPTY_EXTRA_DEBUGGEE_LABELS));
    assertThat(ex)
        .hasMessageThat()
        .matches(
            "Firebase Database write operation to 'cdbg/debuggees.* error: 'Timed out after.*'");
  }

  @Test
  public void registerDebuggeeThrowsOnSetDebuggeeError() throws Exception {
    DatabaseReference mockSchemaVersionDbRef = mock(DatabaseReference.class);
    DatabaseReference mockDebuggeesDbRef = mock(DatabaseReference.class);

    when(mockFirebaseStaticWrappers.getDbInstance(mockFirebaseApp))
        .thenReturn(mockFirebaseDatabase);
    when(mockFirebaseDatabase.getReference(eq("cdbg/schema_version")))
        .thenReturn(mockSchemaVersionDbRef);
    when(mockFirebaseDatabase.getReference(startsWith("cdbg/debuggees/")))
        .thenReturn(mockDebuggeesDbRef);

    setResponseDbGet(mockSchemaVersionDbRef, "2");
    setCompletionFailedDbSet(mockDebuggeesDbRef, "FAKE DB ERROR");

    FirebaseClient firebaseClient =
        new FirebaseClient(
            metadata, classPathLookup, DEFAULT_LABELS, mockFirebaseStaticWrappers, timeouts);
    Exception ex =
        assertThrows(
            Exception.class, () -> firebaseClient.registerDebuggee(EMPTY_EXTRA_DEBUGGEE_LABELS));
    assertThat(ex)
        .hasMessageThat()
        .matches("Firebase Database write operation to 'cdbg/debuggees.* error: 'FAKE DB ERROR'");
  }

  @Test
  public void listActiveBreakpointsSuccess() throws Exception {
    HubClient hubClient = registerDebuggee();
    HubClient.ListActiveBreakpointsResult result;

    DataSnapshot dataSnapshot = buildDataSnapshotFromJsonString("{'A':{'id':'A'}, 'B':{'id':'B'}}");

    breakpointUpdateListener.onDataChange(dataSnapshot);

    result = hubClient.listActiveBreakpoints();
    assertThat(result.getIsTimeout()).isFalse();

    byte[][] breakpoints = result.getActiveBreakpoints();

    assertThat(convertBreakpoints(breakpoints))
        .containsExactly(
            createObjectFromJsonString("{'id':'A'}"), createObjectFromJsonString("{'id':'B'}"));
  }

  @Test
  public void listActiveBreakpointsReturnsMostRecentSnapshot() throws Exception {
    // It's possible multiple active breakpoint data snapshots arrive between calls to
    // listActiveBreakpoints. We want to ensure only the most recent snapshot is returned.
    HubClient hubClient = registerDebuggee();
    HubClient.ListActiveBreakpointsResult result;

    DataSnapshot dataSnapshot1 = buildDataSnapshotFromJsonString("{'A':{'id':'A'}}");
    DataSnapshot dataSnapshot2 =
        buildDataSnapshotFromJsonString("{'A':{'id':'A'}, 'B':{'id':'B'}}");
    DataSnapshot dataSnapshot3 = buildDataSnapshotFromJsonString("{'C':{'id':'C'}}");

    breakpointUpdateListener.onDataChange(dataSnapshot1);
    breakpointUpdateListener.onDataChange(dataSnapshot2);
    breakpointUpdateListener.onDataChange(dataSnapshot3);

    result = hubClient.listActiveBreakpoints();
    assertThat(result.getIsTimeout()).isFalse();

    byte[][] breakpoints = result.getActiveBreakpoints();

    assertThat(convertBreakpoints(breakpoints))
        .containsExactly(createObjectFromJsonString("{'id':'C'}"));

    // The next call should timeout, the first two snapshots should not resurface, they should be
    // dropped.
    assertThat(hubClient.listActiveBreakpoints().getIsTimeout()).isTrue();
  }

  @Test
  public void listActiveBreakpointsAllBreakpointsDeleted() throws Exception {
    HubClient hubClient = registerDebuggee();
    HubClient.ListActiveBreakpointsResult result;

    DataSnapshot dataSnapshot = buildDataSnapshotFromJsonString("{'A':{'id':'A'}, 'B':{'id':'B'}}");
    breakpointUpdateListener.onDataChange(dataSnapshot);

    result = hubClient.listActiveBreakpoints();
    assertThat(result.getIsTimeout()).isFalse();
    assertThat(result.getActiveBreakpoints()).hasLength(2);

    // A null json object represents all breakpoints having being deleted from the RTDB.
    dataSnapshot = buildDataSnapshotFromJsonObject(null);
    breakpointUpdateListener.onDataChange(dataSnapshot);
    result = hubClient.listActiveBreakpoints();
    assertThat(result.getIsTimeout()).isFalse();
    assertThat(result.getActiveBreakpoints()).hasLength(0);
  }

  @Test
  public void listActiveBreakpointsSomeBreakpointsDeleted() throws Exception {
    HubClient hubClient = registerDebuggee();
    HubClient.ListActiveBreakpointsResult result;

    DataSnapshot dataSnapshot = buildDataSnapshotFromJsonString("{'A':{'id':'A'}, 'B':{'id':'B'}}");
    breakpointUpdateListener.onDataChange(dataSnapshot);

    result = hubClient.listActiveBreakpoints();
    assertThat(result.getIsTimeout()).isFalse();
    assertThat(result.getActiveBreakpoints()).hasLength(2);

    dataSnapshot = buildDataSnapshotFromJsonString("{'B':{'id':'B'}}");
    breakpointUpdateListener.onDataChange(dataSnapshot);
    result = hubClient.listActiveBreakpoints();
    assertThat(result.getIsTimeout()).isFalse();

    byte[][] breakpoints = result.getActiveBreakpoints();
    assertThat(convertBreakpoints(breakpoints))
        .containsExactly(createObjectFromJsonString("{'id':'B'}"));
  }

  @Test
  public void listActiveBreakpointsIdAddedIfMissing() throws Exception {
    HubClient hubClient = registerDebuggee();
    HubClient.ListActiveBreakpointsResult result;

    // BP 'A' is missing the 'id' field, it should be inserted.
    DataSnapshot dataSnapshot =
        buildDataSnapshotFromJsonString("{'A':{'foo':'bar'}, 'B':{'id':'B'}}");

    breakpointUpdateListener.onDataChange(dataSnapshot);

    result = hubClient.listActiveBreakpoints();
    assertThat(result.getIsTimeout()).isFalse();

    byte[][] breakpoints = result.getActiveBreakpoints();

    ArrayList<String> breapointsJson = new ArrayList<>();

    assertThat(convertBreakpoints(breakpoints))
        .containsExactly(
            createObjectFromJsonString("{'id':'A', 'foo':'bar'}"),
            createObjectFromJsonString("{'id':'B'}"));
  }

  @Test
  public void listActiveBreakpointsWaitExpired() throws Exception {
    HubClient hubClient = registerDebuggee();

    // We have not provided a breakpoint snapshot, so this call should timeout.
    HubClient.ListActiveBreakpointsResult result = hubClient.listActiveBreakpoints();
    assertThat(result.getIsTimeout()).isTrue();
  }

  @Test(expected = Exception.class)
  public void listActiveBreakpointsBadResponse() throws Exception {
    HubClient hubClient = registerDebuggee();

    // This is bad json since foo maps to an exception, which won't parse as JSON
    Map<String, Object> badJsonObject = ImmutableMap.<String, Object>of("foo", new Exception());
    DataSnapshot dataSnapshot = buildDataSnapshotFromJsonObject(badJsonObject);
    breakpointUpdateListener.onDataChange(dataSnapshot);
    hubClient.listActiveBreakpoints();
  }

  @Test
  public void transmitBreakpointUpdateModifyBpSuccess() throws Exception {
    HubClient hubClient = registerDebuggee();

    // For transmitUpdate to work, the client must first receive the breakpoint from the DB.
    String bpSnapshot =
        quotes("{'A':{'id':'A', 'location':{'path':'Foo.java', 'line':9}, 'isFinalState':false}}");
    DataSnapshot dataSnapshot = buildDataSnapshotFromJsonString(bpSnapshot);
    breakpointUpdateListener.onDataChange(dataSnapshot);

    String bpPath = String.format("cdbg/breakpoints/%s/active/A", registeredDebuggee.id);
    DatabaseReference mockBpDbRef = mock(DatabaseReference.class);
    when(mockFirebaseDatabase.getReference(bpPath)).thenReturn(mockBpDbRef);

    setCompletionSuccessDbSet(mockBpDbRef);

    // Modification here is line changed from 9 to 10.
    String bpUpdate =
        quotes("{'id':'A', 'location':{'path':'Foo.java', 'line':10}, 'isFinalState':false}");
    hubClient.transmitBreakpointUpdate("json", "A", bpUpdate.getBytes(UTF_8));

    Object expectedJsonObject = createObjectFromJsonString(bpUpdate);
    verify(mockBpDbRef).setValue(eq(expectedJsonObject), any());
  }

  @Test
  public void transmitBreakpointUpdateSnapshotCompleteSuccess() throws Exception {
    HubClient hubClient = registerDebuggee();

    // For transmitUpdate to work, the client must first receive the breakpoint from the DB.
    String bpSnapshot =
        quotes(
            "{'A':{'id':'A', 'action':'SNAPSHOT', 'location':{'path':'Foo.java', 'line':9},"
                + " 'isFinalState':false}}");
    DataSnapshot dataSnapshot = buildDataSnapshotFromJsonString(bpSnapshot);
    breakpointUpdateListener.onDataChange(dataSnapshot);

    String debuggeeId = registeredDebuggee.id;
    String activeBpPath = String.format("cdbg/breakpoints/%s/active/A", debuggeeId);
    String finalBpPath = String.format("cdbg/breakpoints/%s/final/A", debuggeeId);
    String snapshotBpPath = String.format("cdbg/breakpoints/%s/snapshot/A", debuggeeId);

    DatabaseReference mockActiveDbRef = mock(DatabaseReference.class);
    DatabaseReference mockFinalDbRef = mock(DatabaseReference.class);
    DatabaseReference mockSnapshotDbRef = mock(DatabaseReference.class);

    when(mockFirebaseDatabase.getReference(eq(activeBpPath))).thenReturn(mockActiveDbRef);
    when(mockFirebaseDatabase.getReference(eq(finalBpPath))).thenReturn(mockFinalDbRef);
    when(mockFirebaseDatabase.getReference(eq(snapshotBpPath))).thenReturn(mockSnapshotDbRef);

    setCompletionSuccessDbSet(mockFinalDbRef);
    setCompletionSuccessDbSet(mockSnapshotDbRef);
    setCompletionSuccessDbSet(mockActiveDbRef);

    String finalCoreFields =
        "'id':'A', 'action':'SNAPSHOT', 'location':{'path':'Foo.java', 'line':9},"
            + " 'isFinalState':true, 'finalTimeUnixMsec':{'.sv':'timestamp'}";

    String captureFields =
        "'evaluatedExpressions':'placeholder', 'stackFrames':'placeholder',"
            + " 'variableTable':'placeholder'";

    String finalUpdate =
        quotes(String.format("{%s, %s}", "'id':'A', 'isFinalState':true", captureFields));
    hubClient.transmitBreakpointUpdate("json", "A", finalUpdate.getBytes(UTF_8));

    Object expectedActiveBpObject = null; // null is sent to delete it from active path
    Object expectedSnapshotBpObject =
        createObjectFromJsonString(
            quotes(String.format("{%s, %s}", finalCoreFields, captureFields)));
    Object expectedFinalBpObject =
        createObjectFromJsonString(quotes(String.format("{%s}", finalCoreFields)));

    InOrder inOrder = inOrder(mockFinalDbRef, mockSnapshotDbRef, mockActiveDbRef);
    inOrder.verify(mockSnapshotDbRef).setValue(eq(expectedSnapshotBpObject), any());
    inOrder.verify(mockFinalDbRef).setValue(eq(expectedFinalBpObject), any());
    inOrder.verify(mockActiveDbRef).setValue(eq(expectedActiveBpObject), any());
  }

  @Test
  public void transmitBreakpointUpdateLogpointCompleteSuccess() throws Exception {
    HubClient hubClient = registerDebuggee();

    // For transmitUpdate to work, the client must first receive the breakpoint from the DB.
    String bpSnapshot =
        quotes(
            "{'A':{'id':'A', 'action':'LOG', 'location':{'path':'Foo.java', 'line':5},"
                + " 'isFinalState':false}}");
    DataSnapshot dataSnapshot = buildDataSnapshotFromJsonString(bpSnapshot);
    breakpointUpdateListener.onDataChange(dataSnapshot);

    String debuggeeId = registeredDebuggee.id;
    String activeBpPath = String.format("cdbg/breakpoints/%s/active/A", debuggeeId);
    String finalBpPath = String.format("cdbg/breakpoints/%s/final/A", debuggeeId);

    DatabaseReference mockActiveDbRef = mock(DatabaseReference.class);
    DatabaseReference mockFinalDbRef = mock(DatabaseReference.class);

    when(mockFirebaseDatabase.getReference(eq(activeBpPath))).thenReturn(mockActiveDbRef);
    when(mockFirebaseDatabase.getReference(eq(finalBpPath))).thenReturn(mockFinalDbRef);

    setCompletionSuccessDbSet(mockFinalDbRef);
    setCompletionSuccessDbSet(mockActiveDbRef);

    String finalUpdate = quotes("{'id':'A', 'isFinalState':true}");
    hubClient.transmitBreakpointUpdate("json", "A", finalUpdate.getBytes(UTF_8));

    Object expectedActiveBpObject = null; // null is sent to delete it from active path
    Object expectedFinalBpObject =
        createObjectFromJsonString(
            "{'id':'A', 'action':'LOG', 'location':{'path':'Foo.java', 'line':5},"
                + " 'isFinalState':true, 'finalTimeUnixMsec':{'.sv':'timestamp'}}");

    InOrder inOrder = inOrder(mockFinalDbRef, mockActiveDbRef);
    inOrder.verify(mockFinalDbRef).setValue(eq(expectedFinalBpObject), any());
    inOrder.verify(mockActiveDbRef).setValue(eq(expectedActiveBpObject), any());
  }

  @Test
  public void transmitBreakpointUpdateThrowsOnSnapshotPathFailure() throws Exception {
    HubClient hubClient = registerDebuggee();

    // For transmitUpdate to work, the client must first receive the breakpoint from the DB.
    String bpSnapshot =
        quotes(
            "{'A':{'id':'A', 'action':'SNAPSHOT', 'location':{'path':'Foo.java', 'line':9},"
                + " 'isFinalState':false}}");
    DataSnapshot dataSnapshot = buildDataSnapshotFromJsonString(bpSnapshot);
    breakpointUpdateListener.onDataChange(dataSnapshot);

    String debuggeeId = registeredDebuggee.id;
    String snapshotBpPath = String.format("cdbg/breakpoints/%s/snapshot/A", debuggeeId);

    DatabaseReference mockSnapshotDbRef = mock(DatabaseReference.class);

    when(mockFirebaseDatabase.getReference(eq(snapshotBpPath))).thenReturn(mockSnapshotDbRef);

    setCompletionFailedDbSet(mockSnapshotDbRef, "FAKE DB ERROR");

    String captureFields =
        "'evaluatedExpressions':'placeholder', 'stackFrames':'placeholder',"
            + " 'variableTable':'placeholder'";

    String finalUpdate =
        quotes(String.format("{%s, %s}", "'id':'A', 'isFinalState':true", captureFields));
    Exception ex =
        assertThrows(
            Exception.class,
            () -> hubClient.transmitBreakpointUpdate("json", "A", finalUpdate.getBytes(UTF_8)));
    assertThat(ex)
        .hasMessageThat()
        .matches(
            String.format(
                "Firebase Database write operation to '%s' failed, error: 'FAKE DB ERROR'",
                snapshotBpPath));
  }

  @Test
  public void transmitBreakpointUpdateThrowsOnFinalPathFailure() throws Exception {
    HubClient hubClient = registerDebuggee();

    // For transmitUpdate to work, the client must first receive the breakpoint from the DB.
    String bpSnapshot =
        quotes(
            "{'A':{'id':'A', 'action':'SNAPSHOT', 'location':{'path':'Foo.java', 'line':9},"
                + " 'isFinalState':false}}");
    DataSnapshot dataSnapshot = buildDataSnapshotFromJsonString(bpSnapshot);
    breakpointUpdateListener.onDataChange(dataSnapshot);

    String debuggeeId = registeredDebuggee.id;
    String finalBpPath = String.format("cdbg/breakpoints/%s/final/A", debuggeeId);
    String snapshotBpPath = String.format("cdbg/breakpoints/%s/snapshot/A", debuggeeId);

    DatabaseReference mockFinalDbRef = mock(DatabaseReference.class);
    DatabaseReference mockSnapshotDbRef = mock(DatabaseReference.class);

    when(mockFirebaseDatabase.getReference(eq(finalBpPath))).thenReturn(mockFinalDbRef);
    when(mockFirebaseDatabase.getReference(eq(snapshotBpPath))).thenReturn(mockSnapshotDbRef);

    setCompletionSuccessDbSet(mockSnapshotDbRef);
    setCompletionFailedDbSet(mockFinalDbRef, "FAKE DB ERROR");

    String captureFields =
        "'evaluatedExpressions':'placeholder', 'stackFrames':'placeholder',"
            + " 'variableTable':'placeholder'";

    String finalUpdate =
        quotes(String.format("{%s, %s}", "'id':'A', 'isFinalState':true", captureFields));
    Exception ex =
        assertThrows(
            Exception.class,
            () -> hubClient.transmitBreakpointUpdate("json", "A", finalUpdate.getBytes(UTF_8)));
    assertThat(ex)
        .hasMessageThat()
        .matches(
            String.format(
                "Firebase Database write operation to '%s' failed, error: 'FAKE DB ERROR'",
                finalBpPath));
  }

  @Test
  public void transmitBreakpointUpdateThrowsOnActivePathFailure() throws Exception {
    HubClient hubClient = registerDebuggee();

    // For transmitUpdate to work, the client must first receive the breakpoint from the DB.
    String bpSnapshot =
        quotes(
            "{'A':{'id':'A', 'action':'SNAPSHOT', 'location':{'path':'Foo.java', 'line':9},"
                + " 'isFinalState':false}}");
    DataSnapshot dataSnapshot = buildDataSnapshotFromJsonString(bpSnapshot);
    breakpointUpdateListener.onDataChange(dataSnapshot);

    String debuggeeId = registeredDebuggee.id;
    String activeBpPath = String.format("cdbg/breakpoints/%s/active/A", debuggeeId);
    String finalBpPath = String.format("cdbg/breakpoints/%s/final/A", debuggeeId);
    String snapshotBpPath = String.format("cdbg/breakpoints/%s/snapshot/A", debuggeeId);

    DatabaseReference mockActiveDbRef = mock(DatabaseReference.class);
    DatabaseReference mockFinalDbRef = mock(DatabaseReference.class);
    DatabaseReference mockSnapshotDbRef = mock(DatabaseReference.class);

    when(mockFirebaseDatabase.getReference(eq(activeBpPath))).thenReturn(mockActiveDbRef);
    when(mockFirebaseDatabase.getReference(eq(finalBpPath))).thenReturn(mockFinalDbRef);
    when(mockFirebaseDatabase.getReference(eq(snapshotBpPath))).thenReturn(mockSnapshotDbRef);

    setCompletionSuccessDbSet(mockSnapshotDbRef);
    setCompletionSuccessDbSet(mockFinalDbRef);
    setCompletionFailedDbSet(mockActiveDbRef, "FAKE DB ERROR");

    String captureFields =
        "'evaluatedExpressions':'placeholder', 'stackFrames':'placeholder',"
            + " 'variableTable':'placeholder'";

    String finalUpdate =
        quotes(String.format("{%s, %s}", "'id':'A', 'isFinalState':true", captureFields));
    Exception ex =
        assertThrows(
            Exception.class,
            () -> hubClient.transmitBreakpointUpdate("json", "A", finalUpdate.getBytes(UTF_8)));
    assertThat(ex)
        .hasMessageThat()
        .matches(
            String.format(
                "Firebase Database write operation to '%s' failed, error: 'FAKE DB ERROR'",
                activeBpPath));
  }

  @Test
  public void transmitBreakpointUpdateModificationsCached() throws Exception {
    // For this test we modify the path, and then we finalize the breakpoint. We want to ensure the
    // finalized breakpoint written to the DB contains the modified path. This ensures the agent is
    // properly caching the BP update internally.
    //
    HubClient hubClient = registerDebuggee();

    // Send in the initial breakpoint.
    String bpSnapshot =
        quotes(
            "{'A':{'id':'A', 'action':'LOG', 'location':{'path':'Foo.java', 'line':5},"
                + " 'isFinalState':false}}");
    DataSnapshot dataSnapshot = buildDataSnapshotFromJsonString(bpSnapshot);
    breakpointUpdateListener.onDataChange(dataSnapshot);

    String debuggeeId = registeredDebuggee.id;
    String activeBpPath = String.format("cdbg/breakpoints/%s/active/A", debuggeeId);
    String finalBpPath = String.format("cdbg/breakpoints/%s/final/A", debuggeeId);
    DatabaseReference mockActiveDbRef = mock(DatabaseReference.class);
    DatabaseReference mockFinalDbRef = mock(DatabaseReference.class);
    when(mockFirebaseDatabase.getReference(eq(activeBpPath))).thenReturn(mockActiveDbRef);
    when(mockFirebaseDatabase.getReference(eq(finalBpPath))).thenReturn(mockFinalDbRef);
    setCompletionSuccessDbSet(mockFinalDbRef);
    setCompletionSuccessDbSet(mockActiveDbRef);

    // Modification here is line changed from 5 to 7.
    String bpUpdate = quotes("{'id':'A', 'location':{'path':'Foo.java', 'line':7}}");
    hubClient.transmitBreakpointUpdate("json", "A", bpUpdate.getBytes(UTF_8));

    // Here we set isFinalState to true to indicate it's complete
    String finalUpdate = quotes("{'id':'A', 'isFinalState':true}");
    hubClient.transmitBreakpointUpdate("json", "A", finalUpdate.getBytes(UTF_8));

    // Note, in the expected final object, the line should be 7.
    Object expectedFinalBpObject =
        createObjectFromJsonString(
            "{'id':'A', 'action':'LOG', 'location':{'path':'Foo.java', 'line':7},"
                + " 'isFinalState':True, 'finalTimeUnixMsec':{'.sv':'timestamp'}}");

    verify(mockFinalDbRef).setValue(eq(expectedFinalBpObject), any());
  }

  @Test
  public void computeDebuggeeIdWorksAsExpected() throws NoSuchAlgorithmException {
    FirebaseClient.Debuggee debuggee = new FirebaseClient.Debuggee();
    HashSet<String> ids = new HashSet<String>();

    // The arguments here don't actually matter, they won't factor into the test, we just need to
    // get an instance of the client.
    FirebaseClient firebaseClient =
        new FirebaseClient(
            metadata, classPathLookup, DEFAULT_LABELS, mockFirebaseStaticWrappers, timeouts);

    ImmutableMap<String, String> labels1 =
        ImmutableMap.<String, String>builder()
            .put("module", "m1")
            .put("version", "v1")
            .buildOrThrow();

    ImmutableMap<String, String> labels2 =
        ImmutableMap.<String, String>builder()
            .put("module", "m2")
            .put("version", "v1")
            .buildOrThrow();

    int count = 0;

    debuggee.setLabels(labels1);
    debuggee.setUniquifier("U1");
    debuggee.setDescription("D1");
    debuggee.setAgentVersion("A1");
    debuggee.setSourceContexts(null);
    ids.add(firebaseClient.computeDebuggeeId(debuggee));
    count++;

    debuggee.setLabels(labels2);
    ids.add(firebaseClient.computeDebuggeeId(debuggee));
    count++;

    debuggee.setUniquifier("U2");
    ids.add(firebaseClient.computeDebuggeeId(debuggee));
    count++;

    debuggee.setDescription("D2");
    ids.add(firebaseClient.computeDebuggeeId(debuggee));
    count++;

    debuggee.setAgentVersion("A2");
    ids.add(firebaseClient.computeDebuggeeId(debuggee));
    count++;

    String[] sourceContexts = {quotes("{'foo':'bar'}")};
    debuggee.setSourceContexts(sourceContexts);
    ids.add(firebaseClient.computeDebuggeeId(debuggee));
    count++;

    // Each computed ID should be unique, so the size should match the number of computed IDs.
    assertThat(ids).hasSize(count);

    // Set debuggee back to an earlier state and compute the ID, it should match one already in the
    // set.
    debuggee.setSourceContexts(null);
    String id = firebaseClient.computeDebuggeeId(debuggee);
    assertThat(ids).contains(id);
  }

  @Test(expected = UnsupportedOperationException.class)
  public void transmitBreakpointUpdateBadFormat() throws Exception {
    HubClient hubClient = registerDebuggee();
    hubClient.transmitBreakpointUpdate("protobuf", "bpid", new byte[0]);
  }

  @Test
  public void dbListenerFailureWorksAsExpected() throws Exception {
    // When the registration for breakpoint updates goes bad, the agent will cause
    // listActiveBreakpoints to begin failing to cause a new register debuggee operation. After this
    // list activeBreakpoints calls should begin succeeding again.

    HubClient hubClient = registerDebuggee();
    assertThat(hubClient.listActiveBreakpoints().getIsTimeout()).isTrue();

    // Not cause the failure
    DatabaseError error = mock(DatabaseError.class);
    doReturn("FAKE DB LISTENER FAILURE").when(error).getMessage();
    breakpointUpdateListener.onCancelled(error);

    // Now it should fail
    Exception ex = assertThrows(Exception.class, () -> hubClient.listActiveBreakpoints());
    assertThat(ex).hasMessageThat().isEqualTo("The Firebase Client needs to re-initialize.");

    // Setup and do registerDebuggee()
    DatabaseReference mockDebuggeesDbRef = mock(DatabaseReference.class);
    DatabaseReference mockBreakpointsDbRef = mock(DatabaseReference.class);
    when(mockFirebaseStaticWrappers.getDbInstance(mockFirebaseApp))
        .thenReturn(mockFirebaseDatabase);
    when(mockFirebaseDatabase.getReference(startsWith("cdbg/debuggees/")))
        .thenReturn(mockDebuggeesDbRef);
    when(mockFirebaseDatabase.getReference(startsWith("cdbg/breakpoints/")))
        .thenReturn(mockBreakpointsDbRef);
    setCompletionSuccessDbSet(mockDebuggeesDbRef);
    assertThat(hubClient.registerDebuggee(EMPTY_EXTRA_DEBUGGEE_LABELS)).isTrue();

    // Now listActiveBreakpoints() should succeed again.
    assertThat(hubClient.listActiveBreakpoints().getIsTimeout()).isTrue();
  }

  /** Common code to get the debuggee registered for all test cases requiring that. */
  private FirebaseClient registerDebuggee() throws Exception {
    return registerDebuggee(DEFAULT_LABELS, EMPTY_EXTRA_DEBUGGEE_LABELS);
  }

  /**
   * Common code to get the debuggee registered with extra labels provided in the registerDebuggee
   * call.
   */
  private FirebaseClient registerDebuggee(
      Map<String, String> gcpEnvironmentLabels, Map<String, String> extraLabels) throws Exception {

    DatabaseReference mockSchemaVersionDbRef = mock(DatabaseReference.class);
    DatabaseReference mockDebuggeesDbRef = mock(DatabaseReference.class);
    DatabaseReference mockBreakpointsDbRef = mock(DatabaseReference.class);

    when(mockFirebaseStaticWrappers.getDbInstance(mockFirebaseApp))
        .thenReturn(mockFirebaseDatabase);
    when(mockFirebaseDatabase.getReference(eq("cdbg/schema_version")))
        .thenReturn(mockSchemaVersionDbRef);
    when(mockFirebaseDatabase.getReference(startsWith("cdbg/debuggees/")))
        .thenReturn(mockDebuggeesDbRef);
    when(mockFirebaseDatabase.getReference(startsWith("cdbg/breakpoints/")))
        .thenReturn(mockBreakpointsDbRef);

    ArgumentCaptor<Object> capturedDebuggee = ArgumentCaptor.forClass(Object.class);
    ArgumentCaptor<String> capturedDbPaths = ArgumentCaptor.forClass(String.class);
    ArgumentCaptor<ValueEventListener> capturedBpUpdateListener =
        ArgumentCaptor.forClass(ValueEventListener.class);

    setResponseDbGet(mockSchemaVersionDbRef, "2");
    setCompletionSuccessDbSet(mockDebuggeesDbRef);

    FirebaseClient firebaseClient =
        new FirebaseClient(
            metadata, classPathLookup, gcpEnvironmentLabels, mockFirebaseStaticWrappers, timeouts);
    assertThat(firebaseClient.registerDebuggee(extraLabels)).isTrue();
    verify(mockDebuggeesDbRef).setValue(capturedDebuggee.capture(), any());
    this.registeredDebuggee = (FirebaseClient.Debuggee) capturedDebuggee.getValue();

    verify(mockFirebaseDatabase, times(3)).getReference(capturedDbPaths.capture());
    List<String> dbPaths = capturedDbPaths.getAllValues();
    assertThat(dbPaths)
        .isEqualTo(
            Arrays.asList(
                "cdbg/schema_version",
                String.format("cdbg/debuggees/%s", registeredDebuggee.id),
                String.format("cdbg/breakpoints/%s/active", registeredDebuggee.id)));

    verify(mockBreakpointsDbRef).addValueEventListener(capturedBpUpdateListener.capture());
    this.breakpointUpdateListener = capturedBpUpdateListener.getValue();
    assertThat(this.breakpointUpdateListener).isNotNull();

    reset(mockFirebaseDatabase);

    return firebaseClient;
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

  private String quotes(String str) {
    return str.replace("'", "\"");
  }

  private DataSnapshot buildDataSnapshotFromJsonString(String jsonString) {
    return buildDataSnapshotFromJsonObject(createObjectFromJsonString(jsonString));
  }

  private DataSnapshot buildDataSnapshotFromJsonObject(Object jsonObject) {
    DataSnapshot dataSnapshot = mock(DataSnapshot.class);
    doReturn(jsonObject).when(dataSnapshot).getValue();
    return dataSnapshot;
  }

  private Object createObjectFromJsonString(String jsonString) {
    Gson gson =
        new GsonBuilder()
            .setObjectToNumberStrategy(ToNumberPolicy.LONG_OR_DOUBLE)
            .serializeNulls()
            .create();
    return gson.fromJson(
        JsonParser.parseString(quotes(jsonString)).getAsJsonObject(), Object.class);
  }

  private ArrayList<Object> convertBreakpoints(byte[][] breakpoints) {
    ArrayList<Object> breakpointsJson = new ArrayList<Object>();
    for (byte[] bp : breakpoints) {
      breakpointsJson.add(
          createObjectFromJsonString(JsonParser.parseString(new String(bp, UTF_8)).toString()));
    }

    return breakpointsJson;
  }

  private void setResponseDbGet(DatabaseReference mockDbRef, Object jsonResponse) {
    doAnswer(
            new Answer<Void>() {
              public Void answer(InvocationOnMock invocation) {
                ValueEventListener eventListener =
                    (ValueEventListener) invocation.getArguments()[0];
                DataSnapshot dataSnapshot = mock(DataSnapshot.class);
                doReturn(jsonResponse).when(dataSnapshot).getValue();
                eventListener.onDataChange(dataSnapshot);
                return null;
              }
            })
        .when(mockDbRef)
        .addValueEventListener((ValueEventListener) notNull());
  }

  private void setOnCancelledDbGet(DatabaseReference mockDbRef, String errorMessage) {
    doAnswer(
            new Answer<Void>() {
              public Void answer(InvocationOnMock invocation) {
                ValueEventListener eventListener =
                    (ValueEventListener) invocation.getArguments()[0];
                DatabaseError error = mock(DatabaseError.class);
                doReturn(errorMessage).when(error).getMessage();
                eventListener.onCancelled(error);
                return null;
              }
            })
        .when(mockDbRef)
        .addValueEventListener((ValueEventListener) notNull());
  }

  private void setCompletionSuccessDbSet(DatabaseReference mockDbRef) {
    DatabaseError error = null; // indicates success
    setCompletionDbSet(mockDbRef, error);
  }

  private void setCompletionFailedDbSet(DatabaseReference mockDbRef, String errorMessage) {
    DatabaseError error = mock(DatabaseError.class);
    doReturn(errorMessage).when(error).getMessage();
    setCompletionDbSet(mockDbRef, error);
  }

  private void setCompletionDbSet(DatabaseReference mockDbRef, DatabaseError error) {
    doAnswer(
            new Answer<Void>() {
              public Void answer(InvocationOnMock invocation) {
                DatabaseReference.CompletionListener completionListener =
                    (DatabaseReference.CompletionListener) invocation.getArguments()[1];
                completionListener.onComplete(error, mockDbRef);
                return null;
              }
            })
        .when(mockDbRef)
        .setValue(any(), (DatabaseReference.CompletionListener) notNull());
  }
}
