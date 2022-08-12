load("@rules_java//java:defs.bzl", "java_test")

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
    name = "YamlConfigParserTest",
    srcs = ["YamlConfigParserTest.java"],
    deps = [
        "//src/agent/internals/src/main/java/com/google/devtools/cdbg/debuglets/java:yaml_config_parser",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
    ],
)

