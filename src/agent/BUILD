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
    name = "mutex",
    hdrs = ["mutex.h"],
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
    name = "data_visibility_policy",
    hdrs = ["data_visibility_policy.h"],
    deps = [
        ":common",
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
