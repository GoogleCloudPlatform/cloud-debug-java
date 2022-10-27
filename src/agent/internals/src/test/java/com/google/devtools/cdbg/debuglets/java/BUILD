load("@rules_java//java:defs.bzl", "java_test")

package(default_visibility = ["//visibility:public"])

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

