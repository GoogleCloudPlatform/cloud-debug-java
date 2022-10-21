package(default_visibility = ["//visibility:public"])

cc_library(
  name = "jdk_headers",
  srcs = [
    "@bazel_tools//tools/jdk:current_java_runtime"
  ],
  includes = [
    "external/local_jdk/include",
    "external/local_jdk/include/linux",
  ],
  linkstatic = 1,
  visibility = [
    "//visibility:public",
  ],
)

java_library(
  name = "jasmin",
  runtime_deps = [
    "@jasmin//jar",
    "@maven//:com_github_vbmacher_java_cup",
  ]
)
