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

import java.lang.reflect.Constructor;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.HashMap;
import java.util.Map;

final class GcpEnvironment {
  /**
   * String to concatenate to project ID to form debuggee description. Provided for backward
   * compatibility only.
   */
  public static final String DEBUGGEE_DESCRIPTION_SUFFIX_LABEL = "description";
  
  /**
   * This application module. This is a native concept in AppEngine and defined artificially
   * on GCE.
   */
  public static final String DEBUGGEE_MODULE_LABEL = "module";

  /**
   * Major application version. This is a native concept in AppEngine and defined artificially
   * on GCE.
   */
  public static final String DEBUGGEE_MAJOR_VERSION_LABEL = "version";

  /**
   * Minor application version. This version code is unique to deployment. This is a native
   * concept in AppEngine and defined artificially on GCE.
   */
  public static final String DEBUGGEE_MINOR_VERSION_LABEL = "minorversion";
  
  /**
   * Cached instance of authentication class.
   */
  private static MetadataQuery metadataQuery = null;
  
  /**
   * Gets the value of a flag defined in the C++ portion of the debuglet.
   * 
   * <p>This function logs a warning if flag is not found.
   * 
   * @param name flag name
   * @return flag value or null if flag not found by name
   */
  private static native String getAgentFlag(String name);
  
  /**
   * Gets the URL of the Debuglet Controller API.
   */
  static URL getControllerBaseUrl() {
    String url = System.getProperty(
        "com.google.cdbg.controller",
        "https://clouddebugger.googleapis.com/v2/controller/");  // Default.
    try {
      return new URL(url);
    } catch (MalformedURLException e) {
      throw new RuntimeException(e);
    }
  }
  
  /**
   * Returns true if the debuglet is configured to authenticate to the backend with service account.
   */
  static boolean getIsServiceAccountEnabled() {
    String value = getFlag("auth.serviceaccount.enable", "enable_service_account_auth");
    return value.equals("1") || value.equalsIgnoreCase("true"); 
  }
  
  /**
   * Lazily creates and returns the class to get access token and project information.
   */
  static synchronized MetadataQuery getMetadataQuery() {
    // Lazy initialization.
    if (metadataQuery == null) {
      if (getIsServiceAccountEnabled()) {
        String projectId = getFlag("auth.serviceaccount.projectid", "project_id");
        if (projectId.isEmpty()) {
          throw new RuntimeException(
              "Project ID not specified for service account authentication. Please set "
              + "either auth.serviceaccount.projectid system property or project_id agent option");
        }
        
        String projectNumber = getFlag("auth.serviceaccount.projectnumber", "project_number");
        if (projectNumber.isEmpty()) {
          throw new RuntimeException(
              "Project number not specified for service account authentication. Please set "
              + "either auth.serviceaccount.projectnumber system property or "
              + "project_number agent option");
        }
        
        String email = getFlag("auth.serviceaccount.email", "service_account_email");
        if (email.isEmpty()) {
          throw new RuntimeException(
              "Service account e-mail not specified for service account authentication. Please set "
              + "either auth.serviceaccount.email system property or "
              + "service_account_email agent option");
        }
        
        String p12File = getFlag("auth.serviceaccount.p12file", "service_account_p12_file");
        if (p12File.isEmpty()) {
          throw new RuntimeException(
              "Private key file not specified for service account authentication. Please set "
              + "either auth.serviceaccount.p12file system property or "
              + "service_account_p12_file agent option");
        }
        
        try {
          // Use reflection to create a new instance of "ServiceAccountAuth" class. This way this
          // class has no explicit dependency on "com.google.api.client" package that can be
          // removed if we don't need it.
          Class<? extends MetadataQuery> serviceAccountAuthClass = Class.forName(
              GcpEnvironment.class.getPackage().getName() + ".ServiceAccountAuth",
              true,
              GcpEnvironment.class.getClassLoader()).asSubclass(MetadataQuery.class);
          Constructor<? extends MetadataQuery> constructor = serviceAccountAuthClass.getConstructor(
              String.class, String.class, String.class, String.class);
          metadataQuery = constructor.newInstance(
              new Object[] { projectId, projectNumber, email, p12File });
        } catch (ReflectiveOperationException e) {
          // The constructor of ServiceAccountAuth doesn't do any IO. It can fail if something
          // in the environment is broken (for example no trusted root certificates). If such
          // a problem happens, there is no point retrying.
          throw new RuntimeException("Failed to initialize service account authentication", e); 
        }
      } else {
        try {
          metadataQuery = new GceMetadataQuery();
        } catch (MalformedURLException e) {
          throw new RuntimeException(e);
        }
      }
    }
    
    return metadataQuery;
  }
  
  /**
   * Gets map of debuggee labels.
   * 
   * <p>TODO(vlif): add additional fallback to Kubernetes to automatically read things like
   * pod, version and Docker image ID (for minor version).
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
      module = System.getenv("GAE_MODULE_NAME");
      if ((module != null) && module.equals("default")) {
        module = null;
      }
    }
    
    if ((module != null) && !module.isEmpty()) {
      labels.put(DEBUGGEE_MODULE_LABEL, module);
    }
    
    // Read major version from environment variable allowing overrides through system property.
    String majorVersion = System.getProperty("com.google.cdbg.version");
    if (majorVersion == null) {
      majorVersion = System.getenv("GAE_MODULE_VERSION");
    }
    
    if ((majorVersion != null) && !majorVersion.isEmpty()) {
      labels.put(DEBUGGEE_MAJOR_VERSION_LABEL, majorVersion);
    }

    // Minorversion can not be override, it is dedicated for appengine versioning only.
    String minorVersion = System.getenv("GAE_MINOR_VERSION");
    if ((minorVersion != null) && !minorVersion.isEmpty()) {
      labels.put(DEBUGGEE_MINOR_VERSION_LABEL, minorVersion);
    }
    
    return labels;
  }
  
  /**
   * Gets configuration flag from a system property falling back to agent flags.
   */
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
    
    return "";  // Return empty string so that we don't need to check for null references. 
  }
}
