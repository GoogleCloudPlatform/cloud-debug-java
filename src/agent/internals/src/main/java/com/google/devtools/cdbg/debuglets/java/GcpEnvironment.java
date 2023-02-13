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

import java.io.File;

// Needed to wrap the calls to the native logging methods which may fail for unit tests of classes
// that use the AgentLogger, since there won't be a .so loaded that provides the native methods.
import java.lang.UnsatisfiedLinkError;

import java.util.HashMap;
import java.util.Map;

final class GcpEnvironment {

  /** Private constructor; class should not be instantiated. */
  private GcpEnvironment() {}

  /**
   * The enum contains one-to-one mapping to the values of Debuggee.canaryMode in Cloud
   * Debugger's V2 API.
   */
  public enum DebuggeeCanaryMode {
    CANARY_MODE_UNSPECIFIED,
    CANARY_MODE_DEFAULT_ENABLED,
    CANARY_MODE_ALWAYS_ENABLED,
    CANARY_MODE_DEFAULT_DISABLED,
    CANARY_MODE_ALWAYS_DISABLED
  }

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

  /** Gets the URL String of the Firebase RTDB instance. */
  static String getFirebaseDatabaseUrlString() {
    return getFlag("firebase_db_url", "firebase_db_url");
  }

  /** Lazily creates and returns the class to get access token and project information. */
  static synchronized MetadataQuery getMetadataQuery() {
    // Lazy initialization.
    if (metadataQuery == null) {
      try {
        String jsonFileGAC = environmentStore.get("GOOGLE_APPLICATION_CREDENTIALS");
        String jsonFileFlag = getFlag("auth.serviceaccount.jsonfile", "service_account_json_file");
        if (jsonFileGAC != null && !jsonFileGAC.isEmpty()) {
          info("Using credentials from GOOGLE_APPLICATION_CREDENTIALS");
          metadataQuery = new ServiceAccountAuth(jsonFileGAC);
        } else if (jsonFileFlag != null && !jsonFileFlag.isEmpty()) {
          info("Using credentials from user's json file");
          metadataQuery = new ServiceAccountAuth(jsonFileFlag);
        } else {
          info("Using credentials from GCE metadata server");
          metadataQuery = new GceMetadataQuery();
        }
      } catch (Exception e) {
        throw new SecurityException(
            "Failed to initialize service account authentication", e);
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

  /**
   * Returns the user dir of a Java8 App Engine Standard instance. This is the base directory where
   * the user's files are deployed. If no Java 8 App Engine Standard user dir could be determined
   * null is returned.  It is safe to call this regardless of environment, as long as the return
   * value is checked.
   *
   * <p> While there is a system property defined, "user.dir", in Java8 App Engine environments, it
   * is unfortunately not set at the point in time this agent code runs, so it cannot be relied
   * upon. Here we use the heuristic that the WEB-INF directory is deployed at the root of the user
   * dir, and the deployed agent is deployed somewhere under the user dir. This means we can search
   * through the agent directory's ancestors looking for the WEB-INFO directory, and once found
   * we've found the user dir.
   *
   * @return the Java 8 App Engine Standard user directory if it was able to be determined, null
   * otherwise.
   */
  public static String getAppEngineJava8UserDir(String agentDir) {
    File currentDir = new File(agentDir);

    while (currentDir != null) {
      if (new File(currentDir.getAbsolutePath(), "WEB-INF").exists()) {
        break;
      }

      currentDir = currentDir.getParentFile();
    }

    return currentDir == null ? null : currentDir.getAbsolutePath();
  }

  /**
   * Gets the Debuggee.canaryMode from the corresponding System properties.
   */
  static DebuggeeCanaryMode getDebuggeeCanaryMode() {
    String enableCanaryString = System.getProperty("com.google.cdbg.breakpoints.enable_canary");
    String allowCanaryOverrideString =
        System.getProperty("com.google.cdbg.breakpoints.allow_canary_override");
    if (enableCanaryString == null) {
      // Return UNSPECIFIED if enable_canary is not configured.
      return DebuggeeCanaryMode.CANARY_MODE_UNSPECIFIED;
    }

    boolean enableCanary = Boolean.parseBoolean(enableCanaryString);
    boolean allowCanaryOverride = Boolean.parseBoolean(allowCanaryOverrideString);
    if (enableCanary && allowCanaryOverride) {
      return DebuggeeCanaryMode.CANARY_MODE_DEFAULT_ENABLED;
    } else if (enableCanary && !allowCanaryOverride) {
      return DebuggeeCanaryMode.CANARY_MODE_ALWAYS_ENABLED;
    } else if (!enableCanary && allowCanaryOverride) {
      return DebuggeeCanaryMode.CANARY_MODE_DEFAULT_DISABLED;
    } else if (!enableCanary && !allowCanaryOverride) {
      return DebuggeeCanaryMode.CANARY_MODE_ALWAYS_DISABLED;
    }

    // Above if-else statements should include all execution pathes, so reaching here is impossible,
    // unless something went really wrong.
    throw new IllegalStateException("Unexpected code execution path in getDebuggeeCanaryMode()");
  }

  /** Gets configuration flag from a system property falling back to agent flags. */
  private static String getFlag(String systemPropertySuffix, String agentFlagName) {
    String value;

    value = System.getProperty("com.google.cdbg." + systemPropertySuffix);
    if (value != null) {
      return value;
    }

    try {
      value = getAgentFlag(agentFlagName);

      if (value != null) {
        return value;
      }
    }
    catch (UnsatisfiedLinkError e) {
    }

    return ""; // Return empty string so that we don't need to check for null references.
  }
}
