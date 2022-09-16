cc_library(
  name = "jdk_headers",
  srcs = [
    "@bazel_tools//tools/jdk:current_java_runtime"
  ],
  includes = [
    "external/local_jdk/include",
    "external/local_jdk/include/linux",
  ],
  visibility = [
    "//visibility:public",
  ],
)
