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
