load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_jar")

RULES_JVM_EXTERNAL_TAG = "4.2"
RULES_JVM_EXTERNAL_SHA = "cd1a77b7b02e8e008439ca76fd34f5b07aecb8c752961f9640dea15e9e5ba1ca"

http_archive(
    name = "rules_jvm_external",
    strip_prefix = "rules_jvm_external-%s" % RULES_JVM_EXTERNAL_TAG,
    sha256 = RULES_JVM_EXTERNAL_SHA,
    url = "https://github.com/bazelbuild/rules_jvm_external/archive/%s.zip" % RULES_JVM_EXTERNAL_TAG,
)

load("@rules_jvm_external//:repositories.bzl", "rules_jvm_external_deps")

rules_jvm_external_deps()

load("@rules_jvm_external//:setup.bzl", "rules_jvm_external_setup")

rules_jvm_external_setup()

load("@rules_jvm_external//:defs.bzl", "maven_install")

maven_install(
    artifacts = [
        "com.github.vbmacher:java-cup:11b-20160615",
        "com.google.api-client:google-api-client:1.35.2",
        "com.google.auth:google-auth-library-oauth2-http:0.26.0",
        "com.google.guava:guava:31.1-jre",
        "com.google.code.gson:gson:2.9.1",
        "com.google.firebase:firebase-admin:9.0.0",
        "com.google.truth:truth:1.1.3",
        "junit:junit:4.13.2",
        "org.freemarker:freemarker:2.3.22",
        "org.mockito:mockito-core:4.6.1",
        "org.ow2.asm:asm:9.1",
        "org.ow2.asm:asm-commons:9.1",
        "org.ow2.asm:asm-util:9.1",
        "org.yaml:snakeyaml:1.32",
    ],
    repositories = [
        "https://maven.google.com",
        "https://repo1.maven.org/maven2",
    ],
)

# https://github.com/bazelbuild/rules_python/releases/tag/0.17.3
http_archive(
    name = "rules_python",
    sha256 = "8c15896f6686beb5c631a4459a3aa8392daccaab805ea899c9d14215074b60ef",
    strip_prefix = "rules_python-0.17.3",
    url = "https://github.com/bazelbuild/rules_python/archive/refs/tags/0.17.3.tar.gz",
)

load("@rules_python//python:repositories.bzl", "py_repositories")

py_repositories()

# https://github.com/bazelbuild/rules_java/releases/tag/5.4.0
http_archive(
    name = "rules_java",
    url = "https://github.com/bazelbuild/rules_java/releases/download/5.4.0/rules_java-5.4.0.tar.gz",
    sha256 = "9b87757af5c77e9db5f7c000579309afae75cf6517da745de01ba0c6e4870951",
)
load("@rules_java//java:repositories.bzl", "rules_java_dependencies", "rules_java_toolchains")
rules_java_dependencies()
rules_java_toolchains()

# https://github.com/bazelbuild/rules_proto/releases/tag/5.3.0-21.7
http_archive(
    name = "rules_proto",
    sha256 = "dc3fb206a2cb3441b485eb1e423165b231235a1ea9b031b4433cf7bc1fa460dd",
    strip_prefix = "rules_proto-5.3.0-21.7",
    urls = [
        "https://github.com/bazelbuild/rules_proto/archive/refs/tags/5.3.0-21.7.tar.gz",
    ],
)
load("@rules_proto//proto:repositories.bzl", "rules_proto_dependencies", "rules_proto_toolchains")
rules_proto_dependencies()
rules_proto_toolchains()


http_archive(
    name = "com_github_gflags_gflags",
    sha256 = "34af2f15cf7367513b352bdcd2493ab14ce43692d2dcd9dfc499492966c64dcf",
    strip_prefix = "gflags-2.2.2",
    urls = ["https://github.com/gflags/gflags/archive/v2.2.2.tar.gz"],
)

http_archive(
    name = "com_github_google_glog",
    sha256 = "122fb6b712808ef43fbf80f75c52a21c9760683dae470154f02bddfc61135022",
    strip_prefix = "glog-0.6.0",
    urls = ["https://github.com/google/glog/archive/v0.6.0.zip"],
)

http_archive(
  name = "com_google_googletest",
  urls = ["https://github.com/google/googletest/archive/7107c441885900fedb8458a96bddca16e9768573.zip"],
  strip_prefix = "googletest-7107c441885900fedb8458a96bddca16e9768573",
)

http_archive(
    name = "com_googlesource_code_re2",
    urls = ["https://github.com/google/re2/archive/2022-06-01.zip"],
    strip_prefix = "re2-2022-06-01",
    sha256 = "9f3b65f2e0c78253fcfdfce1754172b0f97ffdb643ee5fd67f0185acf91a3f28",
)

http_archive(
    name = "jsoncpp",
    sha256 = "f409856e5920c18d0c2fb85276e24ee607d2a09b5e7d5f0a371368903c275da2",
    strip_prefix = "jsoncpp-1.9.5",
    urls = [
        "https://github.com/open-source-parsers/jsoncpp/archive/1.9.5.tar.gz",
    ],
)

http_jar(
    name = "jasmin",
    sha256 = "3eb4af9c439513be23d7c6aedfd31efc456c7bca7bc26f06f80f78f0afe10c03",
    urls = [
        "https://www.sable.mcgill.ca/software/jasminclasses-2.5.0.jar",
    ],
)
