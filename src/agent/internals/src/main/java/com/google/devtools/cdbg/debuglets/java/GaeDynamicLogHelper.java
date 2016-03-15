package com.google.devtools.cdbg.debuglets.java;

import com.google.apphosting.api.ApiProxy;

/**
 * Helper class to send log entries to AppEngine log.
 *
 * <p>AppEngine dynamically rewrites the application code to use ApiProxy.log instead of
 * the default {@link java.util.logging.Logger}. The debugger must follow suit and do the same.
 *
 * <p>The public methods in this class are called from native code. The native code doesn't care
 * about accessors, so we can keep the class as private.
 */
final class GaeDynamicLogHelper {
  /**
   * Writes a single info entry to AppEngine log.
   */
  public static void logInfo(String message) {
    log(ApiProxy.LogRecord.Level.info, message);
  }

  /**
   * Writes a single warning entry to AppEngine log.
   */
  public static void logWarning(String message) {
    log(ApiProxy.LogRecord.Level.warn, message);
  }

  /**
   * Writes a single error entry to AppEngine log.
   */
  public static void logError(String message) {
    log(ApiProxy.LogRecord.Level.error, message);
  }

  /**
   * Writes a single entry to AppEngine log.
   */
  private static void log(ApiProxy.LogRecord.Level level, String message) {
    ApiProxy.log(
        new ApiProxy.LogRecord(level, System.currentTimeMillis() * 1000, message, new Exception()));
  }
}

