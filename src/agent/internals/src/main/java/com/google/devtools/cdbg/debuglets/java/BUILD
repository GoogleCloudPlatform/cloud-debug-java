load("@rules_java//java:defs.bzl", "java_library")

package(default_visibility = ["//visibility:public"])

java_library(
    name = "gcp_metadata",
    srcs = [
        "GceMetadataQuery.java",
        "MetadataQuery.java",
        "ServiceAccountAuth.java",
    ],
    deps = [
        ":logger",
        "@maven//:com_google_auth_google_auth_library_oauth2_http",
        "@maven//:com_google_code_gson_gson"
    ],
)

java_library(
    name = "yaml_config_parser",
    srcs = [
        "YamlConfigParser.java",
        "YamlConfigParserException.java",
    ],
    deps = [
        ":logger",
        "@maven//:org_yaml_snakeyaml"
    ],
)

java_library(
    name = "logger",
    srcs = ["AgentLogger.java"],
)
