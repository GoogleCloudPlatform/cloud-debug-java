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

/**
 * Exposes Cloud Debugger agent statistics to Java code.
 */
final class Statistician {
  /**
   * Statistical variables exposed by the agent.
   */
  static final String[] VARIABLES = new String [] {
      "capture_time_micros",
      "dynamic_log_time_micros",
      "condition_evaluation_time_micros",
      "formatting_time_micros",
      "class_prepare_time_micros",
      "breakpoints_update_time_micros"
  };
  
  private final int count;
  private final double min;
  private final double max;
  private final double mean;
  private final double stdev;

  private Statistician(int count, double min, double max, double mean, double stdev) {
    this.count = count;
    this.min = min;
    this.max = max;
    this.mean = mean;
    this.stdev = stdev;
  }
 
  /**
   * Gets statistics of the specified variable from the agent native code
   * 
   * @param name variable name
   * @return statistics of the variable or null if variable with the specified name not defined
   */
  static native Statistician getStatistics(String name);
  
  /**
   * Gets the number of samples added.
   */
  int getCount() {
    return count;
  }

  /**
   * Gets the minimal sample value encountered.
   */
  double getMin() {
    return min;
  }

  /**
   * Gets the maximal sample value encountered.
   */
  double getMax() {
    return max;
  }

  /**
   * Gets the mean value of all the samples encountered.
   */
  double getMean() {
    return mean;
  }

  /**
   * Gets the standard deviation of the samples.
   */
  double getStdev() {
    return stdev;
  }
}
