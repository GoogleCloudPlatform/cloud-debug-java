package(default_visibility = ["//visibility:public"])

cc_library(
    name = "common",
    textual_hdrs = [
        "common.h",
        "jvm_env.h",
    ],
    deps = [
        "//:jdk_headers",
        "@com_github_google_glog//:glog",
    ],
)

cc_library(
    name = "nullable",
    hdrs = ["nullable.h"],
    deps = [
        ":common",
    ],
)

cc_library(
    name = "messages",
    hdrs = ["messages.h"],
    deps = [
    ],
)

cc_library(
    name = "jvmti_buffer",
    hdrs = ["jvmti_buffer.h"],
    deps = [
        ":common",
    ],
)

cc_library(
    name = "jni_utils_h",
    hdrs = ["jni_utils.h"],
    deps = [
        ":common",
        ":nullable",
    ],
)

cc_library(
    name = "jni_utils",
    srcs = ["jni_utils.cc"],
    copts = ["-Isrc/agent", "-I$(BINDIR)/src/codegen/generated"],
    deps = [
        ":common",
        ":jni_utils_h",
        ":jvmti_buffer",
        ":nullable",
        "//src/codegen:jni_proxies",
    ],
    alwayslink = 1,
)

cc_library(
    name = "type_util",
    srcs = ["type_util.cc"],
    hdrs = ["type_util.h"],
    deps = [
        ":common",
        ":jni_utils",
        ":jni_utils_h",
        ":jvariant",
        ":model",
    ],
)

cc_library(
    name = "model",
    hdrs = ["model.h"],
    deps = [
        ":common",
        ":nullable",
    ],
)

cc_library(
    name = "model_util",
    hdrs = ["model_util.h"],
    deps = [
        ":jni_utils",
        ":jni_utils_h",
        ":messages",
        ":model",
        ":type_util",
    ],
)

cc_library(
    name = "class_metadata_reader",
    hdrs = ["class_metadata_reader.h"],
    deps = [
        ":common",
        ":type_util",
    ],
)

cc_library(
    name = "method_caller",
    hdrs = ["method_caller.h"],
    deps = [
        ":class_metadata_reader",
        ":common",
        ":model_util",
    ],
)

cc_library(
    name = "type_evaluator",
    hdrs = ["type_evaluator.h"],
    deps = [
        ":class_metadata_reader",
        ":common",
        ":method_caller",
    ],
)

cc_library(
    name = "callbacks_monitor",
    srcs = ["callbacks_monitor.cc"],
    hdrs = ["callbacks_monitor.h"],
    deps = [
        ":common",
    ],
)

cc_test(
    name = "callbacks_monitor_test",
    srcs = ["callbacks_monitor_test.cc"],
    deps = [
        ":callbacks_monitor",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "leaky_bucket",
    srcs = ["leaky_bucket.cc"],
    hdrs = ["leaky_bucket.h"],
    deps = [
        ":common"
    ],
)

cc_library(
    name = "mutex",
    hdrs = ["mutex.h"],
)

cc_library(
    name = "mock_jni_env",
    testonly = 1,
    hdrs = ["mock_jni_env.h"],
)

cc_library(
    name = "mock_jvmti_env",
    testonly = 1,
    srcs = ["mock_jvmti_env.cc"],
    hdrs = ["mock_jvmti_env.h"],
    deps = [
        ":common",
        "@com_google_googletest//:gtest_main",
        "//:jdk_headers",
    ],
)

cc_library(
    name = "fake_jni",
    testonly = 1,
    srcs = ["fake_jni.cc"],
    hdrs = ["fake_jni.h"],
    deps = [
        ":class_metadata_reader",
        ":common",
        ":jvariant",
        ":mock_jni_env",
        ":mock_jvmti_env",
    ],
)

cc_library(
    name = "jsoncpp_util",
    srcs = ["jsoncpp_util.cc"],
    hdrs = ["jsoncpp_util.h"],
    deps = [
        ":common",
        "@jsoncpp//:jsoncpp",
    ],
)

cc_test(
    name = "jsoncpp_util_test",
    srcs = ["jsoncpp_util_test.cc"],
    deps = [
        ":jsoncpp_util",
        "@com_google_googletest//:gtest_main",
        "@jsoncpp//:jsoncpp",
    ],
)

cc_library(
    name = "model_json",
    srcs = ["model_json.cc"],
    hdrs = ["model_json.h"],
    copts = ["-Isrc/agent", "-I$(BINDIR)/src/codegen/generated"],
    deps = [
        ":common",
        ":jsoncpp_util",
        ":model",
        ":model_util",
        "//src/codegen:jni_proxies",
        "@jsoncpp//:jsoncpp",
    ],
)

cc_library(
    name = "instance_field_reader",
    hdrs = ["instance_field_reader.h"],
    deps = [
        ":common",
        ":model",
        ":type_util",
    ],
)

cc_library(
    name = "static_field_reader",
    hdrs = ["static_field_reader.h"],
    deps = [
        ":common",
        ":model",
        ":type_util",
    ],
)

cc_library(
    name = "fake_instance_field_reader",
    testonly = 1,
    hdrs = ["fake_instance_field_reader.h"],
    deps = [
        ":common",
        ":instance_field_reader",
        ":jvariant",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "array_type_evaluator",
    hdrs = ["array_type_evaluator.h"],
    deps = [
        ":common",
        ":instance_field_reader",
        ":jni_utils",
        ":jni_utils_h",
        ":jvariant",
        ":type_evaluator",
    ],
)

cc_test(
    name = "array_type_evaluator_test",
    srcs = ["array_type_evaluator_test.cc"],
    deps = [
        ":array_type_evaluator",
        ":fake_instance_field_reader",
        ":mock_jni_env",
        ":mock_jvmti_env",
        ":model",
        ":model_util",
        ":static_field_reader",
        #"//third_party/absl/strings:str_format",
    ],
)

cc_library(
    name = "byte_source",
    hdrs = ["byte_source.h"],
    deps = [
        ":common",
    ],
)

cc_test(
    name = "byte_source_test",
    srcs = ["byte_source_test.cc"],
    deps = [
        ":byte_source",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "observable",
    hdrs = ["observable.h"],
    deps = [
        ":common",
        ":mutex",
    ],
)

cc_test(
    name = "observable_test",
    srcs = ["observable_test.cc"],
    deps = [
        ":observable",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "scheduler",
    srcs = ["scheduler.cc"],
    hdrs = ["scheduler.h"],
    deps = [
        ":common",
        ":mutex",
    ],
)

cc_test(
    name = "scheduler_test",
    srcs = ["scheduler_test.cc"],
    deps = [
        ":scheduler",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "stopwatch",
    hdrs = ["stopwatch.h"],
    deps = [
        ":common",
    ],
)

cc_library(
    name = "jvariant",
    srcs = ["jvariant.cc"],
    hdrs = ["jvariant.h"],
    deps = [
        ":common",
        ":jni_utils",
        ":jni_utils_h",
    ],
)

cc_test(
    name = "jvariant_test",
    srcs = ["jvariant_test.cc"],
    deps = [
        ":jvariant",
        ":mock_jvmti_env",
        ":mock_jni_env",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "data_visibility_policy",
    hdrs = ["data_visibility_policy.h"],
    deps = [
        ":common",
    ],
)

cc_library(
    name = "debuggee_labels",
    srcs = ["debuggee_labels.cc"],
    hdrs = ["debuggee_labels.h"],
    copts = ["-Isrc/agent", "-I$(BINDIR)/src/codegen/generated"],
    deps = [
        ":common",
        "//src/codegen:jni_proxies",
    ],
)

cc_library(
    name = "bridge",
    hdrs = ["bridge.h"],
    deps = [
        ":common",
        ":debuggee_labels",
        ":model",
    ],
)

cc_library(
    name = "mock_bridge",
    testonly = 1,
    hdrs = ["mock_bridge.h"],
    deps = [
        ":bridge",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "breakpoint",
    hdrs = ["breakpoint.h"],
    deps = [
        ":common",
        ":model",
    ],
)

cc_library(
    name = "mock_breakpoint",
    testonly = 1,
    hdrs = ["mock_breakpoint.h"],
    deps = [
        ":breakpoint",
        ":model_util",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "breakpoints_manager",
    hdrs = ["breakpoints_manager.h"],
    deps = [
        ":common",
        ":leaky_bucket",
    ],
)

cc_library(
    name = "mock_breakpoints_manager",
    testonly = 1,
    hdrs = ["mock_breakpoints_manager.h"],
    deps = [
        ":breakpoints_manager",
        ":model_json",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "auto_jvmti_breakpoint",
    srcs = ["auto_jvmti_breakpoint.cc"],
    hdrs = ["auto_jvmti_breakpoint.h"],
    deps = [
        ":breakpoints_manager",
        ":common",
        ":mutex",
    ],
)

cc_test(
    name = "auto_jvmti_breakpoint_test",
    srcs = ["auto_jvmti_breakpoint_test.cc"],
    deps = [
        ":auto_jvmti_breakpoint",
        ":fake_jni",
        ":mock_breakpoint",
        ":mock_breakpoints_manager",
        ":mock_jvmti_env",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "canary_control",
    srcs = ["canary_control.cc"],
    hdrs = ["canary_control.h"],
    copts = ["-Isrc/agent", "-I$(BINDIR)/src/codegen/generated"],
    deps = [
        ":bridge",
        ":callbacks_monitor",
        ":common",
        ":messages",
        ":model",
        ":model_util",
        ":mutex",
        ":stopwatch",
        "@com_github_google_glog//:glog",
    ],
)

cc_test(
    name = "canary_control_test",
    srcs = ["canary_control_test.cc"],
    copts = ["-Isrc/agent", "-I$(BINDIR)/src/codegen/generated"],
    deps = [
        ":callbacks_monitor",
        ":canary_control",
        ":fake_jni",
        ":mock_bridge",
        ":mock_jvmti_env",
        ":model_util",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "mock_data_visibility_policy",
    testonly = 1,
    hdrs = ["mock_data_visibility_policy.h"],
    deps = [
        ":data_visibility_policy",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "multi_data_visibility_policy",
    srcs = ["multi_data_visibility_policy.cc"],
    hdrs = ["multi_data_visibility_policy.h"],
    deps = [
        ":data_visibility_policy",
    ],
)

cc_test(
    name = "multi_data_visibility_policy_test",
    srcs = ["multi_data_visibility_policy_test.cc"],
    deps = [
        ":mock_data_visibility_policy",
        ":multi_data_visibility_policy",
        "@com_google_googletest//:gtest_main",
    ],
)
