package com.google.devtools.cdbg.debuglets.java;

import com.google.apphosting.api.ApiProxy;
import com.google.apphosting.api.ApiProxy.Environment;
import java.util.Map;

/**
 * A utility class to easily retrieve the identifiers used by the other diagnostic tools to collect
 * information about an application.
 *
 * The functionalities provided by this type work only for GAE applications.
 * A JNI proxy is provided for this type to allow invocation from the debugger agent in c++.
 */
class CorrelationIdsProvider {

  /**
   * The identifier of the property used to store the request log id in the environment properties.
   *
   * This identifier is defined in  j/c/g/apphosting/runtime/ApiProxyImpl.java?q=REQUEST_LOG_ID,
   * however that is an internal class on which we don't want to depend here.
   */
  static final String REQUEST_LOG_ID = "com.google.appengine.runtime.request_log_id";

  /**
   * Gets the identifier used by the Cloud Logging backend to save the generated logs.
   *
   * @return The identifier or null if no identifier could be retrieved.
   */
  String getLoggingId() {
    Environment currentEnv = ApiProxy.getCurrentEnvironment();
    if (currentEnv == null) {
      return null;
    }

    // Check if we have the specific attribute
    Map<String, Object> attributes = currentEnv.getAttributes();
    if (attributes == null) {
      return null;
    }

    Object logId = attributes.get(REQUEST_LOG_ID);
    if (logId instanceof String) {
      return (String) logId;
    }

    return null;
  }
}
