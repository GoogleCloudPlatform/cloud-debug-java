load("@rules_java//java:defs.bzl", "java_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

package(default_visibility = ["//visibility:public"])

java_proto_library(
    name = "data_java_proto",
    deps = [":data_proto"],
)

proto_library(
    name = "data_proto",
    srcs = ["data.proto"],
    deps = [
      # Well known protos should be included as deps in the
      # proto_library rules of the source files importing them.
      # A list of all @com_google_protobuf well known protos can
      # seen with:
      # `bazel query 'kind(proto_library, @com_google_protobuf//:all)'`
      "@com_google_protobuf//:source_context_proto",
    ],
)
