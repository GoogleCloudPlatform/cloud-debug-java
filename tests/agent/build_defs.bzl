def jvm_test(name, srcs, copts, cc_deps, java_deps):
    native.cc_binary(
        name = "lib%s.so" % name,
        testonly = 1,
        srcs = srcs,
        copts = copts,
        linkshared = 1,
        linkstatic = 1,
        deps = [
            "//tests/agent:jvm_test_base",
        ] + cc_deps,
    )

    native.genrule(
        name = name + "_main",
        outs = [name + ".java"],
        cmd = """
      echo "final class JvmTestMain {" > $@
      echo "  static {" >> $@
      echo "    System.loadLibrary(\\"%s\\");" >> $@
      echo "  }" >> $@
      echo "" >> $@
      echo "  static native void run();" >> $@
      echo "}" >> $@
      echo "" >> $@
      echo "public final class %s {" >> $@
      echo "  public static void main(String[] args) {" >> $@
      echo "    JvmTestMain.run();" >> $@
      echo "  }" >> $@
      echo "}" >> $@
    """ % (name, name),
    )

    native.java_test(
        name = name,
        testonly = 1,
        srcs = [name + ".java"],
        main_class = name,
        resources = [
          ":lib%s.so" % name,
        ],
        deps = [
          ":lib%s.so" % name,
        ],
        use_testrunner = 0,
        runtime_deps = java_deps,
    )
