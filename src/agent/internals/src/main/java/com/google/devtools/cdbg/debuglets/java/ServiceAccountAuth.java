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

import com.google.auth.oauth2.GoogleCredentials;
import com.google.auth.oauth2.ServiceAccountCredentials;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.time.Duration;
import java.time.Instant;
import java.util.Collections;
import java.util.Objects;

/** Exchanges service account private key for OAuth access token. */
final class ServiceAccountAuth implements MetadataQuery {

  /** OAuth scope requested. */
  private static final String CLOUD_PLATFORM_SCOPE =
      "https://www.googleapis.com/auth/cloud-platform";

  /** Time to refresh the token before it expires. */
  private static final Duration TOKEN_EXPIRATION_BUFFER = Duration.ofSeconds(60);

  /** Runs OAuth flow to obtain the access token. */
  private final ServiceAccountCredentials credential;

  /**
   * Class constructor
   *
   * @param serviceAccountJsonFile json file with the private key of the service account
   * @throws IOException if the file is missing, incomplete, or invalid.
   */
  public ServiceAccountAuth(String serviceAccountJsonFile) throws IOException {
    Objects.requireNonNull(serviceAccountJsonFile);

    InputStream serviceAccountJsonStream = new FileInputStream(serviceAccountJsonFile);
    GoogleCredentials credentials =
        GoogleCredentials.fromStream(serviceAccountJsonStream)
            .createScoped(Collections.singleton(CLOUD_PLATFORM_SCOPE));

    if (!(credentials instanceof ServiceAccountCredentials)) {
      throw new SecurityException("Cannot create service account credentials from file");
    }
    this.credential = (ServiceAccountCredentials) credentials;

    // A refresh is required to populate the access token and expiration fields.
    credential.refreshIfExpired();
  }

  @Override
  public String getProjectId() {
    return credential.getProjectId();
  }

  @Override
  public String getProjectNumber() {
    return credential.getProjectId();
  }

  /**
   * Gets the cached OAuth access token refreshing it as needed.
   *
   * <p>Access token refresh is done synchronously delaying the call.
   */
  @Override
  public synchronized String getAccessToken() {
    // Refresh the token before it expires.
    Instant expirationTime = credential.getAccessToken().getExpirationTime().toInstant();
    if (Instant.now().plus(TOKEN_EXPIRATION_BUFFER).isAfter(expirationTime)) {
      try {
        credential.refresh();
      } catch (IOException e) {
        // Nothing we can do here.
        // Don't log since logger is not available in standalone service account auth utility.
      }
    }

    String accessToken = credential.getAccessToken().getTokenValue();
    return (accessToken == null) ? "" : accessToken;
  }

  @Override
  public void shutdown() {
    // TODO: implement if not handled properly by JVM shutdown.
  }

}
