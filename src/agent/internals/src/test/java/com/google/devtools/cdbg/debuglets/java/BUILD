load("@rules_java//java:defs.bzl", "java_test")

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

