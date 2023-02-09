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
    name = "agent_thread",
    hdrs = ["agent_thread.h"],
    deps = [
        ":common",
    ],
)

cc_library(
    name = "jvmti_agent_thread",
    srcs = ["jvmti_agent_thread.cc"],
    hdrs = ["jvmti_agent_thread.h"],
    copts = ["-Isrc/agent", "-I$(BINDIR)/src/codegen/generated"],
    deps = [
        ":agent_thread",
        ":common",
        ":jni_utils",
        ":jni_utils_h",
        "//src/codegen:jni_proxies",
    ],
)

cc_library(
    name = "semaphore",
    hdrs = ["semaphore.h"],
    deps = [
        ":common",
    ],
)

cc_library(
    name = "jni_semaphore",
    srcs = ["jni_semaphore.cc"],
    hdrs = ["jni_semaphore.h"],
    deps = [
        ":jni_utils",
        ":jni_utils_h",
        ":semaphore",
    ],
)

cc_library(
    name = "auto_reset_event",
    hdrs = ["auto_reset_event.h"],
    deps = [
        ":common",
        ":semaphore",
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

cc_library(
    name = "resolved_source_location",
    hdrs = ["resolved_source_location.h"],
    deps = [
        ":model",
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
    name = "jvm_object_array_reader",
    hdrs = ["jvm_object_array_reader.h"],
    deps = [
        ":array_reader",
        ":common",
        ":jni_utils",
        ":jni_utils_h",
        ":jvariant",
        ":messages",
        ":method_call_result",
        ":model",
        ":model_util",
    ],
)

cc_library(
    name = "jvm_primitive_array_reader",
    hdrs = ["jvm_primitive_array_reader.h"],
    deps = [
        ":array_reader",
        ":common",
        ":jni_utils",
        ":jni_utils_h",
        ":jvariant",
        ":messages",
        ":method_call_result",
        ":model",
        ":model_util",
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
    name = "readers_factory",
    hdrs = ["readers_factory.h"],
    deps = [
        ":class_metadata_reader",
        ":common",
        ":method_caller",
    ],
)

cc_library(
    name = "jvm_readers_factory",
    srcs = ["jvm_readers_factory.cc"],
    hdrs = ["jvm_readers_factory.h"],
    deps = [
        ":class_indexer",
        ":class_metadata_reader",
        ":class_path_lookup",
        ":common",
        ":instance_field_reader",
        ":jni_utils",
        ":jni_utils_h",
        ":jvm_evaluators",
        ":jvm_object_array_reader",
        ":jvm_primitive_array_reader",
        ":jvmti_buffer",
        ":local_variable_reader",
        ":messages",
        ":method_locals",
        ":model",
        ":readers_factory",
        ":safe_method_caller",
        ":static_field_reader",
        ":type_util",
    ],
)

cc_library(
    name = "jvm_class_metadata_reader",
    srcs = ["jvm_class_metadata_reader.cc"],
    hdrs = ["jvm_class_metadata_reader.h"],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated",
    ],
    deps = [
        ":class_metadata_reader",
        ":common",
        ":data_visibility_policy",
        ":jni_utils",
        ":jni_utils_h",
        ":jobject_map",
        ":jvariant",
        ":jvm_instance_field_reader",
        ":jvm_static_field_reader",
        ":jvmti_buffer",
        ":mutex",
        "//src/codegen:jni_proxies",
    ],
)

cc_library(
    name = "jvm_instance_field_reader",
    srcs = ["jvm_instance_field_reader.cc"],
    hdrs = ["jvm_instance_field_reader.h"],
    deps = [
        ":common",
        ":instance_field_reader",
        ":jvariant",
        ":messages",
    ],
)

cc_library(
    name = "jvm_static_field_reader",
    srcs = ["jvm_static_field_reader.cc"],
    hdrs = ["jvm_static_field_reader.h"],
    deps = [
        ":common",
        ":jvariant",
        ":messages",
        ":static_field_reader",
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

cc_library(
    name = "format_queue",
    srcs = ["format_queue.cc"],
    hdrs = ["format_queue.h"],
    deps = [
        ":capture_data_collector",
        ":common",
        ":model",
        ":mutex",
        ":observable",
        ":statistician",
    ],
)

cc_library(
    name = "transmit_queue",
    hdrs = ["transmit_queue.h"],
    deps = [":common"],
)

cc_library(
    name = "class_path_lookup",
    hdrs = ["class_path_lookup.h"],
    deps = [
        ":common",
        ":jni_utils",
        ":jni_utils_h",
        ":jvariant",
    ],
)

cc_library(
    name = "statistician",
    srcs = ["statistician.cc"],
    hdrs = ["statistician.h"],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated"
    ],
    deps = [
        ":common",
        ":mutex",
        ":stopwatch",
        "//src/codegen:jni_proxies",
    ],
    alwayslink = 1,
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
    name = "jsoncpp_util",
    srcs = ["jsoncpp_util.cc"],
    hdrs = ["jsoncpp_util.h"],
    deps = [
        ":common",
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
    name = "type_evaluator",
    hdrs = ["type_evaluator.h"],
    deps = [
        ":class_metadata_reader",
        ":common",
        ":method_caller",
    ],
)

cc_library(
    name = "generic_type_evaluator",
    srcs = ["generic_type_evaluator.cc"],
    hdrs = ["generic_type_evaluator.h"],
    deps = [
        ":common",
        ":instance_field_reader",
        ":jvariant",
        ":messages",
        ":model",
        ":type_evaluator",
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

cc_library(
    name = "object_evaluator",
    hdrs = ["object_evaluator.h"],
    deps = [
        ":common",
    ],
)

cc_library(
    name = "jvm_object_evaluator",
    srcs = ["jvm_object_evaluator.cc"],
    hdrs = ["jvm_object_evaluator.h"],
    deps = [
        ":array_type_evaluator",
        ":class_indexer",
        ":class_metadata_reader",
        ":common",
        ":config",
        ":generic_type_evaluator",
        ":iterable_type_evaluator",
        ":jvariant",
        ":map_entry_type_evaluator",
        ":map_type_evaluator",
        ":messages",
        ":model",
        ":object_evaluator",
        ":safe_method_caller",
        ":stringable_type_evaluator",
        ":type_evaluator",
        ":value_formatter",
    ],
)

cc_library(
    name = "iterable_type_evaluator",
    srcs = ["iterable_type_evaluator.cc"],
    hdrs = ["iterable_type_evaluator.h"],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated"
    ],
    deps = [
        ":common",
        ":jni_utils",
        ":messages",
        ":model",
        ":model_util",
        ":type_evaluator",
        "//src/codegen:jni_proxies",
    ],
)

cc_library(
    name = "map_entry_type_evaluator",
    srcs = ["map_entry_type_evaluator.cc"],
    hdrs = ["map_entry_type_evaluator.h"],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated"
    ],
    deps = [
        ":common",
        ":jni_utils",
        ":jvmti_buffer",
        ":messages",
        ":model",
        ":model_util",
        ":type_evaluator",
        "//src/codegen:jni_proxies",
    ],
)

cc_library(
    name = "map_type_evaluator",
    srcs = ["map_type_evaluator.cc"],
    hdrs = ["map_type_evaluator.h"],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated"
    ],
    deps = [
        ":common",
        ":iterable_type_evaluator",
        ":jni_utils",
        ":map_entry_type_evaluator",
        ":messages",
        ":model",
        ":model_util",
        ":type_evaluator",
        ":value_formatter",
        "//src/codegen:jni_proxies",
    ],
)

cc_library(
    name = "stringable_type_evaluator",
    srcs = ["stringable_type_evaluator.cc"],
    hdrs = ["stringable_type_evaluator.h"],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated"
    ],
    deps = [
        ":type_evaluator",
        "//src/codegen:jni_proxies",
    ],
)

cc_library(
    name = "byte_source",
    hdrs = ["byte_source.h"],
    deps = [
        ":common",
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

cc_library(
    name = "jni_flags",
    srcs = ["jni_flags.cc"],
    deps = [
        ":common",
        ":jni_utils",
        ":jni_utils_h",
    ],
    alwayslink = 1,
)

cc_library(
    name = "jni_logger",
    srcs = ["jni_logger.cc"],
    deps = [
        ":common",
        ":jni_utils",
        ":jni_utils_h",
    ],
    alwayslink = 1,
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

cc_library(
    name = "rate_limit",
    srcs = ["rate_limit.cc"],
    hdrs = ["rate_limit.h"],
    deps = [
        ":common",
        ":leaky_bucket",
        ":mutex",
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

cc_library(
    name = "data_visibility_policy",
    hdrs = ["data_visibility_policy.h"],
    deps = [
        ":common",
    ],
)

cc_library(
    name = "glob_data_visibility_policy",
    srcs = ["glob_data_visibility_policy.cc"],
    hdrs = ["glob_data_visibility_policy.h"],
    deps = [
        ":data_visibility_policy",
        ":type_util",
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

cc_library(
    name = "yaml_data_visibility_config_reader",
    srcs = ["yaml_data_visibility_config_reader.cc"],
    hdrs = ["yaml_data_visibility_config_reader.h"],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated"
    ],
    deps = [
        ":class_path_lookup",
        ":common",
        ":debuggee_labels",
        ":glob_data_visibility_policy",
        ":jni_utils",
        ":jni_utils_h",
        "//src/codegen:jni_proxies",
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
    name = "structured_data_visibility_policy",
    srcs = ["structured_data_visibility_policy.cc"],
    hdrs = ["structured_data_visibility_policy.h"],
    deps = [
        ":common",
        ":data_visibility_policy",
        ":jni_utils",
        ":jni_utils_h",
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

cc_library(
    name = "breakpoint_labels_provider",
    hdrs = ["breakpoint_labels_provider.h"],
    deps = [
        ":common",
    ],
)

cc_library(
    name = "jni_breakpoint_labels_provider",
    srcs = ["jni_breakpoint_labels_provider.cc"],
    hdrs = ["jni_breakpoint_labels_provider.h"],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated"
    ],
    deps = [
        ":breakpoint_labels_provider",
        ":common",
        ":jni_utils",
        ":jni_utils_h",
        "//src/codegen:jni_proxies",
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
    name = "bridge",
    hdrs = ["bridge.h"],
    deps = [
        ":common",
        ":debuggee_labels",
        ":model",
    ],
)

cc_library(
    name = "jni_bridge",
    srcs = ["jni_bridge.cc"],
    hdrs = ["jni_bridge.h"],
    copts = [
        "-DGCP_HUB_CLIENT",
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated"
    ],
    deps = [
        ":bridge",
        ":common",
        ":debuggee_labels",
        ":jni_utils",
        ":jni_utils_h",
        ":mutex",
        ":nullable",
        ":transmit_queue",
        "//src/codegen:jni_proxies",
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
    name = "jvm_breakpoint",
    srcs = ["jvm_breakpoint.cc"],
    hdrs = ["jvm_breakpoint.h"],
    deps = [
        ":auto_jvmti_breakpoint",
        ":breakpoint",
        ":breakpoints_manager",
        ":capture_data_collector",
        ":class_indexer",
        ":class_path_lookup",
        ":common",
        ":dynamic_logger",
        ":expression_evaluator_h",
        ":expression_util_h",
        ":format_queue",
        ":java_expression_evaluator",
        ":jni_utils",
        ":jni_utils_h",
        ":jvm_evaluators",
        ":jvm_readers_factory",
        ":jvmti_buffer",
        ":leaky_bucket",
        ":log_data_collector",
        ":messages",
        ":model",
        ":model_util",
        ":mutex",
        ":rate_limit",
        ":resolved_source_location",
        ":scheduler",
        ":statistician",
        ":stopwatch",
        ":type_util",
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
    name = "jvm_breakpoints_manager",
    srcs = ["jvm_breakpoints_manager.cc"],
    hdrs = ["jvm_breakpoints_manager.h"],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated",
    ],
    deps = [
        ":breakpoint",
        ":breakpoints_manager",
        ":callbacks_monitor",
        ":canary_control",
        ":class_indexer",
        ":common",
        ":format_queue",
        ":jvm_evaluators",
        ":leaky_bucket",
        ":mutex",
        ":rate_limit",
        ":statistician",
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

cc_library(
    name = "worker",
    srcs = ["worker.cc"],
    hdrs = ["worker.h"],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated",
    ],
    deps = [
        ":agent_thread",
        ":auto_reset_event",
        ":bridge",
        ":callbacks_monitor",
        ":canary_control",
        ":common",
        ":debuggee_labels",
        ":format_queue",
        ":model",
        ":stopwatch",
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
        "antlrgen/JavaExpressionCompiler.cpp",
        "antlrgen/JavaExpressionCompiler.hpp",
        "antlrgen/JavaExpressionCompilerTokenTypes.hpp",
        "antlrgen/JavaExpressionLexer.cpp",
        "antlrgen/JavaExpressionLexer.hpp",
        "antlrgen/JavaExpressionLexerTokenTypes.hpp",
        "antlrgen/JavaExpressionLexerTokenTypes.txt",
        "antlrgen/JavaExpressionParser.cpp",
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

cc_library(
    name = "jobject_map",
    hdrs = ["jobject_map.h"],
    deps = [
        ":common",
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

cc_library(
    name = "dynamic_logger",
    hdrs = ["dynamic_logger.h"],
    deps = [
        ":common",
        ":model",
    ],
)

cc_library(
    name = "jvm_dynamic_logger",
    srcs = ["jvm_dynamic_logger.cc"],
    hdrs = ["jvm_dynamic_logger.h"],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated"
    ],
    deps = [
        ":common",
        ":dynamic_logger",
        ":jni_utils",
        ":jni_utils_h",
        ":resolved_source_location",
        ":type_util",
        "//src/codegen:jni_proxies",
    ],
)

cc_library(
    name = "log_data_collector",
    srcs = ["log_data_collector.cc"],
    hdrs = ["log_data_collector.h"],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated",
    ],
    deps = [
        ":class_indexer",
        ":common",
        ":expression_evaluator_h",
        ":expression_util_h",
        ":messages",
        ":method_caller",
        ":model",
        ":object_evaluator",
        ":readers_factory",
        ":type_util",
        ":value_formatter",
        "//src/codegen:jni_proxies",
    ],
)

cc_library(
    name = "jvm_internals",
    srcs = ["jvm_internals.cc"],
    hdrs = [
        "jvm_internals.h",
    ],
    textual_hdrs = [
        "//src/codegen:internals_class_loader_static_defs.inl",
    ],
    copts = ["-I$(BINDIR)/src/codegen"],
    deps = [
        ":class_path_lookup",
        ":common",
        ":jni_utils",
        ":jni_utils_h",
        ":jvariant",
        ":model",
        ":model_util",
        ":resolved_source_location",
        ":stopwatch",
    ],
)

cc_library(
    name = "debugger",
    srcs = ["debugger.cc"],
    hdrs = ["debugger.h"],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated"
    ],
    deps = [
        ":breakpoint_labels_provider",
        ":canary_control",
        ":class_files_cache",
        ":class_metadata_reader",
        ":common",
        ":dynamic_logger",
        ":eval_call_stack",
        ":jni_utils_h",
        ":jvm_breakpoint",
        ":jvm_breakpoints_manager",
        ":jvm_class_indexer",
        ":jvm_evaluators",
        ":jvm_object_evaluator",
        ":method_locals",
        ":model",
        ":safe_method_caller",
        ":scheduler",
        ":statistician",
        ":stopwatch",
        ":user_id_provider",
    ],
)

cc_library(
    name = "jvmti_agent",
    srcs = ["jvmti_agent.cc"],
    hdrs = ["jvmti_agent.h"],
    copts = [
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated"
    ],
    deps = [
        ":auto_reset_event",
        ":bridge",
        ":callbacks_monitor",
        ":common",
        ":config",
        ":config_builder",
        ":data_visibility_policy",
        ":debuggee_labels",
        ":debugger",
        ":eval_call_stack",
        ":jni_breakpoint_labels_provider",
        ":jni_semaphore",
        ":jni_user_id_provider",
        ":jvm_class_metadata_reader",
        ":jvm_dynamic_logger",
        ":jvm_eval_call_stack",
        ":jvm_internals",
        ":jvmti_agent_thread",
        ":jvmti_buffer",
        ":method_locals",
        ":scheduler",
        ":stopwatch",
        ":user_id_provider",
        ":worker",
        "//src/codegen:jni_proxies",
    ],
)

cc_library(
    name = "jvmti_globals",
    srcs = ["jvmti_globals.cc"],
    copts = [
        "-DGCP_HUB_CLIENT",
        "-DSTANDALONE_BUILD",
        "-Isrc/agent",
        "-I$(BINDIR)/src/codegen/generated"
    ],
    deps = [
        ":callbacks_monitor",
        ":common",
        ":data_visibility_scrubbed_eval_call_stack",
        ":debuggee_labels",
        ":glob_data_visibility_policy",
        ":jni_bridge",
        ":jni_flags",
        ":jni_logger",
        ":jvm_env",
        ":jvm_eval_call_stack",
        ":jvm_internals",
        ":jvmti_agent",
        ":jvmti_buffer",
        ":model_json",
        ":statistician",
        ":yaml_data_visibility_config_reader",
        "//src/codegen:jni_proxies",
    ],
    alwayslink = 1,
)

# This rule is here so the cdbg_java_agent_internals.jar file will be located
# in the same directory as cdbg_java_agent.so for Java unit tests that load the
# agent. This is a requirement of the .so file, it requires that the
# cdbg_java_agent_internals.jar file be in the same directory as itself.
genrule(
    name = "cdbg_java_agent_internals_jar_rename",
    srcs = ["//src/agent/internals/src/main/java/com/google/devtools/cdbg/debuglets/java:cdbg_java_agent_internals_deploy.jar"],
    outs = ["cdbg_java_agent_internals.jar"],
    cmd = "cp $< $@",
)

cc_binary(
    name = "cdbg_java_agent.so",
    linkopts = [
        "-Xlinker",
        "--version-script",
        "-Xlinker",
        "$(location cdbg_java_agent.lds)",
        "-Bsymbolic",
    ],
    linkshared = 1,
    linkstatic = 1,
    deps = [
        "cdbg_java_agent.lds",
        ":java_expression_evaluator",
        ":jvmti_agent",
        ":jvmti_globals",
    ],
)
