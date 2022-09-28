/**
 * Copyright 2022 Google Inc. All Rights Reserved.
 *
 * <p>Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 *
 * <p>http://www.apache.org/licenses/LICENSE-2.0
 *
 * <p>Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.google.devtools.cdbg.debuglets.java;

import static com.google.devtools.cdbg.debuglets.java.AgentLogger.infofmt;
import static com.google.devtools.cdbg.debuglets.java.AgentLogger.severefmt;
import static com.google.devtools.cdbg.debuglets.java.AgentLogger.warnfmt;
import static java.nio.charset.StandardCharsets.UTF_8;

import com.google.firebase.FirebaseApp;
import com.google.firebase.FirebaseOptions;
import com.google.firebase.database.DataSnapshot;
import com.google.firebase.database.DatabaseError;
import com.google.firebase.database.DatabaseReference;
import com.google.firebase.database.FirebaseDatabase;
import com.google.firebase.database.ServerValue;
import com.google.firebase.database.ValueEventListener;
import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonElement;
import com.google.gson.JsonParser;
import com.google.gson.JsonSyntaxException;
import com.google.gson.ToNumberPolicy;
import com.google.gson.stream.JsonReader;
import com.google.gson.stream.JsonWriter;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.nio.file.Paths;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.SortedSet;
import java.util.TreeMap;
import java.util.TreeSet;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.TimeUnit;
import javax.xml.bind.DatatypeConverter;

class FirebaseClient implements HubClient {
  // Note, the Debuggee class is to be serialized automatically by the Firebase library. To support
  // this the following is purposely done here:
  //   1. All data members are public.
  //   2. The use of a List for sourceContexts instead of an array.
  static class Debuggee {
    public String id;
    public String uniquifier;
    public String description;
    public Map<String, String> labels;
    public String agentVersion;
    public List<Map<String, Object>> sourceContexts;

    public void setDebuggeeId(String debuggeeId) {
      this.id = debuggeeId;
    }

    public void setUniquifier(String uniquifier) {
      this.uniquifier = uniquifier;
    }

    public void setDescription(String description) {
      this.description = description;
    }

    public void setLabels(Map<String, String> labels) {
      this.labels = labels;
    }

    public void setAgentVersion(String agentVersion) {
      this.agentVersion = agentVersion;
    }

    public void setSourceContexts(String[] files) {
      if (files == null) {
        sourceContexts = null;
        return;
      }

      List<Map<String, Object>> sourceContextsList = new ArrayList<>();
      for (int i = 0; i < files.length; ++i) {
        try {
          sourceContextsList.add(
              GSON.fromJson(JsonParser.parseString(files[i]).getAsJsonObject(), Map.class));
        } catch (JsonSyntaxException e) {
          warnfmt(e, "Source context file could not be parsed as JSON: %s", files[i]);
          continue;
        }
      }

      this.sourceContexts = sourceContextsList;
    }
  }

  static class Timeout {
    Timeout(long value, TimeUnit units) {
      this.value = value;
      this.units = units;
    }

    long value;
    TimeUnit units;
  }
  ;

  /** The timeouts are configurable to allows the unit test to set lower timeouts */
  static class TimeoutConfig {
    TimeoutConfig(long value, TimeUnit units) {
      this.setDebuggee = new Timeout(value, units);
      this.setBreakpoint = new Timeout(value, units);
      this.getSchemaVersion = new Timeout(value, units);
      this.listActiveBreakpoints = new Timeout(value, units);
    }

    Timeout setDebuggee;
    Timeout setBreakpoint;
    Timeout getSchemaVersion;
    Timeout listActiveBreakpoints;
  }
  ;

  /**
   * Provide an interface for static methods of the Firebase admin sdk to allow mocking/injection
   * for testing purposes.
   */
  public static interface FirebaseStaticWrappers {
    /**
     * Initializes a named FirebaseApp instance using the given options.
     *
     * @param options The FirebaseOptions to use for the DB.
     * @param name Unique name for the app. It is an error to initialize an app with an already
     *     existing name. Starting and ending whitespace characters in the name are ignored
     *     (trimmed).
     * @return an instance of FirebaseApp
     * @throws IllegalStateException if an app with the same name has already been initialized.
     */
    public FirebaseApp initializeApp(FirebaseOptions options, String name);

    /**
     * Gets an instance of FirebaseDatabase for a specific FirebaseApp.
     *
     * @param app The FirebaseApp to get a FirebaseDatabase for.
     * @return A FirebaseDatabase instance.
     */
    public FirebaseDatabase getDbInstance(FirebaseApp app);
  }

  private static class FirebaseStaticWrappersImpl implements FirebaseStaticWrappers {
    @Override
    public FirebaseApp initializeApp(FirebaseOptions options, String name) {
      return FirebaseApp.initializeApp(options, name);
    }

    @Override
    public FirebaseDatabase getDbInstance(FirebaseApp app) {
      return FirebaseDatabase.getInstance(app);
    }
  }

  /** List of labels that (if defined) go into debuggee description. */
  private static final String[] DESCRIPTION_LABELS =
      new String[] {Labels.Debuggee.MODULE, Labels.Debuggee.VERSION, Labels.Debuggee.MINOR_VERSION};

  /** Name of the source context file. */
  private static final String SOURCE_CONTEXT_RESOURCE_NAME = "source-context.json";

  /**
   * Expected name of the git properties file. The file is generated by
   * https://github.com/ktoso/maven-git-commit-id-plugin which allows some flexibility in
   * configuring file name, keys prefixes etc. We currently support default value only (if file name
   * is changed, we don't know what we should look for).
   */
  private static final String GIT_PROPERTIES_RESOURCE_NAME = "git.properties";

  /**
   * Locations that will be searched to find the source-context.json file. For each class path entry
   * that represents a directory, these paths will be appended to the directory when searching. For
   * each class path entry that directly represents a jar file, these paths will be relative to the
   * root directory inside the jar file.
   */
  private static final String[] SOURCE_CONTEXT_RESOURCE_DIRS =
      new String[] {
        // The default location for the source context file is anywhere in the class path, or the
        // root of a jar that is directly in the classpath.
        ".",

        // In some frameworks, the user might put the source context file to a place not in the
        // class path or not in the root of the deploy jar. Extending the search of source context
        // to these additional directories should cover most such cases.
        "./WEB-INF/classes",
        "./META-INF/classes",
        "./BOOT-INF/classes"
      };

  private final TimeoutConfig timeouts;

  /** GCP project information and access token. */
  private final MetadataQuery metadata;

  /** Read and explore application resources. */
  private final ClassPathLookup classPathLookup;

  /** Debuggee labels (such as version and module) */
  private final Map<String, String> labels;

  /** Handle to the FirebaseApp as returned by FirebaseApp.initializeApp. */
  private FirebaseApp firebaseApp = null;

  /** Shutdown flag. Prevents any new resources that need to be cleaned up from being created. */
  private boolean isShutdown = false;

  /**
   * Flag to track if the agent is successfully registered with the backend. This flag is used by
   * the active breakpoints listener error handler to cause ListActiveBreakpoints to throw an
   * exception. When the native agent code receives the exception it will cause its run loop to go
   * back to calling registerDebuggee. We need to force this when the listener on the active
   * breakpoints node goes bad, so we can re-initialize, and get a good listener setup again on the
   * active breakpoints.
   */
  private boolean isRegistered = false;

  /**
   * Cache of unique identifier of the application resources. Computing uniquifier is an expensive
   * operation and we don't want to repeat it on every {@link FirebaseClient#registerDebuggee()}
   * call.
   */
  private String uniquifier = null;

  /**
   * Cache of source context files embedded into the application. We read these files only once and
   * reuse it in every {@link FirebaseClient#registerDebuggee()} call.
   */
  private String[] sourceContextFiles = null;

  /** Registered debuggee ID. Use getter/setter to access. */
  private String debuggeeId = "not-registered-yet";

  FirebaseStaticWrappers firebaseStaticWrappers = null;

  DatabaseReference activeBreakpointsRef = null;
  ValueEventListener activeBreakpointsListener = null;

  /**
   * Holds the most recent pending active breakpoint snapshot. This is the snapshot that
   * listActiveBreakpoints() needs to retrieve and return to the native agent code.
   *
   * <p>It is expressly created with a size of 1, as it only ever needs the most recent snapshot,
   * older unprocessed snapshots can be purged when a new snapshot arrives.
   *
   * <p>There is one reader and one writer. The writer is the breakpoints update callback that is
   * registered with the Firebase Admin SDK, and runs whenever the active breakpoints snapshot on
   * the Firebase RTDB changes. Everytime it runs, it will clear out the current entry (if any), and
   * add in the latest snapshot. The reader the native agent code which calls
   * listActiveBreakpoints() to stay up to date on the current snapshot it should have installed.
   *
   * <p>Entry is a Map<BreakpointId, BreakpointData>, where BreakpointId is a String, and
   * BreakpointData is a Map<String, Object>, where each key is a breakpoint field name that maps to
   * its value. Everything is a POJO that can be naturally serialized as a JSON document.
   */
  private ArrayBlockingQueue<Map<String, Map<String, Object>>> pendingActiveBreakpoints =
      new ArrayBlockingQueue<>(1);

  /**
   * Holds the current set of Active Breakpoints. It is used when the native agent code calls
   * transmitBreakpointUpdate(), the breakpoint is retrieved from here to fill in the gaps of the
   * breakpoint as we send the entire Breakpoint data tree to be update the node in the backing
   * Firebase RTDB. This 'filling in' is needed because the breakpoint data that comes up from the
   * native code does not contain all of the fields of the breakpoint, and so this ensures no
   * breakpoint fields are lost. This map also holds any modifications to the breakpoint the native
   * agent code may have made, such as an updated line location, since this data may not yet be in
   * the Firebase RTDB and we need to ensure we don't accidentally lose this local change.
   *
   * <p>There are two writers, and one reader. One writer is the breakpoints update callback that is
   * registered with the Firebase Admin SDK, and runs whenever the active breakpoints snapshot on
   * the Firebase RTDB changes. It will add new active breakpoints, and it will remove breakpoints
   * that are no longer active. However it will not update any breakpoint entries that are still
   * active, as they may contain a locally updated breakpoint location. The other writer, which is
   * also the reader, is the native agent code via its call to transmitBreakpointUpdate(). If the
   * breakpoint update being transmitted is not finalizing a breakpoint (eg. it's a source line
   * update), then it updates the particular breakpoint entry. It also acts as a reader when it
   * needs to combine the breakpoint fields provided from the native agent code with the complete
   * breakpoint to be written to the Firebase RTDB.
   *
   * <p>The map here is the current set of active breakpoints, it is a Map<BreakpointId,
   * BreakpointData>, where BreakpointId is a String, and BreakpointData is a Map<String, Object>,
   * where each key is a breakpoint field name that maps to its value. Everything is a POJO that can
   * be naturally serialized as a JSON document.
   */
  private ConcurrentHashMap<String, Map<String, Object>> currentBreakpoints =
      new ConcurrentHashMap<>();

  private static final Gson GSON =
      new GsonBuilder()
          .setObjectToNumberStrategy(ToNumberPolicy.LONG_OR_DOUBLE)
          .serializeNulls()
          .create();

  /** Default constructor using environment provided by {@link GcpEnvironment}. */
  public FirebaseClient() {
    this.metadata = GcpEnvironment.getMetadataQuery();
    this.classPathLookup = ClassPathLookup.defaultInstance;
    this.labels = GcpEnvironment.getDebuggeeLabels();
    this.firebaseStaticWrappers = new FirebaseStaticWrappersImpl();

    // Initialize everything with the same timeout, then make specific overrides where desired.
    this.timeouts = new TimeoutConfig(30, TimeUnit.SECONDS);
    this.timeouts.getSchemaVersion.value = 10;
  }

  /**
   * Class constructor Visible for testing
   *
   * @param metadata GCP project information and access token
   * @param classPathLookup read and explore application resources
   * @param labels debuggee labels (such as version and module)
   * @param firebaseStaticWrappers implementation of Firebase static api calls
   * @param the timeout values to use
   */
  public FirebaseClient(
      MetadataQuery metadata,
      ClassPathLookup classPathLookup,
      Map<String, String> labels,
      FirebaseStaticWrappers firebaseStaticWrappers,
      TimeoutConfig timeouts) {
    this.metadata = metadata;
    this.classPathLookup = classPathLookup;
    this.labels = labels;
    this.firebaseStaticWrappers = firebaseStaticWrappers;
    this.timeouts = timeouts;
  }

  @Override
  public boolean registerDebuggee(Map<String, String> extraDebuggeeLabels) throws Exception {
    // No point going on if we don't have environment information.
    if (metadata.getProjectId().isEmpty() || metadata.getGoogleCredential() == null) {
      severefmt("Environment not available");
      throw new Exception("Environment not available");
    }

    // We simply allow any exceptions from the helper methods to escape out. When registerDebuggee()
    // throws an exception, it indicates the registration failed to the native agent code, which
    // will wait an appropriate amount of time and then attempt to call registerDebuggee() again.

    initializeFirebaseApp();

    Debuggee debuggee = getDebuggeeInfo(extraDebuggeeLabels);
    setDebuggeeId(debuggee.id);
    String debuggeePath = "cdbg/debuggees/" + getDebuggeeId();
    setDbValue(debuggeePath, debuggee, timeouts.setDebuggee);

    registerActiveBreakpointListener();
    isRegistered = true;

    infofmt(
        "Debuggee %s, registered %s, agent version: %s",
        getDebuggeeId(), GSON.toJsonTree(debuggee), GcpDebugletVersion.VERSION);

    return true;
  }

  @Override
  public ListActiveBreakpointsResult listActiveBreakpoints() throws Exception {
    if (!isRegistered) {
      // This occurs when the active breakpoints listener encounters an error and we need to force a
      // re-initialization. When this method throws an exception, the native agent run loop will go
      // back to calling registerDebuggee() until successful, at which point it will begin calling
      // listActiveBreakpoints() once again.
      warnfmt("The isRegistered flag is false, we need to re initialize.");
      throw new Exception("The Firebase Client needs to re-initialize.");
    }

    // Map<BreakpointId, BreakpointData> where BreakpointId is a string and BreakpointData is a
    // Map<String, Object>, which is a mapping of the field names to their values. Each value is
    // represented as POJO object that represents a JSON value.
    Map<String, Map<String, Object>> snapshot = null;
    try {
      snapshot =
          pendingActiveBreakpoints.poll(
              timeouts.listActiveBreakpoints.value, timeouts.listActiveBreakpoints.units);
    } catch (InterruptedException e) {
      infofmt("Wait for active breakpoints update interrupted: %s", e);
    }

    if (snapshot == null) {
      // Reaching this point means there is no new breakpoint updates to send back down to the
      // native agent code.  Either pendingActiveBreakpoints.poll() returned null, which indicates a
      // timeout, or an InterruptedException was caught, which we'll also simply treat as a timeout
      // for the return to the native code.
      return LIST_ACTIVE_BREAKPOINTS_TIMEOUT;
    }

    infofmt("listActiveBreakpoints processing %d breakpoints.", snapshot.size());

    byte[][] serializedBreakpoints = new byte[(int) snapshot.size()][];

    try {
      int i = 0;
      for (Map.Entry<String, Map<String, Object>> entry : snapshot.entrySet()) {
        String breakpointId = entry.getKey();
        Map<String, Object> breakpoint = (Map<String, Object>) entry.getValue();

        infofmt("FirebaseClient: Serializing breakpoint %s for internal agent", breakpointId);

        // This is simply a precaution, it's expected the 'id' would already be present within the
        // brakpoint data.
        if (!breakpoint.containsKey("id")) {
          breakpoint = new TreeMap<>(breakpoint);
          breakpoint.put("id", breakpointId);
        }

        ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
        JsonWriter writer = new JsonWriter(new OutputStreamWriter(outputStream, UTF_8));
        GSON.toJson(GSON.toJsonTree(breakpoint), writer);
        writer.flush();
        serializedBreakpoints[i] = outputStream.toByteArray();
        i++;
      }
    } catch (Exception e) {
      warnfmt("An unexpected error occurred trying to serialize the breakpoints: '%s'", e);
      throw e;
    }

    final byte[][] breakpoints = serializedBreakpoints;

    return new ListActiveBreakpointsResult() {
      @Override
      public boolean getIsTimeout() {
        return false;
      }

      @Override
      public String getFormat() {
        return "json";
      }

      @Override
      public byte[][] getActiveBreakpoints() {
        return breakpoints;
      }
    };
  }

  @Override
  public void transmitBreakpointUpdate(String format, String breakpointId, byte[] breakpoint)
      throws Exception {
    if (!"json".equals(format)) {
      throw new UnsupportedOperationException("FirebaseClient only supports json format");
    }

    Map<String, Object> breakpointObject = currentBreakpoints.get(breakpointId);
    if (breakpointObject == null) {
      // If we don't find it, that means there was a race and another agent already completed the
      // breakpoint, so we can simply return here without further processing.
      infofmt(
          "Breakpoint id '%s' has already been removed locally, ignoring the update.",
          breakpointId);
      return;
    }

    try {
      ByteArrayInputStream inputStream = new ByteArrayInputStream(breakpoint);
      JsonReader reader = new JsonReader(new InputStreamReader(inputStream, UTF_8));
      Map<String, Object> updatedBreakpointObject = GSON.fromJson(reader, Map.class);
      // Not all of the breakpoint fields that exist get returned from the native code, so here we
      // copy over anything that came from the native side of the agent, but otherwise we keep
      // everything that was present from the Firebase side.
      breakpointObject.putAll(updatedBreakpointObject);
    } catch (Exception e) {
      warnfmt(
          "An unexpected error occurred trying to deserialize breakpoint id '%s', error: '%s'",
          breakpointId, e);

      // We're simply returning here, which has the effect of swallowing the breakpoint. If we throw
      // an exception, the native code will simply wait a bit and attempt to resend, however in this
      // scenario a retry is not helpful.
      return;
    }

    // By default isFinalState is considered false. It is only true if the field is present, and has
    // a value of true.
    boolean isFinalState = false;
    Object object = breakpointObject.get("isFinalState");
    if (object instanceof Boolean) {
      isFinalState = (Boolean) object;
    }

    // A breakpoint is a logpoint only if the 'action' field is present and is set to 'LOG', and if
    // a breakpoint is not a logpoint, then it is otherwise a snaphsot.
    boolean isLogpoint = false;
    object = breakpointObject.get("action");
    if (object instanceof String) {
      isLogpoint = ((String) object).equals("LOG");
    }
    boolean isSnapshot = !isLogpoint;

    String activeBpPath =
        String.format("cdbg/breakpoints/%s/active/%s", getDebuggeeId(), breakpointId);

    infofmt("Breakpoint ID %s is being %s.", breakpointId, isFinalState ? "finalized" : "updated");

    if (!isFinalState) {
      // If the Breakpoint is still active, we must update it in the map so we don't lose the
      // updated information, which is likely an update to the source location, adjusting the line
      // number. We must also update it in the Firebase Database.
      //
      // To note, setDbValue will throw on a failure. We allow this failure to propagate out, it
      // will indicate to the native agent code that it should retry sending the breakpoint update.
      currentBreakpoints.replace(breakpointId, breakpointObject);
      setDbValue(activeBpPath, breakpointObject, timeouts.setBreakpoint);
    } else {
      breakpointObject.put("finalTimeUnixMsec", ServerValue.TIMESTAMP);

      if (isSnapshot) {
        // Note, there may not be any snapshot data present, but final snapshots always get written
        // to the 'snapshot' node regardless.
        String snapshotBpPath =
            String.format("cdbg/breakpoints/%s/snapshot/%s", getDebuggeeId(), breakpointId);
        setDbValue(snapshotBpPath, breakpointObject, timeouts.setBreakpoint);

        // Now strip potential snapshot data for the write to the 'final' node.
        breakpointObject = new TreeMap<String, Object>(breakpointObject);
        breakpointObject.remove("evaluatedExpressions");
        breakpointObject.remove("stackFrames");
        breakpointObject.remove("variableTable");
      }

      String finalBpPath =
          String.format("cdbg/breakpoints/%s/final/%s", getDebuggeeId(), breakpointId);
      setDbValue(finalBpPath, breakpointObject, timeouts.setBreakpoint);

      // We need to delete the breakpoint from the active breakpoints list, passing in null as the
      // value will delete it. We also choose to do this one last, as we don't want to delete the
      // active breakpoint until the finalized version is successfully written out.
      setDbValue(activeBpPath, null, timeouts.setBreakpoint);
    }
  }

  @Override
  public void registerBreakpointCanary(String breakpointId) throws Exception {
    throw new UnsupportedOperationException(
        "FirebaseClient#registerBreakpointCanary not implemented");
  }

  @Override
  public void approveBreakpointCanary(String breakpointId) throws Exception {
    throw new UnsupportedOperationException(
        "FirebaseClient#approveBreakpointCanary not implemented");
  }

  /** If there is any reason to not enable the debugger, return false here. */
  @Override
  public boolean isEnabled() {
    return true;
  }

  @Override
  public void shutdown() {
    infofmt("FirebaseClient::shutdown() begin");
    isShutdown = true;
    metadata.shutdown();
    unregisterActiveBreakpointListener();
    deleteFirebaseApp();
    infofmt("FirebaseClient::shutdown() end");
  }

  public String getDebuggeeId() {
    return debuggeeId;
  }

  private void setDebuggeeId(String id) {
    debuggeeId = id;
  }

  /**
   * Fills in the debuggee registration request message.
   *
   * @param extraDebuggeeLabels Extra labels to include in the Debuggee. These are to be in addition
   *     to the labels already collected locally that are to be included.
   * @return debuggee instance to use
   */
  private Debuggee getDebuggeeInfo(Map<String, String> extraDebuggeeLabels)
      throws NoSuchAlgorithmException, IOException {
    // Read source context files if not done yet.
    if (sourceContextFiles == null) {
      // Using a set eliminates duplicate source context files (i.e., the same file added twice).
      SortedSet<String> resources = new TreeSet<>();

      for (String sourceContextDir : SOURCE_CONTEXT_RESOURCE_DIRS) {
        String sourceContextFilePath =
            Paths.get(sourceContextDir, SOURCE_CONTEXT_RESOURCE_NAME).normalize().toString();
        Collections.addAll(
            resources, classPathLookup.readApplicationResource(sourceContextFilePath));
        Collections.addAll(
            resources, new AppPathLookup().readApplicationResource(sourceContextFilePath));
      }

      // If SOURCE_CONTEXT_RESOURCE_NAME is not found, we try GIT_PROPERTIES_RESOURCE_NAME
      // When searching for GIT_PROPERTIES_RESOURCE_NAME file, do not look at AppPathLookup.
      if (resources.isEmpty()) {
        for (String sourceContextDir : SOURCE_CONTEXT_RESOURCE_DIRS) {
          String gitPropertiesFilePath =
              Paths.get(sourceContextDir, GIT_PROPERTIES_RESOURCE_NAME).normalize().toString();
          Collections.addAll(
              resources,
              classPathLookup.readGitPropertiesResourceAsSourceContext(gitPropertiesFilePath));
        }
      }

      sourceContextFiles = resources.toArray(new String[0]);
    }

    Map<String, String> allLabels = new TreeMap<>();
    allLabels.putAll(extraDebuggeeLabels);
    allLabels.putAll(labels);
    allLabels.put("projectid", metadata.getProjectId());

    if (uniquifier == null) {
      boolean hasSourceContext = ((sourceContextFiles != null) && (sourceContextFiles.length > 0));

      // Compute uniquifier of debuggee properties. Start with an initial 20B sha-1 hash value.
      uniquifier = "DA39A3EE5E6B4B0D3255BFEF95601890AFD80709";

      if (!allLabels.containsKey(Labels.Debuggee.MINOR_VERSION) && !hasSourceContext) {
        // There is no source context and no minor version. It means that different versions of the
        // application may be running with the same debuggee properties. Hash of application
        // binaries to generate different debuggees in this case.
        uniquifier = classPathLookup.computeDebuggeeUniquifier(uniquifier);
      }
    }

    Debuggee debuggee = new Debuggee();
    debuggee.setUniquifier(uniquifier);
    debuggee.setDescription(getDebuggeeDescription());
    debuggee.setLabels(allLabels);
    debuggee.setAgentVersion(
        String.format(
            "cloud-debug-java/v%d.%d",
            GcpDebugletVersion.MAJOR_VERSION, GcpDebugletVersion.MINOR_VERSION));
    debuggee.setSourceContexts(sourceContextFiles);

    String debuggeeId = computeDebuggeeId(debuggee);
    debuggee.setDebuggeeId(debuggeeId);

    return debuggee;
  }

  /**
   * Formats debuggee description based on GCP project metadata and labels.
   *
   * @return debuggee description
   */
  private String getDebuggeeDescription() {
    StringBuilder description = new StringBuilder();
    description.append(metadata.getProjectId());

    if (labels.containsKey(GcpEnvironment.DEBUGGEE_DESCRIPTION_SUFFIX_LABEL)) {
      description.append(labels.get(GcpEnvironment.DEBUGGEE_DESCRIPTION_SUFFIX_LABEL));
      return description.toString();
    }

    for (String key : DESCRIPTION_LABELS) {
      if (labels.containsKey(key)) {
        description.append('-');
        description.append(labels.get(key));
      }
    }

    return description.toString();
  }

  /**
   * Computes the debuggee ID to use for the given debuggee. Visible for testing.
   *
   * @param debuggee the debuggee to compute the ID for.
   * @return the debuggee ID, in string form.
   */
  public String computeDebuggeeId(Debuggee debuggee) throws NoSuchAlgorithmException {
    MessageDigest hash = MessageDigest.getInstance("SHA1");

    // Ideally we could just generate a Json string and then hash that, however with Gson it seems
    // there's no easy way to ensure the keys in the maps get output in a deterministic order.
    updateHash(hash, GSON.toJsonTree(debuggee));
    return "d-" + DatatypeConverter.printHexBinary(hash.digest()).substring(0, 8).toLowerCase();
  }

  /**
   * Updates the current hash with the given JSON element. This is a recursive call, and for all map
   * object will iterate over the keys/json fields in order to ensure the same hash will be computed
   * for the same JSON content.
   *
   * @param hash the current hash to add to
   * @param element the json element to add to the hash.
   */
  private static void updateHash(MessageDigest hash, JsonElement element) {
    if (element.isJsonArray()) {
      for (JsonElement e : element.getAsJsonArray()) {
        updateHash(hash, e);
      }
    } else if (element.isJsonObject()) {
      TreeMap<String, JsonElement> sortedElements = new TreeMap<>();
      for (Map.Entry<String, JsonElement> kv : element.getAsJsonObject().entrySet()) {
        sortedElements.put(kv.getKey(), kv.getValue());
      }
      for (Map.Entry<String, JsonElement> entry : sortedElements.entrySet()) {
        updateHash(hash, entry.getValue());
      }
    } else if (!element.isJsonNull()) {
      hash.update(element.toString().getBytes(UTF_8));
    }
  }

  /**
   * Configures a FirebaseApp instance and verifies the Firebase RTDB URL. On success, the value
   * this.firebaseApp will be correctly set and the method returns normally, throws an Exception on
   * error. Visible for testing
   *
   * @throws Exception when an error occurs or no valid database URL could be found.
   */
  void initializeFirebaseApp() throws Exception {
    // Once we've successfully called FirebaseApp.initializeApp, there's no need to call it again.
    // Also unless we call delete on the FirebaseApp, it will throw an exception.
    if (firebaseApp != null) {
      return;
    }

    ArrayList<String> urls = new ArrayList<>();
    String configuredUrl = GcpEnvironment.getFirebaseDatabaseUrlString();
    if (!configuredUrl.isEmpty()) {
      urls.add(configuredUrl);
    } else {
      urls.add("https://" + metadata.getProjectId() + "-cdbg.firebaseio.com");
      urls.add("https://" + metadata.getProjectId() + "-default-rtdb.firebaseio.com");
    }

    for (String databaseUrl : urls) {
      this.firebaseApp = initializeFirebaseAppForUrl(databaseUrl);
      if (this.firebaseApp != null) {
        infofmt("Successfully initialized the FirebaseApp with url %s", databaseUrl);
        break;
      }
    }

    if (this.firebaseApp == null) {
      String errorMessage = "Failed to initialize FirebaseApp, attempted URls: " + urls;
      warnfmt(errorMessage);
      throw new Exception(errorMessage);
    }
  }

  /**
   * Initializes and tests a FirebaseApp with the given database url. If the DB is configured for
   * Debugger use, then the FirebaseApp is returned, otherwise null is returned.
   *
   * @param databaseUrl the URL of the Firebase RTDB to use.
   * @return The FirebaseApp instance on success, null on failure.
   */
  private FirebaseApp initializeFirebaseAppForUrl(String databaseUrl) {
    // Based on the documentation, setting the read and connect timeouts does not affect the
    // FirebaseDatabase APIs, but it still seems reasonable to set the values to a know value,
    // just as a precaution.
    // https://firebase.google.com/docs/reference/admin/java/reference/com/google/firebase/FirebaseOptions.Builder#public-firebaseoptions.builder-setconnecttimeout-int-connecttimeout
    // https://firebase.google.com/docs/reference/admin/java/reference/com/google/firebase/FirebaseOptions.Builder#public-firebaseoptions.builder-setreadtimeout-int-readtimeout
    FirebaseOptions options =
        FirebaseOptions.builder()
            .setCredentials(this.metadata.getGoogleCredential())
            .setDatabaseUrl(databaseUrl)
            .setConnectTimeout(30 * 1000)
            .setReadTimeout(30 * 1000)
            .build();

    FirebaseApp app = null;
    boolean isAppGoodToUse = false;

    try {
      app = this.firebaseStaticWrappers.initializeApp(options, "SnapshotDebugger");

      infofmt(
          "Attempting to verify if db %s exists and is configured for the Snapshot Debugger",
          databaseUrl);

      Object value =
          getDbValue(
              app, this.firebaseStaticWrappers, "cdbg/schema_version", timeouts.getSchemaVersion);

      if (value != null) {
        // For our purposes, we don't care what the data is, as long long as it's not null it
        // indicates the DB exists and has been initialized for Snapshot Debugger use.
        infofmt("Successfully initialized FirebaseApp with db '%s'", databaseUrl);
        isAppGoodToUse = true;
      }
    } catch (Exception e) {
      infofmt(
          "Failed to find a Snapshot Debugger configured db at '%s', error: %s", databaseUrl, e);
    } finally {
      if (!isAppGoodToUse && app != null) {
        // The delete is important, otherwise subsequant calls to FirebaseApp.initializeApp()
        // using the same app instance name will throw an IllegalStateException.
        app.delete();
      }
    }

    return isAppGoodToUse ? app : null;
  }

  /** Returns the FirebaseApp instance to use. */
  private synchronized FirebaseApp getFirebaseApp() throws RuntimeException {
    // If we've been shutdown, don't create any new resources, the shutdown() will be calling
    // unregisterActiveBreakpointListener(), we don't want to inadvertently create a new listener
    // after this.
    if (isShutdown) {
      throw new RuntimeException("Shutdown in progress");
    }

    if (firebaseApp == null) {
      throw new RuntimeException("FirebaseApp is not initialized!!");
    }

    // Note, it may seem there is still a race here with shutdown() and deleteFirebaseApp(),
    // however, even if another piece of code got a reference to the app, then the delete occurred,
    // and the code then attempted to use the reference, the call will simply thrown an exception:
    // https://firebase.google.com/docs/reference/admin/java/reference/com/google/firebase/FirebaseApp#public-void-delete
    return firebaseApp;
  }

  private synchronized void deleteFirebaseApp() {
    if (firebaseApp != null) {
      firebaseApp.delete();
      firebaseApp = null;
    }
  }

  /**
   * Registers the active breakpoint listener. This is the mechanism the agent uses to find out
   * about any changes to the active breakpoints in the Firebase RTDB. After first registration the
   * callback will run to provide the current active breakpoints snapshots.
   */
  private synchronized void registerActiveBreakpointListener() throws RuntimeException {
    // If we've been shutdown, don't create any new resources, the shutdown() will be calling
    // unregisterActiveBreakpointListener(), we don't want to inadvertently create a new listener
    // after this.
    if (isShutdown) {
      throw new RuntimeException("Shutdown in progress");
    }

    unregisterActiveBreakpointListener();

    String path = "cdbg/breakpoints/" + getDebuggeeId() + "/active";
    activeBreakpointsRef =
        this.firebaseStaticWrappers.getDbInstance(getFirebaseApp()).getReference(path);

    // Install a listener at the top of the active breakpoints tree for the debuggee. Whenever any
    // active breakpoint changes occur (breakpoint is modified/added/deleted), for full snapshot of
    // the new state of the active breakpoint tree will be passed into the onDataChange(). In
    // addition, it will initially be called immediately to provide the current snapshot.
    activeBreakpointsListener =
        activeBreakpointsRef.addValueEventListener(
            new ValueEventListener() {
              @Override
              public void onDataChange(DataSnapshot dataSnapshot) {
                // The data snapshot contains the current state of the active breakpoint tree, and
                // this method is called any time a breakpoint is updated/added/deleted.
                // dataSnapshot.getValue() returns a POJO that represents a JSON object. In the case
                // here, based on how the RTDB is setup/used, it will be a map<String, Map<String,
                // Object>>, which maps a breakpoint ID to a breakpoint, where each breakpoint in
                // turn is a map of field names to their corresponding values.
                Object document = dataSnapshot.getValue();
                infofmt("Active breakpoint snapshot from the Firebase RTDB: %s", document);

                // If the value is null, that means the path is now empty, ie all breakpoints have
                // been removed.
                Map<String, Map<String, Object>> snapshotMap =
                    (Map<String, Map<String, Object>>) document;
                if (document == null) {
                  snapshotMap = new HashMap<>();
                }

                for (Map.Entry<String, Map<String, Object>> entry : snapshotMap.entrySet()) {
                  currentBreakpoints.putIfAbsent(entry.getKey(), entry.getValue());
                }
                currentBreakpoints.keySet().retainAll(snapshotMap.keySet());

                pendingActiveBreakpoints.clear();

                // The offer is expected to always succeed as we've just done a clear and this is
                // the only code path that adds to the queue.
                if (!pendingActiveBreakpoints.offer(new HashMap(currentBreakpoints))) {
                  severefmt("Unexpected failure adding to pendingActiveBreakpoints");
                }
              }

              @Override
              public void onCancelled(DatabaseError error) {
                warnfmt("Database subscription error: %s", error.getMessage());

                // This will force listActiveBreakpoints() to throw an exception and get the native
                // agent code to begin calling registerDebuggee() again so the agent code can get
                // back to a working registered state.
                isRegistered = false;
              }
            });
  }

  synchronized void unregisterActiveBreakpointListener() {
    if (activeBreakpointsListener != null) {
      activeBreakpointsRef.removeEventListener(activeBreakpointsListener);
      activeBreakpointsListener = null;
    }
  }

  /**
   * Helper to set the data at a given path in the Firebase RTDB. Returns normally on success,
   * throws an Exception on timeout or a write error.
   *
   * @param path The database path to write the value to.
   * @param value The data to write. It should be a POJO that can be serialized to JSON.
   * @param timeout How long to wait for confirmation the database was successfully written to.
   * @throws Exception if the write timesout or if an outright error was reported.
   */
  private void setDbValue(String path, Object value, Timeout timeout) throws Exception {
    DatabaseReference dbRef =
        this.firebaseStaticWrappers.getDbInstance(getFirebaseApp()).getReference(path);

    // A value of empty string indicates success, otherwise the string is an error message.
    final ArrayBlockingQueue<String> result = new ArrayBlockingQueue<>(1);
    infofmt("Beginning Firebase Database write operation to '%s'", path);
    dbRef.setValue(
        value,
        new DatabaseReference.CompletionListener() {
          @Override
          public void onComplete(DatabaseError error, DatabaseReference ref) {
            String errorString = (error == null) ? "" : error.getMessage();
            result.offer(errorString);
          }
        });

    // Returns null on timeout.
    String error = result.poll(timeout.value, timeout.units);
    if (error == null) {
      error = "Timed out after " + timeout.value + " " + timeout.units.toString();
    }

    if (!error.isEmpty()) {
      String msg =
          "Firebase Database write operation to '" + path + "' failed, error: '" + error + "'";
      warnfmt(msg);
      throw new Exception(msg);
    }

    infofmt("Firebase Database write operation to '%s' was successful", path);
  }

  /**
   * Helper to read the data at a given path in the Firebase RTDB. Returns the data on success,
   * throws an Exception on timeout or a read error.
   *
   * @param app The FirebaseApp instance to read from
   * @param firebaseStaticWrappers The static wrappers to use, requires the getDbInstance method
   * @param path The database path to read from
   * @param timeout How long to wait for the read to complete
   * @return Object The data read, it is a POJO that represents a JSON document
   * @throws Exception if the write timesout or if an outright error was reported.
   */
  private static Object getDbValue(
      FirebaseApp app,
      FirebaseStaticWrappers firebaseStaticWrappers,
      final String path,
      Timeout timeout)
      throws Exception {
    DatabaseReference dbRef = firebaseStaticWrappers.getDbInstance(app).getReference(path);

    infofmt("Beginning Firebase Database read operation at '%s'", path);

    ValueEventListener listener = null;

    final ArrayBlockingQueue<Boolean> resultObtained = new ArrayBlockingQueue<>(1);
    final ArrayList<Object> obtainedValue = new ArrayList<>();

    try {
      dbRef.addValueEventListener(
          new ValueEventListener() {
            @Override
            public void onDataChange(DataSnapshot dataSnapshot) {
              Object value = dataSnapshot.getValue();
              infofmt(
                  "Response obtained, data was %s found at %s", value == null ? "not" : "", path);

              obtainedValue.add(value);
              resultObtained.offer(Boolean.TRUE);
            }

            @Override
            public void onCancelled(DatabaseError error) {
              warnfmt("Database subscription error: %s", error.getMessage());
              resultObtained.offer(Boolean.FALSE);
            }
          });

      // Returns null on timeout.
      Boolean isSuccess = resultObtained.poll(timeout.value, timeout.units);

      // null will be returned on read timeout.
      if (isSuccess == null) {
        infofmt("Read from %s timed out after %d %s", path, timeout.value, timeout.units);
        throw new Exception("Read timed out");
      } else if (!isSuccess) {
        throw new Exception("Error occurred attempting to read from the DB");
      }
    } finally {
      if (listener != null) {
        dbRef.removeEventListener(listener);
      }
    }

    return obtainedValue.get(0);
  }
}
