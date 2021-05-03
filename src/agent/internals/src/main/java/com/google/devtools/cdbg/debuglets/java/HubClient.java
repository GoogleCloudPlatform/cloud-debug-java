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

import java.util.Map;

/**
 * Communication layer with the Cloud Debugger backend.
 *
 * <p>This interface is implemented separately for each communication protocol and integration test.
 * All methods throw {@link Exception} in case of failure because specific error type depends on the
 * communication protocol library.
 *
 * <p>HubClient interface does not prescribe specific data format to serialize breakpoints. The
 * native code serializes breakpoint into the format it recognizes. It is up to the configuration to
 * make sure that the receiving end recognizes the data format.
 *
 * <p>The HubClient abstraction hides the details about wait_token and debuggee ID.
 *
 * <p>This class should not retry. This will be the responsibility of the caller.
 *
 * <p>This interface must be public. It is being accessed from classes loaded by other class
 * loaders.
 */
public interface HubClient {
  /**
   * Result of {@link HubClient#listActiveBreakpoints()} call.
   *
   * <p>This is either timeout or actual list of breakpoints.
   */
  interface ListActiveBreakpointsResult {
    /** Returns true if no changes to active breakpoints occurred. */
    boolean getIsTimeout();

    /** Gets the serialized format of active breakpoints list. */
    String getFormat();

    /**
     * Gets the returned list of active breakpoints.
     *
     * @return array of serialized breakpoint definitions
     * @throws UnsupportedOperationException if the call resulted in timeout.
     */
    byte[][] getActiveBreakpoints();
  }

  /** Timeout return value for {@link HubClient#listActiveBreakpoints()}. */
  static final ListActiveBreakpointsResult LIST_ACTIVE_BREAKPOINTS_TIMEOUT =
      new ListActiveBreakpointsResult() {
        @Override
        public boolean getIsTimeout() {
          return true;
        }

        @Override
        public String getFormat() {
          throw new UnsupportedOperationException();
        }

        @Override
        public byte[][] getActiveBreakpoints() {
          throw new UnsupportedOperationException();
        }
      };

  /**
   * Registers the debuggee with the controller.
   *
   * @param extraDebuggeeLabels Extra labels to include in the Debuggee in the RegisterDebuggee call
   * to the Hub. These are to be in addition to any labels the Hub client itself may include. NOTE,
   * it's important this list of labels stay the same from call to call, since the labels are used
   * to generate the debuggee ID and it would lead to duplicate IDs for the same agent instance.
   *
   * @return true unless the hub remotely disabled the debuglet
   * @throws Exception if the registration request failed
   */
  boolean registerDebuggee(Map<String, String> extraDebuggeeLabels) throws Exception;

  /**
   * Queries for the list of currently active breakpoints.
   *
   * @return query result
   * @throws Exception if the query fails
   */
  ListActiveBreakpointsResult listActiveBreakpoints() throws Exception;

  /**
   * Sends breakpoint update to the backend.
   *
   * <p>This method will only throw exception if the request fails due to a transient error (such as
   * destination unreachable). If the request fails with permanent error, this function will
   * silently absorb it. Prime example of such a case is updating breakpoint that's already in a
   * final state.
   *
   * <p>The breakpoint ID is duplicated in breakpointId argument and inside the serialized data.
   * This is done for performance reasons, so that the implementation of {@link HubClient} doesn't
   * need to deserialize the breakpoint just to get an ID.
   *
   * @param breakpointId ID of the updated breakpoint
   * @param breakpoint serialized breakpoint result
   * @throws Exception if the request fails due to network failure
   */
  void transmitBreakpointUpdate(String format, String breakpointId, byte[] breakpoint)
      throws Exception;

  /**
   * Notifies that a canary agent enabled the breakpoint.
   *
   * <p>This method must be called exactly once for each canary breakpoint. If this call fails (by
   * throwing an exception), the debuglet must retry before applying the breakpoint.
   *
   * @param breakpointId ID of the canary breakpoint.
   * @throws Exception if the backend could not be notified or on failure on the backend side.
   */
  void registerBreakpointCanary(String breakpointId) throws Exception;

  /**
   * Approves the breakpoint for a global rollout.
   *
   * <p>This method must be called exactly once for each canary breakpoint that was enabled for a
   * considerable period of time. If this call fails (by throwing an exception), the debuglet must
   * retry.
   *
   * @param breakpointId ID of the canary breakpoint.
   * @throws Exception if the backend could not be notified or on failure on the backend side.
   */
  void approveBreakpointCanary(String breakpointId) throws Exception;

  /** Returns false if this agent should not be enabled. */
  boolean isEnabled() throws Exception;

  /** Asynchronously aborts all pending requests and blocks any future network connections. */
  void shutdown();
}
