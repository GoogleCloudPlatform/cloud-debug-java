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
        "com.github.vbmacher:java-cup:11b",
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
  urls = ["https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip"],
  strip_prefix = "googletest-609281088cfefc76f9d0ce82e1ff6c30cc3591e5",
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
    urls = [
        "https://www.sable.mcgill.ca/software/jasminclasses-2.5.0.jar",
    ],
)
