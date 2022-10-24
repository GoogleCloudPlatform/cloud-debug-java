package(default_visibility = ["//visibility:public"])

cc_library(
    name = "mock_array_reader",
    testonly = 1,
    hdrs = ["mock_array_reader.h"],
    deps = [
        "//src/agent:array_reader",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "mock_readers_factory",
    testonly = 1,
    hdrs = ["mock_readers_factory.h"],
    deps = [
        "//src/agent:common",
        "//src/agent:array_reader",
        ":fake_instance_field_reader",
        ":fake_local_variable_reader",
        ":fake_static_field_reader",
        "//src/agent:readers_factory",
        "@com_google_googletest//:gtest_main",
    ],
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
        "//src/agent:common",
        "@com_google_googletest//:gtest_main",
        "//:jdk_headers",
    ],
)

cc_library(
    name = "mock_object_evaluator",
    testonly = 1,
    hdrs = ["mock_object_evaluator.h"],
    deps = [
        "//src/agent:object_evaluator",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "mock_class_indexer",
    testonly = 1,
    hdrs = ["mock_class_indexer.h"],
    deps = [
        "//src/agent:class_indexer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "mock_breakpoint_labels_provider",
    testonly = 1,
    hdrs = ["mock_breakpoint_labels_provider.h"],
    deps = [
        "//src/agent:breakpoint_labels_provider",
        "//src/agent:class_indexer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "mock_user_id_provider",
    testonly = 1,
    hdrs = ["mock_user_id_provider.h"],
    deps = [
        "//src/agent:user_id_provider",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "mock_bridge",
    testonly = 1,
    hdrs = ["mock_bridge.h"],
    deps = [
        "//src/agent:bridge",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "mock_breakpoint",
    testonly = 1,
    hdrs = ["mock_breakpoint.h"],
    deps = [
        "//src/agent:breakpoint",
        "//src/agent:model_util",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "mock_breakpoints_manager",
    testonly = 1,
    hdrs = ["mock_breakpoints_manager.h"],
    deps = [
        "//src/agent:breakpoints_manager",
        "//src/agent:model_json",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "mock_nanojava_internal_error_provider",
    testonly = 1,
    hdrs = ["mock_nanojava_internal_error_provider.h"],
    deps = [
        "//src/agent:nanojava_internal_error_builder",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "mock_data_visibility_policy",
    testonly = 1,
    hdrs = ["mock_data_visibility_policy.h"],
    deps = [
        "//src/agent:data_visibility_policy",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "mock_eval_call_stack",
    testonly = 1,
    hdrs = ["mock_eval_call_stack.h"],
    deps = [
        "//src/agent:eval_call_stack",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "fake_local_variable_reader",
    testonly = 1,
    hdrs = ["fake_local_variable_reader.h"],
    deps = [
        "//src/agent:jvariant",
        "//src/agent:local_variable_reader",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "fake_jni",
    testonly = 1,
    srcs = ["fake_jni.cc"],
    hdrs = ["fake_jni.h"],
    deps = [
        "//src/agent:class_metadata_reader",
        "//src/agent:common",
        "//src/agent:jvariant",
        ":mock_jni_env",
        ":mock_jvmti_env",
    ],
)

cc_library(
    name = "fake_static_field_reader",
    testonly = 1,
    hdrs = ["fake_static_field_reader.h"],
    deps = [
        "//src/agent:common",
        "//src/agent:jvariant",
        "//src/agent:static_field_reader",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "fake_instance_field_reader",
    testonly = 1,
    hdrs = ["fake_instance_field_reader.h"],
    deps = [
        "//src/agent:common",
        "//src/agent:instance_field_reader",
        "//src/agent:jvariant",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "named_jvariant_test_util",
    testonly = 1,
    srcs = ["named_jvariant_test_util.cc"],
    hdrs = ["named_jvariant_test_util.h"],
    deps = [
        ":mock_jni_env",
        "//src/agent:common",
        "//src/agent:jvariant",
        "//src/agent:model_json",
        "//src/agent:type_util",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "jasmin_utils",
    testonly = 1,
    srcs = ["jasmin_utils.cc"],
    hdrs = ["jasmin_utils.h"],
    copts = ["-Isrc/agent", "-I$(BINDIR)/src/codegen/generated"],
    deps = [
        ":file_utils",
        "//src/agent:common",
        "//src/codegen:jni_test_proxies",
    ],
)

cc_library(
    name = "file_utils",
    testonly = 1,
    srcs = ["file_utils.cc"],
    hdrs = ["file_utils.h"],
    deps = [
        "//src/agent:common",
        "@com_google_googletest//:gtest",
    ]
)

cc_library(
    name = "json_eq_matcher",
    testonly = 1,
    hdrs = ["json_eq_matcher.h"],
    deps = [
        "//src/agent:model_json",
        "@com_google_googletest//:gtest_main",
        "@jsoncpp//:jsoncpp",
    ],
)

cc_library(
    name = "jvm_test_base",
    testonly = 1,
    srcs = ["jvm_test_base.cc"],
    deps = [
        "//src/agent:common",
        "//src/agent:jvm_env",
        # Note, we don't use gtest_main target as it will invoke RUN_ALL_TESTS on its own
        "@com_google_googletest//:gtest",
    ],
    alwayslink = 1,
)

