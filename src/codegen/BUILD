load("@rules_python//python:defs.bzl", "py_binary")

package(default_visibility = ["//visibility:public"])


ALL_GENERATED_CLASSES = [
    "api_client_datetime",
    "arithmeticexception",
    "bigdecimal",
    "biginteger",
    "breakpointlabelsprovider",
    "classcastexception",
    "class",
    "classloader",
    "classpathlookup",
    "dynamicloghelper",
    "exception",
    "firebaseclient",
    "gcpbreakpointlabelsprovider",
    "gcpdebugletversion",
    "gcphubclient",
    "hubclient",
    "hubclient_listactivebreakpointsresult",
    "iterable",
    "ju_hashmap",
    "jul_logger",
    "ju_map",
    "ju_map_entry",
    "negativearraysizeexception",
    "nullpointerexception",
    "object",
    "printwriter",
    "statistician",
    "string",
    "stringwriter",
    "thread",
    "throwable",
    "useridprovider",
    "yamlconfigparser",
]

TEST_GENERATED_CLASSES = [
    "classfiletextifier",
    "jasmin_main",
    "java_net_url",
    "java_net_urlclassloader",
]

genrule(
    name = "jni_proxy_code_gen_cc",
    srcs = [
        "config.json",
    ],
    outs = ["generated/jni_proxy_%s.cc" % c for c in ALL_GENERATED_CLASSES],
    cmd = "$(location //src/codegen/src/main/java/devtools/cdbg/debuglets/java/codegen:jni_proxy_code_gen_tool) $(location config.json) $(@D)/generated",
    tools = ["//src/codegen/src/main/java/devtools/cdbg/debuglets/java/codegen:jni_proxy_code_gen_tool"],
)

genrule(
    name = "jni_proxy_code_gen_h",
    srcs = [
        "config.json",
    ],
    outs = ["generated/jni_proxy_%s.h" % c for c in ALL_GENERATED_CLASSES],
    cmd = "$(location //src/codegen/src/main/java/devtools/cdbg/debuglets/java/codegen:jni_proxy_code_gen_tool) $(location config.json) $(@D)/generated",
    tools = ["//src/codegen/src/main/java/devtools/cdbg/debuglets/java/codegen:jni_proxy_code_gen_tool"],
)

genrule(
    name = "mock_jni_proxy_code_gen_h",
    srcs = [
        "config.json",
    ],
    outs = ["generated/mock_%s.h" % c for c in ALL_GENERATED_CLASSES],
    cmd = "$(location //src/codegen/src/main/java/devtools/cdbg/debuglets/java/codegen:jni_proxy_code_gen_tool) $(location config.json) $(@D)/generated",
    tools = ["//src/codegen/src/main/java/devtools/cdbg/debuglets/java/codegen:jni_proxy_code_gen_tool"],
)

cc_library(
    name = "mock_jni_proxies",
    testonly = 1,
    hdrs = [":mock_jni_proxy_code_gen_h"],
    deps = [
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "jni_proxies",
    srcs = [":jni_proxy_code_gen_cc"],
    hdrs = [":jni_proxy_code_gen_h"],
    copts = ["-Isrc/agent"],
    deps = [
        "//src/agent:common",
        "//src/agent:jni_utils_h",
    ],
)

genrule(
    name = "jni_test_proxy_code_gen_cc",
    testonly = 1,
    srcs = [
        "config-test.json",
    ],
    outs = ["generated/jni_proxy_%s.cc" % c for c in TEST_GENERATED_CLASSES],
    cmd = "$(location //src/codegen/src/main/java/devtools/cdbg/debuglets/java/codegen:jni_test_proxy_code_gen_tool) $(location config-test.json) $(@D)/generated",
    tools = ["//src/codegen/src/main/java/devtools/cdbg/debuglets/java/codegen:jni_test_proxy_code_gen_tool"],
)

genrule(
    name = "jni_test_proxy_code_gen_h",
    testonly = 1,
    srcs = [
        "config-test.json",
    ],
    outs = ["generated/jni_proxy_%s.h" % c for c in TEST_GENERATED_CLASSES],
    cmd = "$(location //src/codegen/src/main/java/devtools/cdbg/debuglets/java/codegen:jni_test_proxy_code_gen_tool) $(location config-test.json) $(@D)/generated",
    tools = ["//src/codegen/src/main/java/devtools/cdbg/debuglets/java/codegen:jni_test_proxy_code_gen_tool"],
)

cc_library(
    name = "jni_test_proxies",
    testonly = 1,
    srcs = [":jni_test_proxy_code_gen_cc"],
    hdrs = [":jni_test_proxy_code_gen_h"],
    copts = ["-Isrc/agent"],
    deps = [
        "//src/agent:common",
        "//src/agent:jni_utils",
        "//src/agent:jni_utils_h",
    ],
)

# This tool Generates C++ static definition with the content of binary files.
#
# This script is used to embed content of binary files (such as Java .class
# files) inside the C++ binary. It generates C++ code that statically
# initializes byte array with the content of the specified binary files.
# This tool is used below for the cdbg_java_agent_internals_loader.class
py_binary(
  name = "static_def_gen",
  srcs = ["static_def_gen.py"],
)

# Uses static_def_gen.py tool to convert the InternalsClassLoader.class file
# into a C++ file that can be included by jvm_internals.cc. jvm_internals.cc loads this class file
# directly into the JVM and then uses it to load the cdbg_java_agent_internals.jar file.
#
# It is done this way as the internal functionality has to be completely
# segregated from the application: the application must not be able to
# reference debuglet functionality by mistake and vice versa. Furthermore since
# the debuglet internals uses some open source libraries, it is possible that
# the application is using different versions of the same libraries.
genrule(
    name = "internals_class_loader_static_defs",
    srcs = [
        "//src/agent/internals-class-loader/src/main/java/com/google/devtools/cdbg/debuglets/java:cdbg_java_agent_internals_loader.class",
    ],
    outs = [
        "internals_class_loader_static_defs.inl",
    ],
    cmd = "$(location :static_def_gen) $@ " +
          "$$(dirname $(location //src/agent/internals-class-loader/src/main/java/com/google/devtools/cdbg/debuglets/java:cdbg_java_agent_internals_loader.class)):^cdbg_java_agent_internals_loader\\.class$$",
    tools = [
        ":static_def_gen",
    ],
)
