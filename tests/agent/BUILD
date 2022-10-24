load("//tests/agent:build_defs.bzl", "jvm_test")

jvm_test(
    name = "model_util_test",
    srcs = ["model_util_test.cc"],
    copts = ["-Isrc/agent", "-I$(BINDIR)/src/codegen/generated"],
    cc_deps = [
        "//src/agent:model_util",
        "//src/agent/test_util:fake_jni",
        "//src/agent/test_util:json_eq_matcher",
        "//src/agent/test_util:mock_jvmti_env",
        "//src/codegen:jni_proxies",
        "@com_google_googletest//:gtest",
    ],
    java_deps = [
      "@maven//:com_google_api_client_google_api_client",
    ],
)

cc_test(
    name = "callbacks_monitor_test",
    srcs = ["callbacks_monitor_test.cc"],
    deps = [
        "//src/agent:callbacks_monitor",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "jsoncpp_util_test",
    srcs = ["jsoncpp_util_test.cc"],
    deps = [
        "//src/agent:jsoncpp_util",
        "@com_google_googletest//:gtest_main",
        "@jsoncpp//:jsoncpp",
    ],
)

cc_test(
    name = "array_type_evaluator_test",
    srcs = ["array_type_evaluator_test.cc"],
    deps = [
        "//src/agent:array_type_evaluator",
        "//src/agent/test_util:fake_instance_field_reader",
        "//src/agent/test_util:mock_jni_env",
        "//src/agent/test_util:mock_jvmti_env",
        "//src/agent:model",
        "//src/agent:model_util",
        "//src/agent:static_field_reader",
    ],
)

cc_test(
    name = "byte_source_test",
    srcs = ["byte_source_test.cc"],
    deps = [
        "//src/agent:byte_source",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "observable_test",
    srcs = ["observable_test.cc"],
    deps = [
        "//src/agent:observable",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "scheduler_test",
    srcs = ["scheduler_test.cc"],
    deps = [
        "//src/agent:scheduler",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "jvariant_test",
    srcs = ["jvariant_test.cc"],
    deps = [
        "//src/agent:jvariant",
        "//src/agent/test_util:mock_jvmti_env",
        "//src/agent/test_util:mock_jni_env",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "data_visibility_scrubbed_eval_call_stack_test",
    srcs = ["data_visibility_scrubbed_eval_call_stack_test.cc"],
    deps = [
        "//src/agent:data_visibility_scrubbed_eval_call_stack",
        "//src/agent/test_util:fake_jni",
        "//src/agent/test_util:mock_data_visibility_policy",
        "//src/agent/test_util:mock_eval_call_stack",
        "//src/agent/test_util:mock_jvmti_env",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "debuggee_labels_test",
    srcs = ["debuggee_labels_test.cc"],
    copts = ["-Isrc/agent", "-I$(BINDIR)/src/codegen/generated"],
    deps = [
        "//src/agent:debuggee_labels",
        "//src/agent/test_util:fake_jni",
        "//src/agent:jni_utils",
        "//src/agent:jni_utils_h",
        "//src/agent/test_util:mock_jni_env",
        "//src/agent/test_util:mock_jvmti_env",
        "//src/codegen:mock_jni_proxies",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "auto_jvmti_breakpoint_test",
    srcs = ["auto_jvmti_breakpoint_test.cc"],
    deps = [
        "//src/agent:auto_jvmti_breakpoint",
        "//src/agent/test_util:fake_jni",
        "//src/agent/test_util:mock_breakpoint",
        "//src/agent/test_util:mock_breakpoints_manager",
        "//src/agent/test_util:mock_jvmti_env",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "canary_control_test",
    srcs = ["canary_control_test.cc"],
    copts = ["-Isrc/agent", "-I$(BINDIR)/src/codegen/generated"],
    deps = [
        "//src/agent:callbacks_monitor",
        "//src/agent:canary_control",
        "//src/agent/test_util:fake_jni",
        "//src/agent/test_util:mock_bridge",
        "//src/agent/test_util:mock_jvmti_env",
        "//src/agent:model_util",
        "@com_google_googletest//:gtest_main",
    ],
)

jvm_test(
    name = "class_file_test",
    srcs = ["class_file_test.cc"],
    copts = ["-Isrc/agent", "-I$(BINDIR)/src/codegen/generated"],
    cc_deps = [
        "//src/agent:class_file",
        "//src/agent:jvariant",
        "//src/agent:jvm_class_indexer",
        "//src/agent:jvmti_buffer",
        "//src/agent:type_util",
        "//src/agent/test_util:jasmin_utils",
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

jvm_test(
    name = "class_files_cache_test",
    srcs = ["class_files_cache_test.cc"],
    copts = ["-Isrc/agent", "-I$(BINDIR)/src/codegen/generated"],
    cc_deps = [
        "//src/agent:class_files_cache",
        "//src/agent:jni_utils",
        "//src/agent:jni_utils_h",
        "//src/agent:jvm_class_indexer",
        "//src/agent:type_util",
        "//src/agent/test_util:file_utils",
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

cc_test(
    name = "jni_method_caller_test",
    srcs = ["jni_method_caller_test.cc"],
    deps = [
        "//src/agent:class_metadata_reader",
        "//src/agent/test_util:fake_jni",
        "//src/agent:jni_method_caller",
        "//src/agent:jni_utils",
        "//src/agent:jni_utils_h",
        "//src/agent/test_util:mock_jni_env",
        "//src/agent/test_util:mock_jvmti_env",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "nanojava_locals_test",
    srcs = ["nanojava_locals_test.cc"],
    deps = [
        "//src/agent/test_util:fake_jni",
        "//src/agent/test_util:mock_jvmti_env",
        "//src/agent/test_util:mock_nanojava_internal_error_provider",
        "//src/agent:nanojava_locals",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "nanojava_stack_test",
    srcs = ["nanojava_stack_test.cc"],
    deps = [
        "//src/agent/test_util:fake_jni",
        "//src/agent/test_util:mock_jvmti_env",
        "//src/agent/test_util:mock_nanojava_internal_error_provider",
        "//src/agent:nanojava_stack",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "config_test",
    srcs = ["config_test.cc"],
    deps = [
        "//src/agent:config",
        "//src/agent/test_util:fake_jni",
        "//src/agent/test_util:mock_jvmti_env",
        "//src/agent:type_util",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "config_builder_test",
    srcs = ["config_builder_test.cc"],
    deps = [
        "//src/agent:config_builder",
        "//src/agent/test_util:fake_jni",
        "//src/agent/test_util:mock_jvmti_env",
        "//src/agent:type_util",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "multi_data_visibility_policy_test",
    srcs = ["multi_data_visibility_policy_test.cc"],
    deps = [
        "//src/agent/test_util:mock_data_visibility_policy",
        "//src/agent:multi_data_visibility_policy",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "jvm_eval_call_stack_test",
    srcs = ["jvm_eval_call_stack_test.cc"],
    deps = [
        "//src/agent:common",
        "//src/agent:jvm_eval_call_stack",
        "//src/agent/test_util:mock_jni_env",
        "//src/agent/test_util:mock_jvmti_env",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "jobject_map_test",
    srcs = ["jobject_map_test.cc"],
    deps = [
        "//src/agent:jobject_map",
        "//src/agent/test_util:mock_jni_env",
        "//src/agent/test_util:mock_jvmti_env",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "capture_data_collector_test",
    srcs = ["capture_data_collector_test.cc"],
    deps = [
        "//src/agent:common",
        "//src/agent:capture_data_collector",
        "//src/agent:expression_evaluator_h",
        "//src/agent/test_util:fake_instance_field_reader",
        "//src/agent:instance_field_reader",
        "//src/agent:java_expression_evaluator",
        "//src/agent:jvm_evaluators",
        "//src/agent:local_variable_reader",
        "//src/agent:messages",
        "//src/agent:model_json",
        "//src/agent:model_util",
        "//src/agent:static_field_reader",
        "//src/agent:value_formatter",
        "//src/agent/test_util:json_eq_matcher",
        "//src/agent/test_util:mock_breakpoint_labels_provider",
        "//src/agent/test_util:mock_class_indexer",
        "//src/agent/test_util:mock_eval_call_stack",
        "//src/agent/test_util:mock_jni_env",
        "//src/agent/test_util:mock_jvmti_env",
        "//src/agent/test_util:mock_object_evaluator",
        "//src/agent/test_util:mock_user_id_provider",
        "//src/agent/test_util:named_jvariant_test_util",
        "@com_google_googletest//:gtest_main",
    ],
)

