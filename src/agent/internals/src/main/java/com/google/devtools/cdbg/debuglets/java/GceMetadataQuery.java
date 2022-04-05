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
import static com.google.devtools.cdbg.debuglets.java.AgentLogger.severefmt;
import static com.google.devtools.cdbg.debuglets.java.AgentLogger.warnfmt;
import static java.nio.charset.StandardCharsets.UTF_8;

import com.google.gson.Gson;
import com.google.gson.JsonSyntaxException;
import com.google.gson.annotations.SerializedName;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLConnection;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Reads project and auth token from GCE metadata service.
 *
 * <p>The metadata service is explained <a
 * href="https://developers.google.com/compute/docs/metadata">here</a>.
 */
public final class GceMetadataQuery implements MetadataQuery {
  /** Metadata service token. */
  private static final class Token {
    @SerializedName("access_token")
    private String accessToken;

    @SerializedName("expires_in")
    private int expiresIn;

    @SerializedName("token_type")
    private String tokenType;

    public String getAccessToken() {
      return accessToken;
    }

    public int getExpiresIn() {
      return expiresIn;
    }
  }

  /** Minimal remaining expiration time of access token before we query a new one. */
  static final int CACHE_BUFFER_SEC = 30;

  /** Buffer size for reading input streams. */
  static final int BUFFER_SIZE = 1024;

  /** Timeout for reading data from the metadata server. */
  private static final int READ_TIMEOUT_MILLIS =
      (int) TimeUnit.MILLISECONDS.convert(10, TimeUnit.SECONDS);

  /** Base URL for metadata service. Specific attributes are appended to this URL. */
  static final String DEFAULT_LOCAL_METADATA_SERVICE_BASE =
      "http://metadata.google.internal/computeMetadata/v1/";

  /** JSON deserializer to read access token. */
  private static final Gson gson = new Gson();

  /**
   * Base URL for metadata service. Specific attributes are appended to this URL. Unit test may
   * override it to inject a mock HTTP client.
   */
  private URL localMetadataServiceBaseUrl = null;

  /** Shutdown flag blocking all outgoing HTTP calls to metadata service. */
  private boolean isShutdown = false;

  /** List of active HTTP connections to be closed during shutdown. */
  private final List<HttpURLConnection> activeConnections = new ArrayList<>();

  /** Cached value of GCP project ID. */
  private String projectId = null;

  /** Cached value of GCP project number. */
  private String projectNumber = null;

  /** Cached OAuth access token for account authentication. */
  private String accessToken = null;

  /** Absolute expiration time of the "access_token_" in seconds since Unix epoch. */
  private long accessTokenExpirationTime = 0;

  public GceMetadataQuery() {}

  public GceMetadataQuery(String projectId, String projectNumber) {
    this.projectId = projectId;
    this.projectNumber = projectNumber;
  }

  /** Visible for testing */
  public GceMetadataQuery(URL localMetadataServiceBaseUrl) {
    this.localMetadataServiceBaseUrl = localMetadataServiceBaseUrl;
  }

  @Override
  public synchronized String getProjectId() {
    if (projectId == null) {
      try {
        projectId = queryMetadataAttribute("project/project-id");
      } catch (IOException e) {
        warnfmt(e, "Failed to query GCE metadata service");
        return "";
      } catch (IllegalStateException e) {
        warnfmt(e, "GCE metadata attribute is in illegal state");
        return "";
      }
    }

    return projectId;
  }

  @Override
  public synchronized String getProjectNumber() {
    if (projectNumber == null) {
      try {
        projectNumber = queryMetadataAttribute("project/numeric-project-id");
      } catch (IOException e) {
        warnfmt(e, "Failed to query GCE metadata service");
        return "";
      } catch (IllegalStateException e) {
        warnfmt(e, "GCE metadata attribute is in illegal state");
        return "";
      }
    }

    return projectNumber;
  }

  @Override
  public synchronized String getAccessToken() {
    lazyQueryAccessToken();
    return (accessToken == null) ? "" : accessToken;
  }

  @Override
  public void shutdown() {
    List<HttpURLConnection> connections;

    synchronized (activeConnections) {
      isShutdown = true;
      connections = new ArrayList<>(activeConnections);
    }

    for (HttpURLConnection connection : connections) {
      connection.disconnect();
    }
  }

  /** Reads OAuth access token unless the cached value is still valid. */
  void lazyQueryAccessToken() {
    long startTime = System.currentTimeMillis() / 1000; // epoch time

    if ((accessToken != null) && (startTime + CACHE_BUFFER_SEC < accessTokenExpirationTime)) {
      return; // The cache is still valid.
    }

    // If we fail to query the access token, we just leave the last value. It
    // might be still valid. Worst case the HTTP request that will be using the
    // token will fail with 403.
    String tokenJsonString;
    try {
      tokenJsonString = queryMetadataAttribute("instance/service-accounts/default/token");
    } catch (IOException e) {
      warnfmt(e, "Failed to query access token in GCE metadata service");
      return;
    } catch (IllegalStateException e) {
      warnfmt(e, "GCE metadata attribute is in illegal state");
      return;
    }

    Token token;
    try {
      token = gson.fromJson(tokenJsonString, Token.class);
    } catch (JsonSyntaxException e) {
      severefmt(e, "Access token is not a proper JSON string: %s", tokenJsonString);
      return;
    }

    if ((token.getAccessToken() == null) || token.getAccessToken().isEmpty()) {
      severefmt("\"access_token\" attribute not available in JSON BLOB %s", tokenJsonString);
      return;
    }

    accessToken = token.getAccessToken();

    if (token.getExpiresIn() > 0) {
      accessTokenExpirationTime = startTime + token.getExpiresIn();
    } else {
      warnfmt("Access token expiration time not available: %s", tokenJsonString);
      accessTokenExpirationTime = 0;
    }
  }

  /**
   * Lazily initializes the base URL from system property. If the URL string is a place holder
   * string "#", then defer the initialization by throwing an IllegalStateException. Note that the
   * place holder "#" is only for testing purposes.
   *
   * @throws IllegalStateException when Metadata Service Base URL has not initialized
   * @throws MalformedURLException when the base URL string does not meet standard URL format
   */
  private void requireMetadataBaseUrl() throws MalformedURLException {
    if (localMetadataServiceBaseUrl != null) {
      return;
    }

    String baseUrlString =
        System.getProperty(
            "com.google.cdbg.metadataservicebase", DEFAULT_LOCAL_METADATA_SERVICE_BASE);
    if (baseUrlString.equals("#")) {
      throw new IllegalStateException("Metadata Service Base URL not yet set");
    }
    localMetadataServiceBaseUrl = new URL(baseUrlString);
  }

  /**
   * Queries a single attribute of a local metadata service.
   *
   * @param attributePath relative URL to the queried attribute
   * @return value of the metadata attribute
   *
   * @throws IOException when URL construction or connection fails
   * @throws IllegalStateException when Metadata Service Base URL has not initialized
   */
  private String queryMetadataAttribute(String attributePath) throws IOException {
    requireMetadataBaseUrl();
    URL url = new URL(localMetadataServiceBaseUrl, attributePath);

    // Open HTTP connection to the metadata service. We deliberately don't call disconnect
    // to keep the HTTP connection alive between multiple calls.
    HttpURLConnection connection = (HttpURLConnection) url.openConnection();

    synchronized (activeConnections) {
      if (isShutdown) {
        // We don't need to call "disconnect" here. "URL.openConnection" doesn't actually
        // establish the connection, so calling "disconnect" will have no effect.
        throw new RuntimeException("Shutdown in progress");
      }

      activeConnections.add(connection);
    }

    try {
      connection.setUseCaches(false);

      // Google specific HTTP header that must be included in all requests to metadata service.
      connection.setRequestProperty("Metadata-Flavor", "Google");

      return readResponse(connection);
    } finally {
      synchronized (activeConnections) {
        activeConnections.remove(connection);
      }
    }
  }

  /**
   * Reads HTTP response to a string.
   *
   * <p>This function assumes that the response is UTF-8 encoded.
   *
   * @param connection HTTP connection
   * @return HTTP response body decoded as UTF-8 string
   */
  private static String readResponse(URLConnection connection) throws IOException {
    connection.setReadTimeout(READ_TIMEOUT_MILLIS);
    try (InputStream inputStream = connection.getInputStream()) {
      ByteArrayOutputStream outputStream = new ByteArrayOutputStream();

      byte[] buffer = new byte[BUFFER_SIZE];
      int chunkSize;
      while ((chunkSize = inputStream.read(buffer)) != -1) {
        outputStream.write(buffer, 0, chunkSize);
      }

      return new String(outputStream.toByteArray(), UTF_8);
    }
  }
}
