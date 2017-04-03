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
 * Command line utility to print OAuth access token exchanged for service account p12 private key.
 *
 * <p>Syntax: ServiceAccountAuthTool <email> <file>
 */
public final class ServiceAccountAuthTool {
  public static void main(String[] args) throws Exception {
    if (args.length != 2) {
      throw new IllegalArgumentException("Require 2 arguments");
    }

    String p12File = "";
    String jsonFile = "";

    if (args[1].endsWith(".p12")) {
      p12File = args[1];
    } else if (args[1].endsWith(".json")) {
      jsonFile = args[1];
    } else {
      throw new IllegalArgumentException("Unsupported file extension: " + args[1]);
    }

    ServiceAccountAuth auth = new ServiceAccountAuth("", "", args[0], p12File, jsonFile);
    System.out.println(auth.getAccessToken());
  }
}
