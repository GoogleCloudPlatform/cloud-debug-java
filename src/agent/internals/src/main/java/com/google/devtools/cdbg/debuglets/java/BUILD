load("@rules_java//java:defs.bzl", "java_library")

package(default_visibility = ["//visibility:public"])

java_library(
    name = "gcp_debuglet_version",
    srcs = ["GcpDebugletVersion.java"],
)

java_library(
    name = "hub_client",
    srcs = ["HubClient.java"],
)

java_library(
    name = "gcp_hub_client",
    srcs = ["GcpHubClient.java"],
    deps = [
        ":gcp_debuglet_version",
        ":gcp_environment",
        ":gcp_metadata",
        ":hub_client",
        ":labels",
        ":logger",
        ":mapper",
        "@maven//:com_google_code_gson_gson"
    ],
)

java_library(
    name = "gcp_environment",
    srcs = [
        "GcpEnvironment.java",
    ],
    deps = [
        ":gcp_metadata",
        ":labels",
        ":logger",
    ],
)

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
    name = "labels",
    srcs = ["Labels.java"],
)

java_library(
    name = "logger",
    srcs = ["AgentLogger.java"],
)

java_library(
    name = "mapper",
    srcs = [
        "AppPathLookup.java",
        "ClassPathLookup.java",
        "ClassResourcesIndexer.java",
        "FormatMessage.java",
        "Messages.java",
        "ResolvedSourceLocation.java",
        "ResourceIndexer.java",
        "ResourcesDatabase.java",
        "SourceFileMapper.java",
        "UniquifierComputer.java",
    ],
    deps = [
        ":logger",
        ":yaml_config_parser",
        "@maven//:org_ow2_asm_asm",
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

