package(default_visibility = ["//visibility:public"])

java_library(
    name = "classloader",
    srcs = ["InternalsClassLoader.java"],
)

# Extract InternalsClassLoader.class from the :classloader .jar file. The .class
# file is actually used by the debuglet. The debuglet needs ".class" file
# rather than ".jar" file since that's the only thing JNI supports directly.
genrule(
    name = "classloader_extract",
    srcs = [":classloader"],
    outs = ["cdbg_java_agent_internals_loader.class"],
    cmd = "$(location @local_jdk//:jar) xf $(SRCS) com/google/devtools/cdbg/debuglets/java/InternalsClassLoader.class && " +
          "mv com/google/devtools/cdbg/debuglets/java/InternalsClassLoader.class $@ &&" +
          "rm -r com",
    tools = ["@local_jdk//:jar"],
)
