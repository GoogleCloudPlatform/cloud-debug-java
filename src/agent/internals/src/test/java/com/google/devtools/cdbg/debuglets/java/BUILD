# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_java//java:defs.bzl", "java_test")

package(default_visibility = ["//visibility:public"])

# For tests that need to load the agent. These test will also need the following:
#    data = [
#        "//src/agent:cdbg_java_agent.so",
#        "//src/agent:cdbg_java_agent_internals.jar",
#    ],
CDBG_AGENT_PATH = \
    "-agentpath:$${TEST_SRCDIR}/__main__/src/agent/cdbg_java_agent.so=" + \
    "--log_dir=`if [[ -n \"$${TEST_TMPDIR}\" ]]; then echo \"$${TEST_TMPDIR}\"; else echo \"/tmp\"; fi`"


java_test(
    name = "AgentLoggerTest",
    size = "small",
    srcs = [
        "AgentLoggerTest.java",
    ],
    resources = [
        "//tests/agent:libtest_logger.so",
    ],
    deps = [
        "//src/agent/internals/src/main/java/com/google/devtools/cdbg/debuglets/java:logger",
        "//tests/agent:libtest_logger.so",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
    ],
)

java_test(
    name = "ClassPathLookupTest",
    size = "medium",
    srcs = [
        "ClassPathLookupTest.java",
    ],
    data = [
        ":DynamicJar.jar",
    ],
    resources = [
        "//tests/agent:libtest_logger.so",
    ],
    deps = [
        ":BreakpointTag",
        "//src/agent/internals/src/main/java/com/google/devtools/cdbg/debuglets/java:mapper",
        "//tests/agent:libtest_logger.so",
        "@maven//:com_google_guava_guava",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
        "@maven//:org_ow2_asm_asm",
    ],
)

java_test(
    name = "DataTypeConverterTest",
    size = "small",
    srcs = [
        "DataTypeConverterTest.java",
    ],
    deps = [
        "//src/agent/internals/src/main/java/com/google/devtools/cdbg/debuglets/java:datatype_converter",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
    ],
)

java_binary(
    name = "DynamicJar",
    srcs = [
        "DynamicJarClass.java",
    ],
    deps = [
        "@maven//:junit_junit",
    ],
)

java_test(
    name = "FirebaseClientTest",
    size = "medium",
    srcs = [
        "FirebaseClientTest.java",
    ],
    deps = [
        "//src/agent/internals/src/main/java/com/google/devtools/cdbg/debuglets/java:firebase_client",
        "//src/agent/internals/src/main/java/com/google/devtools/cdbg/debuglets/java:gcp_environment",
        "//src/agent/internals/src/main/java/com/google/devtools/cdbg/debuglets/java:gcp_metadata",
        "//src/agent/internals/src/main/java/com/google/devtools/cdbg/debuglets/java:hub_client",
        "//src/agent/internals/src/main/java/com/google/devtools/cdbg/debuglets/java:mapper",
        "@maven//:com_google_auth_google_auth_library_oauth2_http",
        "@maven//:com_google_code_gson_gson",
        "@maven//:com_google_firebase_firebase_admin",
        "@maven//:com_google_guava_guava",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
        "@maven//:org_mockito_mockito_core",
    ],
)

java_test(
    name = "GcpEnvironmentTest",
    size = "small",
    srcs = [
        "GcpEnvironmentTest.java",
    ],
    jvm_flags = [
        CDBG_AGENT_PATH,
    ],
    data = [
        "//src/agent:cdbg_java_agent.so",
        "//src/agent:cdbg_java_agent_internals.jar",
    ],
    deps = [
        "//src/agent/internals/src/main/java/com/google/devtools/cdbg/debuglets/java:gcp_environment",
        "//src/agent/internals/src/main/java/com/google/devtools/cdbg/debuglets/java:gcp_metadata",
        "//src/agent/internals/src/main/java/com/google/devtools/cdbg/debuglets/java:labels",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
    ],
)

java_test(
    name = "GceMetadataQueryTest",
    size = "small",
    srcs = [
        "GceMetadataQueryTest.java",
    ],
    deps = [
        "//src/agent/internals/src/main/java/com/google/devtools/cdbg/debuglets/java:gcp_metadata",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
        "@maven//:org_mockito_mockito_core",
    ],
)

java_test(
    name = "GcpHubClientTest",
    size = "medium",
    srcs = [
        "GcpHubClientTest.java",
    ],
    deps = [
        "//src/agent/internals/src/main/java/com/google/devtools/cdbg/debuglets/java:gcp_environment",
        "//src/agent/internals/src/main/java/com/google/devtools/cdbg/debuglets/java:gcp_hub_client",
        "//src/agent/internals/src/main/java/com/google/devtools/cdbg/debuglets/java:gcp_metadata",
        "//src/agent/internals/src/main/java/com/google/devtools/cdbg/debuglets/java:hub_client",
        "//src/agent/internals/src/main/java/com/google/devtools/cdbg/debuglets/java:mapper",
        "@maven//:com_google_auth_google_auth_library_oauth2_http",
        "@maven//:com_google_code_gson_gson",
        "@maven//:com_google_guava_guava",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
        "@maven//:org_mockito_mockito_core",
    ],
)

java_test(
    name = "YamlConfigParserTest",
    size = "small",
    srcs = ["YamlConfigParserTest.java"],
    deps = [
        "//src/agent/internals/src/main/java/com/google/devtools/cdbg/debuglets/java:yaml_config_parser",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
    ],
)

java_library(
    name = "class_file_textifier",
    srcs = [
        "ClassFileTextifier.java",
    ],
    deps = [
        "@maven//:com_google_guava_guava",
        "@maven//:org_ow2_asm_asm",
        "@maven//:org_ow2_asm_asm_util",
    ],
)

java_library(
    name = "nano_java_interpreter_test_lib",
    srcs = [
        "NanoJavaInterpreterTestLib.java",
    ],
)

# Provides support for finding a line number in a source file for tests that need this.
# Eg. add either '/* BPTAG: TAG_NAME */' or '// BPTAG: TAG_NAME'  to the end of
# a line in the source file.
# All the .java files are included automatically and available for tests to set TAGs in
java_library(
    name = "BreakpointTag",
    testonly = 1,
    srcs = [
        "BreakpointDefinition.java",
        "BreakpointTagException.java",
        "TestDataPath.java",
    ],
    data = glob(["*.java"]),
    deps = [
        "//schema:data_java_proto",
        "@bazel_tools//tools/java/runfiles:runfiles",
        "@maven//:com_google_guava_guava",
        "@maven//:junit_junit",
    ]
)

java_test(
    name = "BreakpointDefinitionTest",
    size = "small",
    srcs = ["BreakpointDefinitionTest.java"],
    deps = [
        ":BreakpointTag",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
    ],
)
