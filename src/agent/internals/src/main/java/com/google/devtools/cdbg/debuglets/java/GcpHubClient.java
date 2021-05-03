/**
 * Copyright 2015 Google Inc. All Rights Reserved.
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

import static com.google.devtools.cdbg.debuglets.java.AgentLogger.info;
import static com.google.devtools.cdbg.debuglets.java.AgentLogger.infofmt;
import static com.google.devtools.cdbg.debuglets.java.AgentLogger.warnfmt;
import static java.nio.charset.StandardCharsets.UTF_8;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonDeserializationContext;
import com.google.gson.JsonDeserializer;
import com.google.gson.JsonElement;
import com.google.gson.JsonParser;
import com.google.gson.JsonSerializationContext;
import com.google.gson.JsonSerializer;
import com.google.gson.JsonSyntaxException;
import com.google.gson.stream.JsonWriter;
import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.io.Reader;
import java.io.Writer;
import java.lang.reflect.Type;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLEncoder;
import java.nio.file.Paths;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.SortedSet;
import java.util.TreeMap;
import java.util.TreeSet;

class GcpHubClient implements HubClient {
  /** HTTP request and response of RegisterDebuggee message for GSON deserialization. */
  static class RegisterDebuggeeMessage {
    @SuppressWarnings("unused")
    static class Debuggee {
      private String id;
      private String project;
      private String uniquifier;
      private String description;
      private Map<String, String> labels;
      private String agentVersion;
      private JsonElement[] sourceContexts;
      private Boolean isDisabled;
      private String canaryMode;
    }

    private Debuggee debuggee = new Debuggee();

    // The field will be set by the gson serialization, so no need for a setter.
    private String agentId = null;

    public void setProject(String project) {
      debuggee.project = project;
    }

    public void setUniquifier(String uniquifier) {
      debuggee.uniquifier = uniquifier;
    }

    public void setDescription(String description) {
      debuggee.description = description;
    }

    public void setLabels(Map<String, String> labels) {
      debuggee.labels = labels;
    }

    public void setAgentVersion(String agentVersion) {
      debuggee.agentVersion = agentVersion;
    }

    public void setSourceContexts(String[] files) {
      if (files == null) {
        debuggee.sourceContexts = null;
        return;
      }

      JsonParser parser = new JsonParser();
      List<JsonElement> sourceContextsList = new ArrayList<>();
      for (int i = 0; i < files.length; ++i) {
        try {
          sourceContextsList.add(parser.parse(files[i]));
        } catch (JsonSyntaxException e) {
          warnfmt(e, "Source context file could not be parsed as JSON: %s", files[i]);
          continue;
        }
      }

      debuggee.sourceContexts = sourceContextsList.toArray(new JsonElement[0]);
    }

    public void setCanaryMode(String canaryMode) {
      debuggee.canaryMode = canaryMode;
    }

    public String getDebuggeeId() {
      return (debuggee == null) ? null : debuggee.id;
    }

    public boolean getIsDisabled() {
      if (debuggee == null) {
        return false;
      }

      if (debuggee.isDisabled == null) {
        return false; // Default.
      }

      return debuggee.isDisabled;
    }

    public String getCanaryMode() {
      return (debuggee == null) ? null : debuggee.canaryMode;
    }

    public String getAgentId() {
      return agentId;
    }
  }

  /** HTTP response of ListActiveBreakpoints message for GSON deserialization. */
  static class ListActiveBreakpointsResponse {
    private String nextWaitToken;
    private boolean waitExpired;
    private JsonElement[] breakpoints;

    public String getNextWaitToken() {
      return nextWaitToken;
    }

    public boolean getWaitExpired() {
      return waitExpired;
    }

    public byte[][] serializeBreakpoints() throws IOException {
      if (breakpoints == null) {
        return new byte[0][];
      }

      byte[][] serializedBreakpoints = new byte[breakpoints.length][];
      for (int i = 0; i < breakpoints.length; ++i) {
        ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
        JsonWriter writer = new JsonWriter(new OutputStreamWriter(outputStream, UTF_8));
        gson.toJson(breakpoints[i], writer);
        writer.flush();
        serializedBreakpoints[i] = outputStream.toByteArray();
      }

      return serializedBreakpoints;
    }
  }

  /**
   * Helper class to automatically insert and remove connection from {@link
   * GcpHubClient#activeConnections}.
   */
  private final class ActiveConnection implements AutoCloseable {
    private final HttpURLConnection connection;

    public ActiveConnection(HttpURLConnection connection) {
      this.connection = connection;

      synchronized (activeConnections) {
        if (isShutdown) {
          // We don't need to call "disconnect" here. "URL.openConnection" doesn't actually
          // establish the connection, so calling "disconnect" will have no effect.
          throw new RuntimeException("Shutdown in progress");
        }

        activeConnections.add(this);
      }
    }

    @Override
    public void close() {
      synchronized (activeConnections) {
        activeConnections.remove(this);
      }
    }

    public HttpURLConnection get() {
      return connection;
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

  /**
   * JSON serialization and deserialization. We register a special adapter for {@link JsonElement}
   * to bring whole chunks of JSON we don't bother to define a schema.
   */
  private static final Gson gson =
      new GsonBuilder()
          .registerTypeAdapter(
              JsonElement.class,
              new JsonDeserializer<JsonElement>() {
                @Override
                public JsonElement deserialize(
                    JsonElement json, Type t, JsonDeserializationContext c) {
                  return json;
                }
              })
          .registerTypeAdapter(
              JsonElement.class,
              new JsonSerializer<JsonElement>() {
                @Override
                public JsonElement serialize(JsonElement json, Type t, JsonSerializationContext c) {
                  return json;
                }
              })
          .create();

  /** Base URL of Debuglet Controller service. */
  private URL controllerBaseUrl = null;

  /** GCP project information and access token. */
  private final MetadataQuery metadata;

  /** Read and explore application resources. */
  private final ClassPathLookup classPathLookup;

  /** Debuggee labels (such as version and module) */
  private final Map<String, String> labels;

  /** Shutdown flag blocking all outgoing HTTP calls to metadata service. */
  private boolean isShutdown = false;

  /** List of active HTTP connections to be closed during shutdown. */
  private final List<ActiveConnection> activeConnections = new ArrayList<>();

  /**
   * Cache of unique identifier of the application resources. Computing uniquifier is an expensive
   * operation and we don't want to repeat it on every {@link GcpHubClient#registerDebuggee()} call.
   */
  private String uniquifier = null;

  /**
   * Cache of source context files embedded into the application. We read these files only once and
   * reuse it in every {@link GcpHubClient#registerDebuggee()} call.
   */
  private String[] sourceContextFiles = null;

  /** Registered debuggee ID. Use getter/setter to access. */
  private String debuggeeId = "not-registered-yet";

  /** Registered agent's ID. Use getter/setter to access. */
  private String agentId = "";

  /** The last wait_token returned in the ListActiveBreakpoints response. */
  private String lastWaitToken = "init";

  /** Default constructor using environment provided by {@link GcpEnvironment}. */
  public GcpHubClient() {
    this.metadata = GcpEnvironment.getMetadataQuery();
    this.classPathLookup = ClassPathLookup.defaultInstance;
    this.labels = GcpEnvironment.getDebuggeeLabels();
  }

  /**
   * Class constructor
   * Visible for testing
   * @param controllerBaseUrl base URL of Debuglet Controller service
   * @param metadata GCP project information and access token
   * @param classPathLookup read and explore application resources
   * @param labels debuggee labels (such as version and module)
   */
  public GcpHubClient(
      URL controllerBaseUrl,
      MetadataQuery metadata,
      ClassPathLookup classPathLookup,
      Map<String, String> labels) {
    this.controllerBaseUrl = controllerBaseUrl;
    this.metadata = metadata;
    this.classPathLookup = classPathLookup;
    this.labels = labels;
  }

  @Override
  public boolean registerDebuggee(Map<String, String> extraDebuggeeLabels) throws Exception {
    // No point going on if we don't have environment information.
    if (metadata.getProjectId().isEmpty() || metadata.getProjectNumber().isEmpty()) {
      throw new Exception("Environment not available");
    }

    // Send the request.
    RegisterDebuggeeMessage request = getDebuggeeInfo(extraDebuggeeLabels);
    JsonElement responseJson;
    String errorResponse = "";
    try (ActiveConnection connection = openConnection("debuggees/register")) {
      connection.get().setDoOutput(true);
      try (Writer writer = new OutputStreamWriter(connection.get().getOutputStream(), UTF_8)) {
        gson.toJson(request, writer);
      }

      try (Reader reader = new InputStreamReader(connection.get().getInputStream(), UTF_8)) {
        JsonParser parser = new JsonParser();
        responseJson = parser.parse(reader);
      } catch (IOException e) {
        errorResponse = readErrorStream(connection.get());
        throw e;
      }
    } catch (IOException e) {
      warnfmt(e, "Failed to register debuggee %s: %s", gson.toJson(request), errorResponse);
      throw e;
    }

    RegisterDebuggeeMessage response = gson.fromJson(responseJson, RegisterDebuggeeMessage.class);
    if ((response == null)
        || (response.getDebuggeeId() == null)
        || response.getDebuggeeId().isEmpty()) {
      throw new Exception("Bad debuggee");
    }

    if (response.getIsDisabled()) {
      info("Debuggee is marked as disabled");
      return false;
    }

    setDebuggeeId(response.getDebuggeeId());
    setAgentId(response.getAgentId());
    infofmt(
        "Debuggee %s, agentId %s, registered: %s, agent version: %s",
        getDebuggeeId(), getAgentId(), responseJson, GcpDebugletVersion.VERSION);

    return true;
  }

  @Override
  public ListActiveBreakpointsResult listActiveBreakpoints() throws Exception {
    StringBuilder path = new StringBuilder();
    path.append("debuggees/");
    path.append(URLEncoder.encode(getDebuggeeId(), UTF_8.name()));
    path.append("/breakpoints");
    path.append("?successOnTimeout=true");
    if (lastWaitToken != null) {
      path.append("&waitToken=");
      path.append(URLEncoder.encode(lastWaitToken, UTF_8.name()));
    }
    if (getAgentId() != null && !getAgentId().isEmpty()) {
      path.append("&agentId=");
      path.append(URLEncoder.encode(getAgentId(), UTF_8.name()));
    }

    ListActiveBreakpointsResponse response;
    String errorResponse = "";
    try (ActiveConnection connection = openConnection(path.toString())) {
      try (Reader reader = new InputStreamReader(connection.get().getInputStream(), UTF_8)) {
        response = gson.fromJson(reader, ListActiveBreakpointsResponse.class);
      } catch (IOException e) {
        errorResponse = readErrorStream(connection.get());
        throw e;
      }
    } catch (IOException e) {
      warnfmt(e, "Failed to query the list of active breakpoints: %s", errorResponse);
      throw e;
    }

    if (response.getWaitExpired()) {
      return LIST_ACTIVE_BREAKPOINTS_TIMEOUT;
    }

    lastWaitToken = response.getNextWaitToken();
    final byte[][] breakpoints = response.serializeBreakpoints();

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
      throw new UnsupportedOperationException("GcpHubClient only supports protobuf format");
    }

    String theDebuggeeId = getDebuggeeId();
    String path =
        String.format(
            "debuggees/%s/breakpoints/%s",
            URLEncoder.encode(theDebuggeeId, UTF_8.name()),
            URLEncoder.encode(breakpointId, UTF_8.name()));
    try (ActiveConnection connection = openConnection(path)) {
      // We could parse breakpoint back to JsonObject, then embed it as in the request element
      // and serialize everything back. This would be very inefficient, which is important here
      // since breakpoint result is fairly large and contains thousands of JSON elements.
      connection.get().setDoOutput(true); // Request with body.
      connection.get().setRequestMethod("PUT");
      try (OutputStream outputStream = connection.get().getOutputStream()) {
        outputStream.write("{\"breakpoint\":".getBytes(UTF_8));
        outputStream.write(breakpoint);
        outputStream.write("}".getBytes(UTF_8));
      }

      // Trigger the request to be sent over. We don't really care to read the response.
      try {
        connection.get().getInputStream().close();
      } catch (IOException e) {
        // We always call readErrorStream to close the error stream to avoid socket leak.
        String errorResponse = readErrorStream(connection.get());
        int responseCode = connection.get().getResponseCode();

        // We consider all application errors (5xx) and timeout (408) to be transient errors
        // that can be retried.
        boolean isTransientError = ((responseCode >= 500) || (responseCode == 408));
        if (!isTransientError) {
          // There is no point in retrying the transmission. It will fail.
          warnfmt(
              e,
              "Failed to transmit breakpoint update, debuggee: %s, breakpoint ID: %s, "
                  + "response: %s\n%s",
              theDebuggeeId,
              breakpointId,
              connection.get().getResponseMessage(),
              errorResponse);
          return;
        }

        throw e;
      }
    }
  }

  @Override
  public void registerBreakpointCanary(String breakpointId) throws Exception {
    throw new UnsupportedOperationException(
        "GcpHubClient#registerBreakpointCanary not implemented");
  }

  @Override
  public void approveBreakpointCanary(String breakpointId) throws Exception {
    throw new UnsupportedOperationException("GcpHubClient#approveBreakpointCanary not implemented");
  }

  /** If there is any reason to not enable the debugger, return false here. */
  @Override
  public boolean isEnabled() {
    return true;
  }

  @Override
  public void shutdown() {
    metadata.shutdown();

    List<ActiveConnection> connections = new ArrayList<>();
    synchronized (activeConnections) {
      isShutdown = true;
      connections.addAll(activeConnections);
    }

    for (ActiveConnection connection : connections) {
      connection.get().disconnect();
    }
  }

  public String getDebuggeeId() {
    return debuggeeId;
  }

  private void setDebuggeeId(String id) {
    debuggeeId = id;
  }

  public String getAgentId() {
    return agentId;
  }

  private void setAgentId(String id) {
    agentId = id;
  }

  /**
   * Lazily initializes the base URL from GcpEnvironment. If the URL string is a place holder string
   * "#", then defer the initialization by throwing an IllegalStateException. Note that the place
   * holder "#" is only for testing purposes.
   *
   * @throws IllegalStateException when Controller Base URL has not initialized
   * @throws MalformedURLException when the base URL string does not meet standard URL format
   */
  private void requireControllerBaseUrl() throws MalformedURLException {
    if (controllerBaseUrl != null) {
      return;
    }

    String baseUrlString = GcpEnvironment.getControllerBaseUrlString();
    if (baseUrlString.equals("#")) {
      throw new IllegalStateException("Controller Base URL not yet set");
    }
    controllerBaseUrl = new URL(baseUrlString);
  }

  /**
   * Fills in the debuggee registration request message.
   *
   * @param extraDebuggeeLabels Extra labels to include in the Debuggee. These are to be in addition
   *     to the labels already collected locally that are to be included.
   *
   * @return debuggee registration request message
   */
  private RegisterDebuggeeMessage getDebuggeeInfo(Map<String, String> extraDebuggeeLabels)
      throws NoSuchAlgorithmException, IOException {
    String property;

    property = System.getProperty("com.google.cdbg.debuggee.file");
    if ((property != null) && !property.isEmpty()) {
      try (InputStream stream = new FileInputStream(property);
          Reader reader = new InputStreamReader(stream, UTF_8)) {
        // If we got bad JSON, GSON will just throw exception that will fail registerDebuggee call.
        return gson.fromJson(reader, RegisterDebuggeeMessage.class);
      }
    }

    property = System.getProperty("com.google.cdbg.debuggee.json");
    if ((property != null) && !property.isEmpty()) {
      // If we got bad JSON, GSON will just throw exception that will fail registerDebuggee call.
      return gson.fromJson(property, RegisterDebuggeeMessage.class);
    }

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

    if (uniquifier == null) {
      boolean hasSourceContext = ((sourceContextFiles != null) && (sourceContextFiles.length > 0));

      // Compute uniquifier of debuggee properties. Start with an initial 20B sha-1 hash value.
      uniquifier = "DA39A3EE5E6B4B0D3255BFEF95601890AFD80709";

      if (!allLabels.containsKey(Labels.Debuggee.MINOR_VERSION) && !hasSourceContext) {
        // There is no source context and minor version. It means that different versions of the
        // application may be running with the same debuggee properties. Hash of application
        // binaries to generate different debuggees in this case.
        uniquifier = classPathLookup.computeDebuggeeUniquifier(uniquifier);
      }
    }

    // Format the request body.
    RegisterDebuggeeMessage request = new RegisterDebuggeeMessage();
    request.setProject(metadata.getProjectNumber());
    request.setUniquifier(uniquifier);
    request.setDescription(getDebuggeeDescription());
    request.setLabels(allLabels);
    request.setAgentVersion(
        String.format("google.com/java-gcp/@%d", GcpDebugletVersion.MAJOR_VERSION));
    request.setSourceContexts(sourceContextFiles);
    request.setCanaryMode(GcpEnvironment.getDebuggeeCanaryMode().toString());

    return request;
  }

  /**
   * Formats debuggee description based on GCP project metadata and labels.
   *
   * @return debuggee description
   */
  private String getDebuggeeDescription() {
    StringBuilder description = new StringBuilder();
    description.append(metadata.getProjectId());

    // TODO: remove after we completely switch over to structured labels.
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
   * Creates HTTP request to the Debuglet Controller API.
   *
   * @param path request URI relative to {@link GcpHubClient#controllerBaseUrl}
   * @return new HTTP connection
   * @throws IOException when URL construction or connection fails
   * @throws IllegalStateException when Controller Service Base URL has not initialized
   */
  private ActiveConnection openConnection(String path) throws IOException {
    requireControllerBaseUrl();
    URL url = new URL(controllerBaseUrl, path);

    ActiveConnection connection = new ActiveConnection((HttpURLConnection) url.openConnection());
    try {
      connection.get().setUseCaches(false);

      connection.get().setRequestProperty("Authorization", "Bearer " + metadata.getAccessToken());
      connection.get().setRequestProperty("Content-Type", "application/json");

      return connection;
    } catch (Throwable t) {
      connection.close();
      throw t;
    }
  }

  /**
   * Reads error response stream from an HTTP connection.
   *
   * @param connection failed HTTP connection
   * @return error response or empty string if one could not be read
   */
  private String readErrorStream(HttpURLConnection connection) {
    try (InputStream inputStream = connection.getErrorStream();
        InputStreamReader inputStreamReader = new InputStreamReader(inputStream, UTF_8);
        BufferedReader bufferedReader = new BufferedReader(inputStreamReader)) {
      StringBuilder errorResponse = new StringBuilder();
      String line;
      while ((line = bufferedReader.readLine()) != null) {
        errorResponse.append(line);
        errorResponse.append('\n');
      }

      return errorResponse.toString();
    } catch (IOException e) {
      return "";
    }
  }
}
