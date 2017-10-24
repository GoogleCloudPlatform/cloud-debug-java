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

import com.google.api.client.googleapis.auth.oauth2.GoogleCredential;
import com.google.api.client.json.JsonParser;
import com.google.api.client.json.jackson2.JacksonFactory;
import com.google.api.client.util.Key;

import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.security.GeneralSecurityException;
import java.util.Collections;
import java.util.Objects;

/**
 * Exchanges service account private key for OAuth access token.
 */
final class ServiceAccountAuth implements MetadataQuery {

  /** The JSON Service Account file gets parsed into an object of this type. */
  public static class ServiceAccountAuthJsonFile {
    /** The only field we are interested in is project_id, the rest will be ignored. */
    @Key("project_id")
    private String projectId = null;

    String getProjectId() {
      return projectId;
    }
  }

  /**
   * OAuth scope requested.
   */
  private static final String CLOUD_PLATFORM_SCOPE =
      "https://www.googleapis.com/auth/cloud-platform";

  /**
   * Time to refresh the token before it expires.
   */
  private static final int TOKEN_EXPIRATION_BUFFER_SEC = 60;

  /**
   * GCP project ID that created the service account.
   */
  private final String projectId;

  /**
   * GCP project number corresponding to projectId.
   */
  private final String projectNumber;

  /**
   * Runs OAuth flow to obtain the access token.
   */
  private final GoogleCredential credential;

  /**
   * Class constructor
   * @param serviceAccountJsonFile json file with the private key of the service account
   */
  public ServiceAccountAuth(String serviceAccountJsonFile)
      throws GeneralSecurityException, IOException {
    Objects.requireNonNull(serviceAccountJsonFile);

    this.projectId = parseServiceAccountAuthJsonFile(serviceAccountJsonFile).getProjectId();
    this.projectNumber = this.projectId;

    InputStream serviceAccountJsonStream = new FileInputStream(serviceAccountJsonFile);
    this.credential = GoogleCredential.fromStream(serviceAccountJsonStream)
        .createScoped(Collections.singleton(CLOUD_PLATFORM_SCOPE));
  }

  @Override
  public String getProjectId() {
    return projectId;
  }

  @Override
  public String getProjectNumber() {
    return projectNumber;
  }

  /**
   * Gets the cached OAuth access token refreshing it as needed.
   *
   * <p> Access token refresh is done synchronously delaying the call.
   */
  @Override
  public synchronized String getAccessToken() {
    Long expiresIn = credential.getExpiresInSeconds();
    if ((expiresIn == null) || (expiresIn < TOKEN_EXPIRATION_BUFFER_SEC)) {
      try {
        credential.refreshToken();
      } catch (IOException e) {
        // Nothing we can do here.
        // Don't log since logger is not available in standalone service account auth utility.
      }
    }

    String accessToken = credential.getAccessToken();
    return (accessToken == null) ? "" : accessToken;
  }

  @Override
  public void shutdown() {
    // TODO(vlif): implement if not handled properly by JVM shutdown.
  }


  /** Parses the given JSON auth file.
   * <p> TODO(emrekultursay): Remove this method when a new version of the google-api-java-client
   * library is released (newer than v22), and replace it with the new
   * GoogleCredential.getServiceAccountProjectId() method. */
  private static ServiceAccountAuthJsonFile parseServiceAccountAuthJsonFile(
      String serviceAccountJsonFile) throws IOException {
    JsonParser parser = JacksonFactory.getDefaultInstance().createJsonParser(
        new FileInputStream(serviceAccountJsonFile));
    ServiceAccountAuthJsonFile parsedFile = parser.parse(ServiceAccountAuthJsonFile.class);
    if (parsedFile.getProjectId() == null) {
      throw new IllegalArgumentException("Service account JSON file is missing project_id field");
    }

    return parsedFile;
  }
}
