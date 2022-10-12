package(default_visibility = ["//visibility:public"])

java_library(
    name = "jni_proxy_code_gen_lib",
    srcs = [
        "Config.java",
        "JniProxyCodeGen.java",
    ],
    resources = glob(["*.tpl"]),
    deps = [
        "@maven//:com_google_guava_guava",
        "@maven//:org_ow2_asm_asm",
        "@maven//:org_freemarker_freemarker",
        "@maven//:com_google_code_gson_gson",
    ],
)

java_binary(
    name = "jni_proxy_code_gen_tool",
    main_class = "devtools.cdbg.debuglets.java.codegen.JniProxyCodeGen",
    runtime_deps = [
        ":jni_proxy_code_gen_lib",
        "//src/agent/internals/src/main/java/com/google/devtools/cdbg/debuglets/java:cdbg_java_agent_internals",
    ],
)
