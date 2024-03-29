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

/**
 * Obtains GCP project details and auth token for backend calls.
 *
 * <p>This interface is thread safe. All functions return empty strings in case of a failure.
 * Querying metadata service is a local operation that doesn't involve network traffic, so it is not
 * supposed to fail under normal circumstances.
 */
interface MetadataQuery {
  /** Reads unique Google Cloud project ID (e.g. "cdbg-test-hub"). */
  String getProjectId();

  /** Reads unique numeric project ID (e.g. 901565030211). */
  String getProjectNumber();

  /** Retrieves the Google credentials. */
  GoogleCredentials getGoogleCredential();

  /** Retrieves OAuth access token for account authentication. */
  String getAccessToken();

  /** Closes HTTP connections and blocks any further calls. */
  void shutdown();
}
