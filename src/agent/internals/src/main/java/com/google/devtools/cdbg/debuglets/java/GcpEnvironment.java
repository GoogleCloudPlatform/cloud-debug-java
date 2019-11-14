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

import java.lang.reflect.Constructor;
import java.util.HashMap;
import java.util.Map;

final class GcpEnvironment {
  /**
   * String to concatenate to project ID to form debuggee description. Provided for backward
   * compatibility only.
   */
  public static final String DEBUGGEE_DESCRIPTION_SUFFIX_LABEL = "description";

  /** Cached instance of authentication class. Visible for testing. */
  static MetadataQuery metadataQuery = null;

  /**
   * Gets the value of a flag defined in the C++ portion of the debuglet.
   *
   * <p>This function logs a warning if flag is not found.
   *
   * @param name flag name
   * @return flag value or null if flag not found by name
   */
  private static native String getAgentFlag(String name);

  /** Wrapper interface for getEnv to facilitate testing. */
  public static interface EnvironmentStore {
    public String get(String name);
  }

  /** Default system environmentStore implementation */
  private static class SystemEnvironmentStore implements EnvironmentStore {
    @Override
    public String get(String name) {
      return System.getenv(name);
    }
  }

  /** The environment variable store. Public for testing. */
  public static EnvironmentStore environmentStore = new SystemEnvironmentStore();

  /** Gets the URL String of the Debuglet Controller API. */
  static String getControllerBaseUrlString() {
    return System.getProperty(
        "com.google.cdbg.controller", "https://clouddebugger.googleapis.com/v2/controller/");
  }

  /** Lazily creates and returns the class to get access token and project information. */
  static synchronized MetadataQuery getMetadataQuery() {
    // Lazy initialization.
    if (metadataQuery == null) {
      String jsonFile = environmentStore.get("GOOGLE_APPLICATION_CREDENTIALS");
      if (jsonFile == null) {
        jsonFile = getFlag("auth.serviceaccount.jsonfile", "service_account_json_file");
      }
      if (jsonFile != null && !jsonFile.isEmpty()) {
        try {
          // Use reflection to create a new instance of "ServiceAccountAuth" class. This way this
          // class has no explicit dependency on "com.google.api.client" package that can be
          // removed if we don't need it.
          Class<? extends MetadataQuery> serviceAccountAuthClass =
              Class.forName(
                      GcpEnvironment.class.getPackage().getName() + ".ServiceAccountAuth",
                      true,
                      GcpEnvironment.class.getClassLoader())
                  .asSubclass(MetadataQuery.class);
          Constructor<? extends MetadataQuery> constructor =
              serviceAccountAuthClass.getConstructor(String.class);
          // Note that we are passing projectId instead of projectNumber here, as the Cloud Debugger
          // service is able to translate projectId into projectNumber.
          metadataQuery = constructor.newInstance(new Object[] {jsonFile});
        } catch (ReflectiveOperationException e) {
          // The constructor of ServiceAccountAuth doesn't do any IO. It can fail if something
          // in the environment is broken (for example no trusted root certificates). If such
          // a problem happens, there is no point retrying.
          throw new RuntimeException("Failed to initialize service account authentication", e);
        }
      } else {
        metadataQuery = new GceMetadataQuery();
      }
    }

    return metadataQuery;
  }

  /**
   * Gets map of debuggee labels.
   *
   * <p>TODO: add additional fallback to Kubernetes to automatically read things like pod,
   * version and Docker image ID (for minor version).
   */
  static Map<String, String> getDebuggeeLabels() {
    Map<String, String> labels = new HashMap<>();

    String legacySuffix = getFlag(DEBUGGEE_DESCRIPTION_SUFFIX_LABEL, "cdbg_description_suffix");
    if (!legacySuffix.isEmpty()) {
      labels.put(DEBUGGEE_DESCRIPTION_SUFFIX_LABEL, legacySuffix);
    }

    // Read module from environment variable allowing overrides through system property.
    String module = System.getProperty("com.google.cdbg.module");
    if (module == null) {
      module = environmentStore.get("GAE_SERVICE");
      if (module == null || module.isEmpty()) {
        module = environmentStore.get("GAE_MODULE_NAME");
      }
      if (module == null || module.isEmpty()) {
        module = environmentStore.get("K_SERVICE");
      }
      if ((module != null) && module.equals("default")) {
        module = null;
      }
    }

    if ((module != null) && !module.isEmpty()) {
      labels.put(Labels.Debuggee.MODULE, module);
    }

    // Read major version from environment variable allowing overrides through system property.
    String majorVersion = System.getProperty("com.google.cdbg.version");
    if (majorVersion == null) {
      majorVersion = environmentStore.get("GAE_VERSION");
      if (majorVersion == null || majorVersion.isEmpty()) {
        majorVersion = environmentStore.get("GAE_MODULE_VERSION");
      }
      if (majorVersion == null || majorVersion.isEmpty()) {
        majorVersion = environmentStore.get("K_REVISION");
      }
    }

    if ((majorVersion != null) && !majorVersion.isEmpty()) {
      labels.put(Labels.Debuggee.VERSION, majorVersion);
    }

    // Minorversion/deployment ID can not be overridden. It is currently dedicated for appengine
    // versioning only.
    String minorVersion = environmentStore.get("GAE_DEPLOYMENT_ID");
    if (minorVersion == null || minorVersion.isEmpty()) {
      minorVersion = environmentStore.get("GAE_MINOR_VERSION");
    }
    if ((minorVersion != null) && !minorVersion.isEmpty()) {
      labels.put(Labels.Debuggee.MINOR_VERSION, minorVersion);
    }

    return labels;
  }

  /** Gets configuration flag from a system property falling back to agent flags. */
  private static String getFlag(String systemPropertySuffix, String agentFlagName) {
    String value;

    value = System.getProperty("com.google.cdbg." + systemPropertySuffix);
    if (value != null) {
      return value;
    }

    value = getAgentFlag(agentFlagName);
    if (value != null) {
      return value;
    }

    return ""; // Return empty string so that we don't need to check for null references.
  }
}
