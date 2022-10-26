load("//tests/agent:build_defs.bzl", "jvm_test")

cc_test(
    name = "array_type_evaluator_test",
    srcs = ["array_type_evaluator_test.cc"],
    deps = [
        ":fake_instance_field_reader",
        ":mock_jni_env",
        ":mock_jvmti_env",
        "//src/agent:array_type_evaluator",
        "//src/agent:model",
        "//src/agent:model_util",
        "//src/agent:static_field_reader",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "auto_jvmti_breakpoint_test",
    srcs = ["auto_jvmti_breakpoint_test.cc"],
    deps = [
        ":fake_jni",
        ":mock_breakpoint",
        ":mock_breakpoints_manager",
        ":mock_jvmti_env",
        "//src/agent:auto_jvmti_breakpoint",
        "@com_google_googletest//:gtest_main",
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
    name = "canary_control_test",
    srcs = ["canary_control_test.cc"],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated",
    ],
    deps = [
        ":fake_jni",
        ":mock_bridge",
        ":mock_jvmti_env",
        "//src/agent:callbacks_monitor",
        "//src/agent:canary_control",
        "//src/agent:model_util",
        "@com_google_googletest//:gtest_main",
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
    name = "capture_data_collector_test",
    srcs = ["capture_data_collector_test.cc"],
    deps = [
        ":fake_instance_field_reader",
        ":json_eq_matcher",
        ":mock_breakpoint_labels_provider",
        ":mock_class_indexer",
        ":mock_eval_call_stack",
        ":mock_jni_env",
        ":mock_jvmti_env",
        ":mock_object_evaluator",
        ":mock_user_id_provider",
        ":named_jvariant_test_util",
        "//src/agent:capture_data_collector",
        "//src/agent:common",
        "//src/agent:expression_evaluator_h",
        "//src/agent:instance_field_reader",
        "//src/agent:java_expression_evaluator",
        "//src/agent:jvm_evaluators",
        "//src/agent:local_variable_reader",
        "//src/agent:messages",
        "//src/agent:model_json",
        "//src/agent:model_util",
        "//src/agent:static_field_reader",
        "//src/agent:value_formatter",
        "@com_google_googletest//:gtest_main",
    ],
)

jvm_test(
    name = "class_file_test",
    srcs = ["class_file_test.cc"],
    cc_deps = [
        ":jasmin_utils",
        "//src/agent:class_file",
        "//src/agent:jvariant",
        "//src/agent:jvm_class_indexer",
        "//src/agent:jvmti_buffer",
        "//src/agent:type_util",
        "//src/codegen:jni_proxies",
        "//src/codegen:jni_test_proxies",
        "@com_google_googletest//:gtest",
        "@com_googlesource_code_re2//:re2",
    ],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated",
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
    cc_deps = [
        ":file_utils",
        "//src/agent:class_files_cache",
        "//src/agent:jni_utils",
        "//src/agent:jni_utils_h",
        "//src/agent:jvm_class_indexer",
        "//src/agent:type_util",
        "//src/codegen:jni_proxies",
        "//src/codegen:jni_test_proxies",
        "@com_google_googletest//:gtest",
    ],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated",
    ],
    java_deps = [
        "//src/agent/internals/src/main/java/com/google/devtools/cdbg/debuglets/java:mapper",
        "//src/agent/internals/src/test/java/com/google/devtools/cdbg/debuglets/java:class_file_textifier",
        "//:jasmin",
    ],
)

cc_test(
    name = "config_test",
    srcs = ["config_test.cc"],
    deps = [
        ":fake_jni",
        ":mock_jvmti_env",
        "//src/agent:config",
        "//src/agent:type_util",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "config_builder_test",
    srcs = ["config_builder_test.cc"],
    deps = [
        ":fake_jni",
        ":mock_jvmti_env",
        "//src/agent:config_builder",
        "//src/agent:type_util",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "data_visibility_scrubbed_eval_call_stack_test",
    srcs = ["data_visibility_scrubbed_eval_call_stack_test.cc"],
    deps = [
        ":fake_jni",
        ":mock_data_visibility_policy",
        ":mock_eval_call_stack",
        ":mock_jvmti_env",
        "//src/agent:data_visibility_scrubbed_eval_call_stack",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "debuggee_labels_test",
    srcs = ["debuggee_labels_test.cc"],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated",
    ],
    deps = [
        ":fake_jni",
        ":mock_jni_env",
        ":mock_jvmti_env",
        "//src/agent:debuggee_labels",
        "//src/agent:jni_utils",
        "//src/agent:jni_utils_h",
        "//src/codegen:mock_jni_proxies",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "encoding_util_test",
    srcs = ["encoding_util_test.cc"],
    deps = [
        "//src/agent:encoding_util",
        "@com_google_googletest//:gtest_main",
        # Provides runetochar in util/utf.h, which the test uses.
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_test(
    name = "expressions_test",
    srcs = ["expressions_test.cc"],
    deps = [
        ":fake_jni",
        ":mock_array_reader",
        ":mock_class_indexer",
        ":mock_jvmti_env",
        ":mock_method_caller",
        ":mock_readers_factory",
        "//src/agent:expression_evaluator_h",
        "//src/agent:java_expression_evaluator",
        "//src/agent:jni_utils_h",
        "//src/agent:local_variable_reader",
        "//src/agent:messages",
        "//src/agent:model",
        "//src/agent:model_util",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "format_queue_test",
    srcs = ["format_queue_test.cc"],
    deps = [
        ":fake_jni",
        ":mock_jvmti_env",
        "//src/agent:config_builder",
        "//src/agent:format_queue",
        "//src/agent:model_util",
        "//src/agent:statistician",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "generic_type_evaluator_test",
    srcs = ["generic_type_evaluator_test.cc"],
    deps = [
        ":fake_instance_field_reader",
        ":mock_jni_env",
        ":mock_jvmti_env",
        "//src/agent:generic_type_evaluator",
        "//src/agent:messages",
        "//src/agent:model",
        "//src/agent:model_util",
        "//src/agent:static_field_reader",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "glob_data_visibility_policy_test",
    srcs = ["glob_data_visibility_policy_test.cc"],
    deps = [
        ":fake_jni",
        ":mock_jvmti_env",
        "//src/agent:glob_data_visibility_policy",
        #"//util/glob",
        #"//util/random:mt_random",
        "@com_google_googletest//:gtest_main",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_test(
    name = "java_expression_parser_test",
    srcs = ["java_expression_parser_test.cc"],
    copts = [
        "-I$(BINDIR)/src/agent/antlrgen",
        "-Isrc/agent",
        "-Ithird_party/antlr/lib/cpp",
    ],
    deps = [
        ":fake_jni",
        ":mock_jvmti_env",
        "//src/agent:common",
        "//src/agent:java_expression_evaluator",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "jni_method_caller_test",
    srcs = ["jni_method_caller_test.cc"],
    deps = [
        ":fake_jni",
        ":mock_jni_env",
        ":mock_jvmti_env",
        "//src/agent:class_metadata_reader",
        "//src/agent:jni_method_caller",
        "//src/agent:jni_utils",
        "//src/agent:jni_utils_h",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "jni_utils_test",
    srcs = ["jni_utils_test.cc"],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated",
    ],
    deps = [
        ":fake_jni",
        ":mock_jvmti_env",
        "//src/agent:jni_utils",
        "//src/agent:jni_utils_h",
        "//src/codegen:mock_jni_proxies",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "jobject_map_test",
    srcs = ["jobject_map_test.cc"],
    deps = [
        ":mock_jni_env",
        ":mock_jvmti_env",
        "//src/agent:jobject_map",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "jsoncpp_util_test",
    srcs = ["jsoncpp_util_test.cc"],
    deps = [
        "//src/agent:jsoncpp_util",
        "@com_google_googletest//:gtest_main",
        "@jsoncpp",
    ],
)

cc_test(
    name = "jvariant_test",
    srcs = ["jvariant_test.cc"],
    deps = [
        ":mock_jni_env",
        ":mock_jvmti_env",
        "//src/agent:jvariant",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "jvm_breakpoint_test",
    srcs = ["jvm_breakpoint_test.cc"],
    deps = [
        ":fake_jni",
        ":json_eq_matcher",
        ":mock_breakpoint_labels_provider",
        ":mock_breakpoints_manager",
        ":mock_class_indexer",
        ":mock_class_metadata_reader",
        ":mock_class_path_lookup",
        ":mock_dynamic_logger",
        ":mock_eval_call_stack",
        ":mock_jvmti_env",
        ":mock_object_evaluator",
        ":mock_user_id_provider",
        "//src/agent:class_metadata_reader",
        "//src/agent:config_builder",
        "//src/agent:format_queue",
        "//src/agent:instance_field_reader",
        "//src/agent:jni_utils_h",
        "//src/agent:jvm_breakpoint",
        "//src/agent:jvm_evaluators",
        "//src/agent:messages",
        "//src/agent:method_locals",
        "//src/agent:model_json",
        "//src/agent:model_util",
        "//src/agent:object_evaluator",
        "//src/agent:resolved_source_location",
        "//src/agent:static_field_reader",
        "//src/agent:statistician",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "jvm_breakpoints_manager_test",
    srcs = ["jvm_breakpoints_manager_test.cc"],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated",
    ],
    deps = [
        ":fake_jni",
        ":mock_breakpoint",
        ":mock_bridge",
        ":mock_class_indexer",
        ":mock_class_metadata_reader",
        ":mock_class_path_lookup",
        ":mock_dynamic_logger",
        ":mock_eval_call_stack",
        ":mock_jvmti_env",
        ":mock_object_evaluator",
        "//src/agent:callbacks_monitor",
        "//src/agent:format_queue",
        "//src/agent:jvm_breakpoints_manager",
        "//src/agent:jvm_evaluators",
        "//src/agent:messages",
        "//src/agent:method_locals",
        "//src/agent:model_util",
        "//src/agent:object_evaluator",
        "//src/agent:resolved_source_location",
        "//src/agent:statistician",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "jvm_class_indexer_test",
    srcs = ["jvm_class_indexer_test.cc"],
    deps = [
        ":fake_jni",
        ":mock_jni_env",
        ":mock_jvmti_env",
        "//src/agent:jni_utils_h",
        "//src/agent:jvm_class_indexer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "jvm_class_metadata_reader_test",
    srcs = ["jvm_class_metadata_reader_test.cc"],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated",
    ],
    deps = [
        ":mock_jni_env",
        ":mock_jvmti_env",
        "//src/agent:glob_data_visibility_policy",
        "//src/agent:instance_field_reader",
        "//src/agent:jvm_class_metadata_reader",
        "//src/agent:static_field_reader",
        "//src/agent:structured_data_visibility_policy",
        "//src/codegen:mock_jni_proxies",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "jvm_eval_call_stack_test",
    srcs = ["jvm_eval_call_stack_test.cc"],
    deps = [
        ":mock_jni_env",
        ":mock_jvmti_env",
        "//src/agent:common",
        "//src/agent:jvm_eval_call_stack",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "jvm_instance_field_reader_test",
    srcs = ["jvm_instance_field_reader_test.cc"],
    deps = [
        ":mock_jni_env",
        ":mock_jvmti_env",
        "//src/agent:jvm_instance_field_reader",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "jvm_local_variable_reader_test",
    srcs = ["jvm_local_variable_reader_test.cc"],
    deps = [
        ":fake_jni",
        ":mock_jvmti_env",
        "//src/agent:jvm_local_variable_reader",
        "//src/agent:readers_factory",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "jvm_object_array_reader_test",
    srcs = ["jvm_object_array_reader_test.cc"],
    deps = [
        ":fake_jni",
        ":mock_jni_env",
        ":mock_jvmti_env",
        "//src/agent:jni_utils",
        "//src/agent:jni_utils_h",
        "//src/agent:jvm_object_array_reader",
        "//src/agent:messages",
        "//src/agent:model",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "jvm_object_evaluator_test",
    srcs = ["jvm_object_evaluator_test.cc"],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated",
    ],
    deps = [
        ":fake_instance_field_reader",
        ":fake_jni",
        ":mock_class_indexer",
        ":mock_class_metadata_reader",
        ":mock_jvmti_env",
        ":mock_method_caller",
        "//src/agent:config_builder",
        "//src/agent:instance_field_reader",
        "//src/agent:jni_utils",
        "//src/agent:jni_utils_h",
        "//src/agent:jvm_object_evaluator",
        "//src/agent:messages",
        "//src/agent:model",
        "//src/agent:static_field_reader",
        "//src/agent:type_evaluator",
        "//src/codegen:jni_proxies",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "jvm_primitive_array_reader_test",
    srcs = ["jvm_primitive_array_reader_test.cc"],
    deps = [
        ":fake_jni",
        ":mock_jni_env",
        ":mock_jvmti_env",
        "//src/agent:jni_utils",
        "//src/agent:jni_utils_h",
        "//src/agent:jvm_primitive_array_reader",
        "//src/agent:messages",
        "//src/agent:model",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "jvm_static_field_reader_test",
    srcs = ["jvm_static_field_reader_test.cc"],
    deps = [
        ":fake_jni",
        ":mock_jni_env",
        ":mock_jvmti_env",
        "//src/agent:jvm_static_field_reader",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "log_data_collector_test",
    srcs = ["log_data_collector_test.cc"],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated",
    ],
    deps = [
        ":fake_jni",
        ":mock_jvmti_env",
        ":mock_method_caller",
        ":mock_object_evaluator",
        ":mock_readers_factory",
        "//src/agent:model_json",
        "//src/agent:model_util",
        "//src/agent:config_builder",
        "//src/agent:expression_evaluator_h",
        "//src/agent:java_expression_evaluator",
        "//src/agent:log_data_collector",
        "//src/codegen:mock_jni_proxies",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "method_locals_test",
    srcs = ["method_locals_test.cc"],
    deps = [
        ":mock_jni_env",
        ":mock_jvmti_env",
        "//src/agent:common",
        "//src/agent:local_variable_reader",
        "//src/agent:method_locals",
        "//src/agent:structured_data_visibility_policy",
        "//src/agent:readers_factory",
        "@com_google_googletest//:gtest_main",
    ],
)

jvm_test(
    name = "model_json_test",
    srcs = ["model_json_test.cc"],
    cc_deps = [
        ":fake_jni",
        ":mock_jvmti_env",
        "//src/agent:model_json",
        "//src/agent:model_util",
        "//src/codegen:jni_proxies",
        "@com_google_googletest//:gtest",
    ],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated",
    ],
    java_deps = [
        "@maven//:com_google_api_client_google_api_client",
    ],
)

jvm_test(
    name = "model_util_test",
    srcs = ["model_util_test.cc"],
    cc_deps = [
        ":fake_jni",
        ":json_eq_matcher",
        ":mock_jvmti_env",
        "//src/agent:model_util",
        "//src/codegen:jni_proxies",
        "@com_google_googletest//:gtest",
    ],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated",
    ],
    java_deps = [
        "@maven//:com_google_api_client_google_api_client",
    ],
)

cc_test(
    name = "multi_data_visibility_policy_test",
    srcs = ["multi_data_visibility_policy_test.cc"],
    deps = [
        ":mock_data_visibility_policy",
        "//src/agent:multi_data_visibility_policy",
        "@com_google_googletest//:gtest_main",
    ],
)

jvm_test(
    name = "nanojava_interpreter_test",
    srcs = ["nanojava_interpreter_test.cc"],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated",
    ],
    cc_deps = [
        ":jasmin_utils",
        "//src/agent:jni_method_caller",
        "//src/agent:jvm_class_indexer",
        "//src/agent:model_util",
        "//src/agent:nanojava_interpreter",
        "//src/agent:type_util",
        "//src/codegen:jni_proxies",
        "//src/codegen:jni_test_proxies",
        "@com_google_googletest//:gtest",
    ],
    java_deps = [
        "//:jasmin",
        "//src/agent/internals/src/test/java/com/google/devtools/cdbg/debuglets/java:nano_java_interpreter_test_lib",
        "//src/agent/internals/src/test/java/com/google/devtools/cdbg/debuglets/java:class_file_textifier",
    ],
)

cc_test(
    name = "nanojava_locals_test",
    srcs = ["nanojava_locals_test.cc"],
    deps = [
        ":fake_jni",
        ":mock_jvmti_env",
        ":mock_nanojava_internal_error_provider",
        "//src/agent:nanojava_locals",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "nanojava_stack_test",
    srcs = ["nanojava_stack_test.cc"],
    deps = [
        ":fake_jni",
        ":mock_jvmti_env",
        ":mock_nanojava_internal_error_provider",
        "//src/agent:nanojava_stack",
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
    name = "statistician_test",
    srcs = ["statistician_test.cc"],
    deps = [
        ":mock_jvmti_env",
        "//src/agent:jni_utils",
        "//src/agent:statistician",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "structured_data_visibility_policy_test",
    srcs = ["structured_data_visibility_policy_test.cc"],
    deps = [
        ":fake_jni",
        ":mock_jvmti_env",
        "//src/agent:structured_data_visibility_policy",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "type_util_test",
    srcs = ["type_util_test.cc"],
    deps = [
        ":fake_jni",
        ":mock_jvmti_env",
        "//src/agent:type_util",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "value_formatter_test",
    srcs = ["value_formatter_test.cc"],
    deps = [
        ":fake_jni",
        ":mock_jvmti_env",
        "//src/agent:model_util",
        "//src/agent:value_formatter",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "worker_test",
    srcs = ["worker_test.cc"],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated",
    ],
    deps = [
        ":fake_jni",
        ":mock_bridge",
        ":mock_class_path_lookup",
        ":mock_jvmti_env",
        ":mock_worker_provider",
        "//src/agent:agent_thread",
        "//src/agent:callbacks_monitor",
        "//src/agent:common",
        "//src/agent:format_queue",
        "//src/agent:mutex",
        "//src/agent:semaphore",
        "//src/agent:statistician",
        "//src/agent:worker",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "yaml_data_visibility_config_reader_test",
    srcs = ["yaml_data_visibility_config_reader_test.cc"],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated",
    ],
    deps = [
        ":fake_jni",
        ":mock_class_path_lookup",
        ":mock_jni_env",
        ":mock_jvmti_env",
        "//src/agent:debuggee_labels",
        "//src/agent:yaml_data_visibility_config_reader",
        "//src/codegen:mock_jni_proxies",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "mock_array_reader",
    testonly = 1,
    hdrs = ["mock_array_reader.h"],
    deps = [
        "//src/agent:array_reader",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "mock_breakpoint",
    testonly = 1,
    hdrs = ["mock_breakpoint.h"],
    deps = [
        "//src/agent:breakpoint",
        "//src/agent:model_util",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "mock_breakpoints_manager",
    testonly = 1,
    hdrs = ["mock_breakpoints_manager.h"],
    deps = [
        "//src/agent:breakpoints_manager",
        "//src/agent:model_json",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "mock_breakpoint_labels_provider",
    testonly = 1,
    hdrs = ["mock_breakpoint_labels_provider.h"],
    deps = [
        "//src/agent:breakpoint_labels_provider",
        "//src/agent:class_indexer",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "mock_bridge",
    testonly = 1,
    hdrs = ["mock_bridge.h"],
    deps = [
        "//src/agent:bridge",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "mock_class_indexer",
    testonly = 1,
    hdrs = ["mock_class_indexer.h"],
    deps = [
        "//src/agent:class_indexer",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "mock_class_metadata_reader",
    testonly = 1,
    hdrs = ["mock_class_metadata_reader.h"],
    deps = [
        "//src/agent:class_metadata_reader",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "mock_class_path_lookup",
    testonly = 1,
    hdrs = ["mock_class_path_lookup.h"],
    deps = [
        "//src/agent:class_path_lookup",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "mock_data_visibility_policy",
    testonly = 1,
    hdrs = ["mock_data_visibility_policy.h"],
    deps = [
        "//src/agent:data_visibility_policy",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "mock_dynamic_logger",
    testonly = 1,
    hdrs = ["mock_dynamic_logger.h"],
    deps = [
        "//src/agent:dynamic_logger",
        "//src/agent:resolved_source_location",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "mock_eval_call_stack",
    testonly = 1,
    hdrs = ["mock_eval_call_stack.h"],
    deps = [
        "//src/agent:eval_call_stack",
        "@com_google_googletest//:gtest",
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
        "//:jdk_headers",
        "//src/agent:common",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "mock_method_caller",
    testonly = 1,
    hdrs = ["mock_method_caller.h"],
    deps = [
        "//src/agent:method_caller",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "mock_nanojava_internal_error_provider",
    testonly = 1,
    hdrs = ["mock_nanojava_internal_error_provider.h"],
    deps = [
        "//src/agent:nanojava_internal_error_builder",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "mock_object_evaluator",
    testonly = 1,
    hdrs = ["mock_object_evaluator.h"],
    deps = [
        "//src/agent:object_evaluator",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "mock_readers_factory",
    testonly = 1,
    hdrs = ["mock_readers_factory.h"],
    deps = [
        ":fake_instance_field_reader",
        ":fake_local_variable_reader",
        ":fake_static_field_reader",
        "//src/agent:array_reader",
        "//src/agent:common",
        "//src/agent:readers_factory",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "mock_user_id_provider",
    testonly = 1,
    hdrs = ["mock_user_id_provider.h"],
    deps = [
        "//src/agent:user_id_provider",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "mock_worker_provider",
    testonly = 1,
    hdrs = ["mock_worker_provider.h"],
    deps = [
        "//src/agent:model_json",
        "//src/agent:worker",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "fake_local_variable_reader",
    testonly = 1,
    hdrs = ["fake_local_variable_reader.h"],
    deps = [
        "//src/agent:jvariant",
        "//src/agent:local_variable_reader",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "fake_jni",
    testonly = 1,
    srcs = ["fake_jni.cc"],
    hdrs = ["fake_jni.h"],
    deps = [
        ":mock_jni_env",
        ":mock_jvmti_env",
        "//src/agent:class_metadata_reader",
        "//src/agent:common",
        "//src/agent:jvariant",
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
        "@com_google_googletest//:gtest",
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
        "@com_google_googletest//:gtest",
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
    ],
)

cc_library(
    name = "jasmin_utils",
    testonly = 1,
    srcs = ["jasmin_utils.cc"],
    hdrs = ["jasmin_utils.h"],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated",
    ],
    deps = [
        ":file_utils",
        "//src/agent:common",
        "//src/codegen:jni_test_proxies",
    ],
)

cc_library(
    name = "json_eq_matcher",
    testonly = 1,
    hdrs = ["json_eq_matcher.h"],
    deps = [
        "//src/agent:model_json",
        "@com_google_googletest//:gtest",
        "@jsoncpp",
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
