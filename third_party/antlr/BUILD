package(default_visibility = ["//visibility:public"])

java_import(
  name = "libantlr",
  jars = [
    "libantlr.jar",
  ]
)

java_binary(
  name = "antlr_tool",
  main_class = "antlr.Tool",
  runtime_deps = [
    ":libantlr",
  ]
)
