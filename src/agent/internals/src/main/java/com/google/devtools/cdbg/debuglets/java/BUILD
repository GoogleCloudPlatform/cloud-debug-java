load("@rules_java//java:defs.bzl", "java_library")

package(default_visibility = ["//visibility:public"])

java_library(
    name = "breakpoint_labels_provider",
    srcs = ["BreakpointLabelsProvider.java"],
)

java_library(
    name = "datatype_converter",
    srcs = ["DataTypeConverter.java"],
)

java_library(
    name = "dynamic_log_helper",
    srcs = ["DynamicLogHelper.java"],
)

java_library(
    name = "embed_class_loader",
    srcs = ["EmbedClassLoader.java"],
)

java_library(
    name = "firebase_client",
    srcs = ["FirebaseClient.java"],
    deps = [
        ":datatype_converter",
        ":gcp_debuglet_version",
        ":gcp_environment",
        ":gcp_metadata",
        ":hub_client",
        ":labels",
        ":logger",
        ":mapper",
        "@maven//:com_google_code_gson_gson",
        "@maven//:com_google_firebase_firebase_admin",
    ],
)

java_library(
    name = "gcp_breakpoint_labels_provider",
    srcs = ["GcpBreakpointLabelsProvider.java"],
    deps = [
        ":breakpoint_labels_provider",
        ":gcp_debuglet_version",
    ],
)

java_library(
    name = "gcp_debuglet_version",
    srcs = ["GcpDebugletVersion.java"],
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
    name = "hub_client",
    srcs = ["HubClient.java"],
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
        ":datatype_converter",
        ":gcp_environment",
        ":logger",
        ":safe_caller",
        ":yaml_config_parser",
        "@maven//:org_ow2_asm_asm",
    ],
)

java_library(
    name = "safe_caller",
    srcs = [
        "ProxyObjectBase.java",
        "SafeCallerException.java",
    ],
)

java_library(
    name = "statistician",
    srcs = ["Statistician.java"],
)

java_library(
    name = "user_id_provider",
    srcs = ["UserIdProvider.java"],
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
    name = "agent",
    runtime_deps = [
        ":breakpoint_labels_provider",
        ":datatype_converter",
        ":dynamic_log_helper",
        ":embed_class_loader",
        ":firebase_client",
        ":gcp_breakpoint_labels_provider",
        ":gcp_debuglet_version",
        ":gcp_environment",
        ":gcp_hub_client",
        ":gcp_metadata",
        ":hub_client",
        ":labels",
        ":logger",
        ":mapper",
        ":statistician",
        ":user_id_provider",
        ":yaml_config_parser",
    ],
)

java_binary(
    name = "cdbg_java_agent_internals",
    create_executable = 0,
    use_launcher = False,
    runtime_deps = [
        ":agent"
    ],
)
