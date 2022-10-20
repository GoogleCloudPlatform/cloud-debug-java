load("//src/agent:build_defs.bzl", "jvm_test")

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
    name = "jvm_env",
    srcs = ["jvm_env.cc"],
    copts = ["-Isrc/agent", "-I$(BINDIR)/src/codegen/generated"],
    deps = [
        ":common",
        ":jni_utils",
        "//src/codegen:jni_proxies",
    ],
    alwayslink = 1,
)

cc_library(
    name = "jvm_test_base",
    testonly = 1,
    srcs = ["jvm_test_base.cc"],
    deps = [
        ":common",
        ":jvm_env",
        # Note, we don't use gtest_main target as it will invoke RUN_ALL_TESTS on its own
        "@com_google_googletest//:gtest",
    ],
    alwayslink = 1,
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
    name = "file_utils",
    testonly = 1,
    srcs = ["file_utils.cc"],
    hdrs = ["file_utils.h"],
    deps = [
        ":common",
        "@com_google_googletest//:gtest",
    ]
)

cc_library(
    name = "jasmin_utils",
    testonly = 1,
    srcs = ["jasmin_utils.cc"],
    hdrs = ["jasmin_utils.h"],
    copts = ["-Isrc/agent", "-I$(BINDIR)/src/codegen/generated"],
    deps = [
        ":common",
        ":file_utils",
        "//src/codegen:jni_test_proxies",
    ],
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

jvm_test(
    name = "model_util_test",
    srcs = ["model_util_test.cc"],
    copts = ["-Isrc/agent", "-I$(BINDIR)/src/codegen/generated"],
    cc_deps = [
        ":fake_jni",
        ":json_eq_matcher",
        ":mock_jvmti_env",
        ":model_util",
        "//src/codegen:jni_proxies",
        "@com_google_googletest//:gtest",
    ],
    java_deps = [
      "@maven//:com_google_api_client_google_api_client",
    ],
)

cc_library(
    name = "value_formatter",
    srcs = ["value_formatter.cc"],
    hdrs = ["value_formatter.h"],
    deps = [
        ":common",
        ":jni_utils",
        ":jni_utils_h",
        ":model_util",
        ":type_util",
    ],
)

cc_library(
    name = "array_reader",
    hdrs = ["array_reader.h"],
    deps = [
        ":common",
        ":model_util",
    ],
)

cc_library(
    name = "mock_array_reader",
    testonly = 1,
    hdrs = ["mock_array_reader.h"],
    deps = [
        ":array_reader",
        "@com_google_googletest//:gtest_main",
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
    name = "local_variable_reader",
    hdrs = ["local_variable_reader.h"],
    deps = [
        ":common",
        ":model",
        ":type_util",
    ],
)

cc_library(
    name = "fake_local_variable_reader",
    testonly = 1,
    hdrs = ["fake_local_variable_reader.h"],
    deps = [
        ":jvariant",
        ":local_variable_reader",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "jvm_local_variable_reader",
    srcs = ["jvm_local_variable_reader.cc"],
    hdrs = ["jvm_local_variable_reader.h"],
    deps = [
        ":common",
        ":jvariant",
        ":local_variable_reader",
        ":messages",
        ":readers_factory",
    ],
)

cc_library(
    name = "method_locals",
    srcs = ["method_locals.cc"],
    hdrs = ["method_locals.h"],
    deps = [
        ":common",
        ":data_visibility_policy",
        ":jni_utils",
        ":jni_utils_h",
        ":jvm_local_variable_reader",
        ":jvmti_buffer",
        ":mutex",
    ],
)

cc_library(
    name = "json_eq_matcher",
    testonly = 1,
    hdrs = ["json_eq_matcher.h"],
    deps = [
        ":model_json",
        "@com_google_googletest//:gtest_main",
        "@jsoncpp//:jsoncpp",
    ],
)

cc_library(
    name = "readers_factory",
    hdrs = ["readers_factory.h"],
    deps = [
        ":class_metadata_reader",
        ":common",
        ":method_caller",
    ],
)

cc_library(
    name = "mock_readers_factory",
    testonly = 1,
    hdrs = ["mock_readers_factory.h"],
    deps = [
        ":common",
        ":array_reader",
        ":fake_instance_field_reader",
        ":fake_local_variable_reader",
        ":fake_static_field_reader",
        ":readers_factory",
        "@com_google_googletest//:gtest_main",
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
    name = "fake_static_field_reader",
    testonly = 1,
    hdrs = ["fake_static_field_reader.h"],
    deps = [
        ":common",
        ":jvariant",
        ":static_field_reader",
        "@com_google_googletest//:gtest_main",
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
    ],
)

cc_library(
    name = "object_evaluator",
    hdrs = ["object_evaluator.h"],
    deps = [
        ":common",
    ],
)

cc_library(
    name = "mock_object_evaluator",
    testonly = 1,
    hdrs = ["mock_object_evaluator.h"],
    deps = [
        ":object_evaluator",
        "@com_google_googletest//:gtest_main",
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
    name = "data_visibility_scrubbed_eval_call_stack",
    srcs = ["data_visibility_scrubbed_eval_call_stack.cc"],
    hdrs = ["data_visibility_scrubbed_eval_call_stack.h"],
    deps = [
        ":common",
        ":data_visibility_policy",
        ":eval_call_stack",
        ":jvmti_buffer",
    ],
)

cc_test(
    name = "data_visibility_scrubbed_eval_call_stack_test",
    srcs = ["data_visibility_scrubbed_eval_call_stack_test.cc"],
    deps = [
        ":data_visibility_scrubbed_eval_call_stack",
        ":fake_jni",
        ":mock_data_visibility_policy",
        ":mock_eval_call_stack",
        ":mock_jvmti_env",
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
    name = "class_indexer",
    hdrs = ["class_indexer.h"],
    deps = [
        ":common",
        ":jni_utils",
        ":jni_utils_h",
        ":jvariant",
        ":observable",
        ":type_util",
    ],
)

cc_library(
    name = "mock_class_indexer",
    testonly = 1,
    hdrs = ["mock_class_indexer.h"],
    deps = [
        ":class_indexer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "jvm_class_indexer",
    srcs = ["jvm_class_indexer.cc"],
    hdrs = ["jvm_class_indexer.h"],
    copts = ["-Isrc/agent", "-I$(BINDIR)/src/codegen/generated"],
    deps = [
        ":class_indexer",
        ":common",
        ":jni_utils",
        ":jni_utils_h",
        ":jobject_map",
        ":jvmti_buffer",
        ":mutex",
        ":type_util",
        "//src/codegen:jni_proxies",
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

cc_test(
    name = "debuggee_labels_test",
    srcs = ["debuggee_labels_test.cc"],
    copts = ["-Isrc/agent", "-I$(BINDIR)/src/codegen/generated"],
    deps = [
        ":debuggee_labels",
        ":fake_jni",
        ":jni_utils",
        ":jni_utils_h",
        ":mock_jni_env",
        ":mock_jvmti_env",
        "//src/codegen:mock_jni_proxies",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "breakpoint_labels_provider",
    hdrs = ["breakpoint_labels_provider.h"],
    deps = [
        ":common",
    ],
)

cc_library(
    name = "mock_breakpoint_labels_provider",
    testonly = 1,
    hdrs = ["mock_breakpoint_labels_provider.h"],
    deps = [
        ":breakpoint_labels_provider",
        ":class_indexer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "user_id_provider",
    hdrs = ["user_id_provider.h"],
    deps = [
        ":common",
    ],
)

cc_library(
    name = "jni_user_id_provider",
    srcs = ["jni_user_id_provider.cc"],
    hdrs = ["jni_user_id_provider.h"],
    copts = ["-Isrc/agent", "-I$(BINDIR)/src/codegen/generated"],
    deps = [
        ":common",
        ":jni_utils",
        ":jni_utils_h",
        ":user_id_provider",
        "//src/codegen:jni_proxies",
    ],
)

cc_library(
    name = "mock_user_id_provider",
    testonly = 1,
    hdrs = ["mock_user_id_provider.h"],
    deps = [
        ":user_id_provider",
        "@com_google_googletest//:gtest_main",
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
        "@com_github_gflags_gflags//:gflags",
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
    name = "class_file",
    srcs = ["class_file.cc"],
    hdrs = ["class_file.h"],
    copts = ["-Isrc/agent", "-I$(BINDIR)/src/codegen/generated"],
    deps = [
        ":byte_source",
        ":class_indexer",
        ":class_metadata_reader",
        ":common",
        ":jvmti_buffer",
        ":nullable",
        ":type_util",
        "//src/codegen:jni_proxies",
    ],
)

jvm_test(
    name = "class_file_test",
    srcs = ["class_file_test.cc"],
    copts = ["-Isrc/agent", "-I$(BINDIR)/src/codegen/generated"],
    cc_deps = [
        ":class_file",
        ":jasmin_utils",
        ":jvariant",
        ":jvm_class_indexer",
        ":jvmti_buffer",
        ":type_util",
        "//src/codegen:jni_proxies",
        "//src/codegen:jni_test_proxies",
        "@com_google_googletest//:gtest",
        "@com_googlesource_code_re2//:re2",
    ],
    java_deps = [
        "//src/agent/internals/src/main/java/com/google/devtools/cdbg/debuglets/java:mapper",
        "//src/agent/internals/src/test/java/com/google/devtools/cdbg/debuglets/java:class_file_textifier",
        "//:jasmin",
    ],
)

cc_library(
    name = "class_files_cache",
    srcs = ["class_files_cache.cc"],
    hdrs = ["class_files_cache.h"],
    deps = [
        ":class_file",
        ":class_indexer",
        ":common",
        ":jobject_map",
        ":mutex",
        ":nullable",
    ],
)

jvm_test(
    name = "class_files_cache_test",
    srcs = ["class_files_cache_test.cc"],
    copts = ["-Isrc/agent", "-I$(BINDIR)/src/codegen/generated"],
    cc_deps = [
        ":class_files_cache",
        ":file_utils",
        ":jni_utils",
        ":jni_utils_h",
        ":jvm_class_indexer",
        ":type_util",
        "//src/codegen:jni_proxies",
        "//src/codegen:jni_test_proxies",
        "@com_google_googletest//:gtest",
    ],
    java_deps = [
        "//src/agent/internals/src/main/java/com/google/devtools/cdbg/debuglets/java:mapper",
        "//src/agent/internals/src/test/java/com/google/devtools/cdbg/debuglets/java:class_file_textifier",
        "//:jasmin",
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
    name = "safe_method_caller",
    srcs = ["safe_method_caller.cc"],
    hdrs = ["safe_method_caller.h"],
    copts = ["-Isrc/agent", "-I$(BINDIR)/src/codegen/generated"],
    deps = [
        ":class_file",
        ":class_files_cache",
        ":class_indexer",
        ":class_metadata_reader",
        ":common",
        ":config",
        ":jni_method_caller",
        ":jni_utils",
        ":jni_utils_h",
        ":jobject_map",
        ":jvariant",
        ":messages",
        ":method_call_result",
        ":method_caller",
        ":model",
        ":nanojava_interpreter",
        ":type_util",
        "//src/codegen:jni_proxies",
    ],
)

cc_library(
    name = "jni_method_caller",
    srcs = ["jni_method_caller.cc"],
    hdrs = ["jni_method_caller.h"],
    deps = [
        ":class_metadata_reader",
        ":common",
        ":jni_utils",
        ":jni_utils_h",
        ":jvariant",
        ":method_call_result",
    ],
)

cc_test(
    name = "jni_method_caller_test",
    srcs = ["jni_method_caller_test.cc"],
    deps = [
        ":class_metadata_reader",
        ":fake_jni",
        ":jni_method_caller",
        ":jni_utils",
        ":jni_utils_h",
        ":mock_jni_env",
        ":mock_jvmti_env",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "method_call_result",
    hdrs = ["method_call_result.h"],
    deps = [
        ":common",
        ":jvariant",
        ":messages",
        ":model",
        ":type_util",
    ],
)

cc_library(
    name = "nanojava_internal_error_builder",
    hdrs = ["nanojava_internal_error_builder.h"],
    deps = [
        ":common",
        ":method_call_result",
    ],
)

cc_library(
    name = "mock_nanojava_internal_error_provider",
    testonly = 1,
    hdrs = ["mock_nanojava_internal_error_provider.h"],
    deps = [
        ":nanojava_internal_error_builder",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "nanojava_slot",
    hdrs = ["nanojava_slot.h"],
    deps = [
        ":common",
    ],
)

cc_library(
    name = "nanojava_locals",
    srcs = ["nanojava_locals.cc"],
    hdrs = ["nanojava_locals.h"],
    deps = [
        ":common",
        ":jvariant",
        ":nanojava_internal_error_builder",
        ":nanojava_slot",
    ],
)

cc_test(
    name = "nanojava_locals_test",
    srcs = ["nanojava_locals_test.cc"],
    deps = [
        ":fake_jni",
        ":mock_jvmti_env",
        ":mock_nanojava_internal_error_provider",
        ":nanojava_locals",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "nanojava_stack",
    srcs = ["nanojava_stack.cc"],
    hdrs = ["nanojava_stack.h"],
    deps = [
        ":common",
        ":jvariant",
        ":nanojava_internal_error_builder",
        ":nanojava_slot",
    ],
)

cc_test(
    name = "nanojava_stack_test",
    srcs = ["nanojava_stack_test.cc"],
    deps = [
        ":fake_jni",
        ":mock_jvmti_env",
        ":mock_nanojava_internal_error_provider",
        ":nanojava_stack",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "nanojava_interpreter",
    srcs = ["nanojava_interpreter.cc"],
    hdrs = ["nanojava_interpreter.h"],
    copts = ["-Isrc/agent", "-I$(BINDIR)/src/codegen/generated"],
    deps = [
        ":class_file",
        ":common",
        ":jni_utils",
        ":jvariant",
        ":messages",
        ":method_call_result",
        ":model",
        ":nanojava_internal_error_builder",
        ":nanojava_locals",
        ":nanojava_slot",
        ":nanojava_stack",
        "//src/codegen:jni_proxies",
    ],
)

cc_library(
    name = "safe_caller_proxies",
    srcs = ["safe_caller_proxies.cc"],
    hdrs = ["safe_caller_proxies.h"],
    copts = ["-Isrc/agent", "-I$(BINDIR)/src/codegen/generated"],
    deps = [
        ":class_metadata_reader",
        ":common",
        ":jvariant",
        ":method_call_result",
        ":method_caller",
        ":safe_method_caller",
        ":type_util",
        "//src/codegen:jni_proxies",
    ],
)

cc_library(
    name = "config",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    deps = [
        ":common",
        ":jvariant",
        ":method_call_result",
    ],
)

cc_test(
    name = "config_test",
    srcs = ["config_test.cc"],
    deps = [
        ":config",
        ":fake_jni",
        ":mock_jvmti_env",
        ":type_util",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "config_builder",
    srcs = ["config_builder.cc"],
    hdrs = ["config_builder.h"],
    deps = [
        ":common",
        ":config",
        ":safe_caller_proxies",
    ],
)

cc_test(
    name = "config_builder_test",
    srcs = ["config_builder_test.cc"],
    deps = [
        ":config_builder",
        ":fake_jni",
        ":mock_jvmti_env",
        ":type_util",
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

cc_library(
    name = "encoding_util",
    srcs = ["encoding_util.cc"],
    hdrs = ["encoding_util.h"],
    deps = [
        ":common",
    ],
)

cc_library(
    name = "expression_evaluator_h",
    hdrs = ["expression_evaluator.h"],
    deps = [
        ":common",
        ":model_util",
        ":nullable",
        ":type_util",
    ],
)

cc_library(
    name = "expression_util_h",
    hdrs = ["expression_util.h"],
    deps = [
        ":common",
        ":model",
    ],
)

genrule(
    name = "java_expression_grammar_gen",
    srcs = ["java_expression.g"],
    outs = [
        "antlrgen/JavaExpressionCompiler.cc",
        "antlrgen/JavaExpressionCompiler.hpp",
        "antlrgen/JavaExpressionCompilerTokenTypes.hpp",
        "antlrgen/JavaExpressionLexer.cc",
        "antlrgen/JavaExpressionLexer.hpp",
        "antlrgen/JavaExpressionLexerTokenTypes.hpp",
        "antlrgen/JavaExpressionLexerTokenTypes.txt",
        "antlrgen/JavaExpressionParser.cc",
        "antlrgen/JavaExpressionParser.hpp",
        "antlrgen/JavaExpressionParserTokenTypes.hpp",
    ],
    cmd = "$(location //third_party/antlr:antlr_tool) -o $(@D)/antlrgen $(location java_expression.g)",
    tools = ["//third_party/antlr:antlr_tool"],
)

cc_library(
    name = "java_expression_evaluator",
    srcs = [
        "array_expression_evaluator.cc",
        "binary_expression_evaluator.cc",
        "conditional_operator_evaluator.cc",
        "expression_util.cc",
        "field_evaluator.cc",
        "identifier_evaluator.cc",
        "instanceof_binary_expression_evaluator.cc",
        "java_expression.cc",
        "method_call_evaluator.cc",
        "string_evaluator.cc",
        "type_cast_operator_evaluator.cc",
        "unary_expression_evaluator.cc",
        ":java_expression_grammar_gen",
    ],
    hdrs = [
        "antlrgen/JavaExpressionCompiler.hpp",
        "antlrgen/JavaExpressionLexer.hpp",
        "antlrgen/JavaExpressionParser.hpp",
        "array_expression_evaluator.h",
        "binary_expression_evaluator.h",
        "conditional_operator_evaluator.h",
        "expression_util.h",
        "field_evaluator.h",
        "identifier_evaluator.h",
        "instanceof_binary_expression_evaluator.h",
        "java_expression.h",
        "literal_evaluator.h",
        "method_call_evaluator.h",
        "numeric_cast_evaluator.h",
        "string_evaluator.h",
        "type_cast_operator_evaluator.h",
        "unary_expression_evaluator.h",
    ],
    copts = [
        "-I$(BINDIR)/src/agent/antlrgen",
        "-Isrc/agent",
        "-Ithird_party/antlr/lib/cpp",
    ],
    deps = [
        ":array_reader",
        ":class_indexer",
        ":class_metadata_reader",
        ":common",
        ":expression_evaluator_h",
        ":expression_util_h",
        ":instance_field_reader",
        ":jni_utils",
        ":jni_utils_h",
        ":jvariant",
        ":jvmti_buffer",
        ":local_variable_reader",
        ":messages",
        ":method_caller",
        ":model",
        ":model_util",
        ":readers_factory",
        ":static_field_reader",
        ":type_util",
        "//third_party/antlr/lib/cpp/antlr:headers",
        "//third_party/antlr/lib/cpp/src:antlr",
    ],
)

cc_library(
    name = "eval_call_stack",
    hdrs = ["eval_call_stack.h"],
    deps = [
        ":common",
    ],
)

cc_library(
    name = "mock_eval_call_stack",
    testonly = 1,
    hdrs = ["mock_eval_call_stack.h"],
    deps = [
        ":eval_call_stack",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "jvm_eval_call_stack",
    srcs = ["jvm_eval_call_stack.cc"],
    hdrs = ["jvm_eval_call_stack.h"],
    deps = [
        ":common",
        ":eval_call_stack",
        ":jni_utils",
        ":jni_utils_h",
        ":jvmti_buffer",
        ":mutex",
        "@com_github_gflags_gflags//:gflags",
    ],
)

cc_test(
    name = "jvm_eval_call_stack_test",
    srcs = ["jvm_eval_call_stack_test.cc"],
    deps = [
        ":common",
        ":jvm_eval_call_stack",
        ":mock_jni_env",
        ":mock_jvmti_env",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "jobject_map",
    hdrs = ["jobject_map.h"],
    deps = [
        ":common",
    ],
)

cc_test(
    name = "jobject_map_test",
    srcs = ["jobject_map_test.cc"],
    deps = [
        ":jobject_map",
        ":mock_jni_env",
        ":mock_jvmti_env",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "jvm_evaluators",
    hdrs = ["jvm_evaluators.h"],
    deps = [
        ":breakpoint_labels_provider",
        ":config",
        ":method_caller",
        ":user_id_provider",
    ],
)

cc_library(
    name = "capture_data_collector",
    srcs = ["capture_data_collector.cc"],
    hdrs = ["capture_data_collector.h"],
    deps = [
        ":breakpoint_labels_provider",
        ":class_indexer",
        ":class_metadata_reader",
        ":common",
        ":encoding_util",
        ":eval_call_stack",
        ":expression_evaluator_h",
        ":expression_util_h",
        ":jobject_map",
        ":jvm_evaluators",
        ":local_variable_reader",
        ":messages",
        ":method_locals",
        ":model",
        ":model_util",
        ":object_evaluator",
        ":readers_factory",
        ":type_util",
        ":user_id_provider",
        ":value_formatter",
    ],
)

cc_test(
    name = "capture_data_collector_test",
    srcs = ["capture_data_collector_test.cc"],
    deps = [
        ":common",
        ":capture_data_collector",
        ":expression_evaluator_h",
        ":fake_instance_field_reader",
        ":instance_field_reader",
        ":java_expression_evaluator",
        ":json_eq_matcher",
        ":jvm_evaluators",
        ":local_variable_reader",
        ":messages",
        ":mock_breakpoint_labels_provider",
        ":mock_class_indexer",
        ":mock_eval_call_stack",
        ":mock_jni_env",
        ":mock_jvmti_env",
        ":mock_object_evaluator",
        ":mock_user_id_provider",
        ":model_json",
        ":model_util",
        ":named_jvariant_test_util",
        ":static_field_reader",
        ":value_formatter",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "named_jvariant_test_util",
    testonly = 1,
    srcs = ["named_jvariant_test_util.cc"],
    hdrs = ["named_jvariant_test_util.h"],
    deps = [
        ":common",
        ":jvariant",
        ":mock_jni_env",
        ":model_json",
        ":type_util",
        "@com_google_googletest//:gtest_main",
    ],
)
