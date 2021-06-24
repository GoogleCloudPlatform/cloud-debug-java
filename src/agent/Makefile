# Operating Systems: Linux or Darwin
# Darwin build is not officially supported, it is maintained at best effort.
# This Makefile assumes Linux build uses GCC and Darwin build uses clang.
OS = $(shell uname)

ifneq ($(OS),Darwin)
# Pass this flag only when building with gcc (does not exist on clang).
ANTLR_DISABLED_WARNINGS = -Wno-unused-but-set-variable
endif

# Ignore warning produced by glog
GLOG_DISABLED_WARNINGS = -Wno-strict-aliasing

CC = g++

CFLAGS += \
	-m64 \
	-std=c++11 \
	-fPIC \
	-Werror \
	-Wall \
	-Wno-unused-parameter \
	-Wno-deprecated \
	-Wno-ignored-qualifiers \
	-Wno-sign-compare \
	-Wno-array-bounds \
	-g0 \
	-DSTANDALONE_BUILD \
	-DGCP_HUB_CLIENT \
	$(ANTLR_DISABLED_WARNINGS) \
	$(GLOG_DISABLED_WARNINGS) \

THIRD_PARTY_LIB_PATH ?= /usr/local/lib
THIRD_PARTY_INCLUDE_PATH ?= /usr/local/include
SYSTEM_LIB_PATH ?= /usr/lib/x86_64-linux-gnu
ASM_JAR_PATH ?= /usr/lib/asm-9.1/lib/asm-9.1.jar
ASM_COMMONS_JAR_PATH ?= /usr/lib/asm-9.1/lib/asm-commons-9.1.jar
ASM_UTIL_JAR_PATH ?= /usr/lib/asm-9.1/lib/asm-util-9.1.jar
GOOGLE_API_JAVA_CLIENT_PATH ?= /usr/lib/google-api-java-client
GOOGLE_AUTH_OAUTH2_JAR_PATH ?= /usr/lib/google-auth-library-java/oauth2-http.jar
GOOGLE_AUTH_CREDENTIALS_JAR_PATH ?= /usr/lib/google-auth-library-java/credentials.jar
JAVA_BUILD ?= maven
JAVAC=javac -source 1.7 -target 1.7
BUILD_TARGET_PATH ?= .

OPT_FLAGS = -O3 -D NDEBUG
LDFLAGS += -shared

ifneq ($(OS),Darwin)
# Pass these linker flags only when building with gcc (i.e., not with clang).
LDS_FLAGS = -Wl,-z,defs -Wl,--version-script=cdbg_java_agent.lds
endif

ROOT=../../
THIRD_PARTY_PATH = ../../third_party
ANTLR_TOOL_PATH = $(THIRD_PARTY_PATH)/antlr
ANTLR_CPP_LIB = $(THIRD_PARTY_PATH)/antlr/lib/cpp
ANTLR_CPP_LIB_SRC = $(ANTLR_CPP_LIB)/src
ANTLR_CPP_LIB_INCLUDE = $(ANTLR_CPP_LIB)

INCLUDES = \
	-I$(ROOT) \
	-I$(THIRD_PARTY_INCLUDE_PATH) \
	-I$(ANTLR_CPP_LIB_INCLUDE) \
	-I. \
	-I../codegen \
	-Iantlrgen \

ifeq ($(OS),Darwin)
JDK ?= $(shell /usr/libexec/java_home)
INCLUDES += \
	-I$(JDK)/include \
	-I$(JDK)/include/darwin
else
# Find the java command. Strip the trailing '/bin/java'. Then, strip an
# extra trailing '/jre' (if exists).
JDK ?= $(shell readlink -f $(shell which java) | \
			 sed -e 's!\(.*\)/bin/java$$!\1!' | \
			 sed -e 's!\(.*\)/jre$$!\1!')
INCLUDES += \
	-I$(JDK)/include \
	-I$(JDK)/include/linux
endif

TARGET_AGENT = cdbg_java_agent.so
INTERNALS_CLASS_LOADER = cdbg_java_agent_internals_loader.class
INTERNALS_CLASS_LOADER_STATIC_DEFS = internals_class_loader_static_defs.inl
INTERNALS_JAR = internals.jar
SERVICE_ACCOUNT_AUTH_TOOL = $(BUILD_TARGET_PATH)/cdbg_service_account_auth.jar
TARGET_TAR_GZ_APPENGINE = $(BUILD_TARGET_PATH)/cdbg_java_agent.tar.gz
TARGET_TAR_GZ_GCE = $(BUILD_TARGET_PATH)/cdbg_java_agent_gce.tar.gz
TARGET_TAR_GZ_SERVICE_ACCOUNT = $(BUILD_TARGET_PATH)/cdbg_java_agent_service_account.tar.gz
TARGET_VERSION_TXT = $(BUILD_TARGET_PATH)/version.txt
APPENGINE_FORMAT_SCRIPT = format-env-appengine-vm.sh

ANTLR_RUNTIME_SOURCES = \
	$(ANTLR_CPP_LIB_SRC)/ANTLRException.cpp \
	$(ANTLR_CPP_LIB_SRC)/ANTLRUtil.cpp \
	$(ANTLR_CPP_LIB_SRC)/ASTFactory.cpp \
	$(ANTLR_CPP_LIB_SRC)/ASTNULLType.cpp \
	$(ANTLR_CPP_LIB_SRC)/ASTRefCount.cpp \
	$(ANTLR_CPP_LIB_SRC)/BaseAST.cpp \
	$(ANTLR_CPP_LIB_SRC)/BitSet.cpp \
	$(ANTLR_CPP_LIB_SRC)/CharBuffer.cpp \
	$(ANTLR_CPP_LIB_SRC)/CharScanner.cpp \
	$(ANTLR_CPP_LIB_SRC)/CharStreamException.cpp \
	$(ANTLR_CPP_LIB_SRC)/CharStreamIOException.cpp \
	$(ANTLR_CPP_LIB_SRC)/CommonAST.cpp \
	$(ANTLR_CPP_LIB_SRC)/CommonASTWithHiddenTokens.cpp \
	$(ANTLR_CPP_LIB_SRC)/CommonHiddenStreamToken.cpp \
	$(ANTLR_CPP_LIB_SRC)/CommonToken.cpp \
	$(ANTLR_CPP_LIB_SRC)/ExceptionSlot.cpp \
	$(ANTLR_CPP_LIB_SRC)/IOException.cpp \
	$(ANTLR_CPP_LIB_SRC)/InputBuffer.cpp \
	$(ANTLR_CPP_LIB_SRC)/LLkParser.cpp \
	$(ANTLR_CPP_LIB_SRC)/MismatchedCharException.cpp \
	$(ANTLR_CPP_LIB_SRC)/MismatchedTokenException.cpp \
	$(ANTLR_CPP_LIB_SRC)/NoViableAltException.cpp \
	$(ANTLR_CPP_LIB_SRC)/NoViableAltForCharException.cpp \
	$(ANTLR_CPP_LIB_SRC)/Parser.cpp \
	$(ANTLR_CPP_LIB_SRC)/RecognitionException.cpp \
	$(ANTLR_CPP_LIB_SRC)/SemanticException.cpp \
	$(ANTLR_CPP_LIB_SRC)/String.cpp \
	$(ANTLR_CPP_LIB_SRC)/Token.cpp \
	$(ANTLR_CPP_LIB_SRC)/TokenBuffer.cpp \
	$(ANTLR_CPP_LIB_SRC)/TokenStreamBasicFilter.cpp \
	$(ANTLR_CPP_LIB_SRC)/TokenStreamException.cpp \
	$(ANTLR_CPP_LIB_SRC)/TokenStreamHiddenTokenFilter.cpp \
	$(ANTLR_CPP_LIB_SRC)/TokenStreamIOException.cpp \
	$(ANTLR_CPP_LIB_SRC)/TokenStreamRecognitionException.cpp \
	$(ANTLR_CPP_LIB_SRC)/TokenStreamRetryException.cpp \
	$(ANTLR_CPP_LIB_SRC)/TokenStreamSelector.cpp \
	$(ANTLR_CPP_LIB_SRC)/TreeParser.cpp \

JAVA_EXPRESSION_GRAMMAR_FILE = \
	java_expression.g \

ANTLR_JAVA_EXPRESSION_GENERATED_SOURCES = \
	antlrgen/JavaExpressionCompiler.cc \
	antlrgen/JavaExpressionLexer.cc \
	antlrgen/JavaExpressionParser.cc \

ANTLR_JAVA_EXPRESSION_GENERATED_HEADERS = \
	antlrgen/JavaExpressionCompiler.hpp \
	antlrgen/JavaExpressionCompilerTokenTypes.hpp \
	antlrgen/JavaExpressionLexer.hpp \
	antlrgen/JavaExpressionLexerTokenTypes.hpp \
	antlrgen/JavaExpressionParser.hpp \
	antlrgen/JavaExpressionParserTokenTypes.hpp \

JNI_PROXIES_GENERATED_SOURCES = \
	../codegen/jni_proxy_*.cc \

JNI_PROXIES_GENERATED_HEADERS = \
	../codegen/jni_proxy_*.h \

JNI_PROXIES_GENERATED_OBJECTS = \
	jni_proxy_*.o \

SOURCES := \
	$(wildcard *.cc) \
	$(ANTLR_JAVA_EXPRESSION_GENERATED_SOURCES) \
	$(ANTLR_RUNTIME_SOURCES) \

OBJECTS=$(patsubst %.cc,%.o,$(patsubst %.cpp,%.o,$(SOURCES)))

HEADERS := \
	$(wildcard *.h) \

LIBS= \
	-ldl \
	$(THIRD_PARTY_LIB_PATH)/libjsoncpp.a \
	$(THIRD_PARTY_LIB_PATH)/libgflags.a \
	$(THIRD_PARTY_LIB_PATH)/libglog.a \

ifneq ($(OS),Darwin)
# Pass these flags only when building with gcc (i.e., not clang).
LIBS += \
	-pthread \
	-lrt
endif

GOOGLE_API_CLIENT_JARS = \
	$(GOOGLE_API_JAVA_CLIENT_PATH)/commons-logging-1.1.1.jar \
	$(GOOGLE_API_JAVA_CLIENT_PATH)/google-api-client-1.19.0.jar \
	$(GOOGLE_API_JAVA_CLIENT_PATH)/google-http-client-1.19.0.jar \
	$(GOOGLE_API_JAVA_CLIENT_PATH)/google-http-client-gson-1.19.0.jar \
	$(GOOGLE_API_JAVA_CLIENT_PATH)/google-http-client-jackson2-1.19.0.jar \
	$(GOOGLE_API_JAVA_CLIENT_PATH)/jackson-core-2.1.3.jar \

EMPTY :=
SPACE := $(empty) $(empty)
GOOGLE_API_CLIENT_JARS_CLASS_PATH := $(subst $(SPACE),:,$(GOOGLE_API_CLIENT_JARS))

all: \
	packages \
	service_account_auth_tool \
	target_version_txt \

antlrgen: $(JAVA_EXPRESSION_GRAMMAR_FILE)
	mkdir antlrgen
	java -classpath $(ANTLR_TOOL_PATH)/libantlr.jar antlr.Tool -o antlrgen $(JAVA_EXPRESSION_GRAMMAR_FILE)

$(ANTLR_JAVA_EXPRESSION_GENERATED_SOURCES) $(ANTLR_JAVA_EXPRESSION_GENERATED_HEADERS): antlrgen


jni_proxies_src: internals_jar
ifeq ($(JAVA_BUILD),maven)
	cd ../codegen && mvn clean install
	java -cp "../codegen/target/jni_proxy_code_gen_tool-SNAPSHOT-jar-with-dependencies.jar" devtools.cdbg.debuglets.java.codegen.JniProxyCodeGen ../codegen/config.json ../codegen
else
	mkdir -p ../codegen/target
	cd ../codegen ; $(JAVAC) -g -cp "$(ASM_JAR_PATH):$(GSON_JAR_PATH):$(FREE_MARKER_JAR_PATH):$(GUAVA_JAR_PATH):$(SNAKE_YAML_JAR_PATH)" -d target src/main/java/devtools/cdbg/debuglets/java/codegen/*.java
	cp ../codegen/src/main/java/devtools/cdbg/debuglets/java/codegen/*.tpl ../codegen/target/devtools/cdbg/debuglets/java/codegen
	java -cp "../codegen/target:$(ASM_JAR_PATH):$(GSON_JAR_PATH):$(FREE_MARKER_JAR_PATH):$(GUAVA_JAR_PATH):$(INTERNALS_JAR):$(SNAKE_YAML_JAR_PATH)" devtools.cdbg.debuglets.java.codegen.JniProxyCodeGen ../codegen/config.json ../codegen
endif

%.o: %.cpp antlrgen
	$(CC) $(INCLUDES) $(CFLAGS) $(OPT_FLAGS) -c $< -o $@

%.o: %.cc $(HEADERS) antlrgen jni_proxies_src internals_class_loader_static_defs
	$(CC) $(INCLUDES) $(CFLAGS) $(OPT_FLAGS) -c $< -o $@

jni_proxies_obj: jni_proxies_src
	$(CC) $(INCLUDES) $(CFLAGS) $(OPT_FLAGS) -c $(JNI_PROXIES_GENERATED_SOURCES)

target_agent: $(OBJECTS) jni_proxies_obj
	$(CC) $(INCLUDES) $(CFLAGS) $(OPT_FLAGS) $(LDFLAGS) $(OBJECTS) $(JNI_PROXIES_GENERATED_OBJECTS) $(LIBS) -o $(TARGET_AGENT) $(LDS_FLAGS)

internals_class_loader:
ifeq ($(JAVA_BUILD),maven)
	cd internals-class-loader ; mvn clean install
else
	mkdir -p internals-class-loader/target
	cd internals-class-loader ; $(JAVAC) -g:none -d target src/main/java/com/google/devtools/cdbg/*.java
endif
	cp internals-class-loader/target/com/google/devtools/cdbg/debuglets/java/InternalsClassLoader.class $(INTERNALS_CLASS_LOADER)

internals_class_loader_static_defs: internals_class_loader
	python ../codegen/static_def_gen.py $(INTERNALS_CLASS_LOADER_STATIC_DEFS) .:^$(INTERNALS_CLASS_LOADER)$

internals_jar:
ifeq ($(JAVA_BUILD),maven)
	cd internals ; mvn clean install
	cp internals/target/cdbg_java_agent_internals-SNAPSHOT-jar-with-dependencies.jar $(INTERNALS_JAR)
else
	mkdir -p internals/target
	cd internals ; $(JAVAC) -g:none -cp $(ASM_JAR_PATH):$(ASM_COMMONS_JAR_PATH):$(ASM_UTIL_JAR_PATH):$(GSON_JAR_PATH):$(GOOGLE_API_CLIENT_JARS_CLASS_PATH):$(GOOGLE_AUTH_OAUTH2_JAR_PATH):$(GOOGLE_AUTH_GSON_JAR_PATH):$(GOOGLE_AUTH_CREDENTIALS_JAR_PATH):$(SNAKE_YAML_JAR_PATH) -d target src/main/java/com/google/devtools/cdbg/debuglets/java/*.java
	cd internals/target ; jar xf $(ASM_JAR_PATH) ; rm -rf META-INF
	cd internals/target ; jar xf $(ASM_COMMONS_JAR_PATH) ; rm -rf META-INF
	cd internals/target ; jar xf $(ASM_UTIL_JAR_PATH) ; rm -rf META-INF
	cd internals/target ; jar xf $(GSON_JAR_PATH) ; rm -rf META-INF
	cd internals/target ; jar xf $(GUAVA_JAR_PATH) ; rm -rf META-INF
	cd internals/target ; jar xf $(SNAKE_YAML_JAR_PATH) ; rm -rf META-INF
	cd internals/target ; jar xf $(GOOGLE_AUTH_OAUTH2_JAR_PATH) ; rm -rf META-INF
	cd internals/target ; jar xf $(GOOGLE_AUTH_CREDENTIALS_JAR_PATH) ; rm -rf META-INF
	cd internals/target ; echo "$(GOOGLE_API_CLIENT_JARS)" | xargs -n 1 jar xf
	jar cvf $(INTERNALS_JAR) -C internals/target/ .
endif

# Separate tool to obtain OAuth access token given service account email
# and p12 file. This tool is used by GCE bootstrap script when service account
# authentication is enabled.
service_account_auth_tool: internals_jar
ifeq ($(JAVA_BUILD),maven)
	cd service-account-auth ; mvn clean install
	cp service-account-auth/target/cdbg_service_account_auth-SNAPSHOT-jar-with-dependencies.jar $(SERVICE_ACCOUNT_AUTH_TOOL)
else
	mkdir -p service-account-auth/target
	cd service-account-auth ; $(JAVAC) -g:none -cp ../$(INTERNALS_JAR) -d target src/main/java/com/google/devtools/cdbg/*.java
	cd service-account-auth/target ; jar xf ../../$(INTERNALS_JAR) ; rm -rf META-INF
	jar cvfe $(SERVICE_ACCOUNT_AUTH_TOOL) com.google.devtools.cdbg.debuglets.java.ServiceAccountAuthTool -C service-account-auth/target/ .
endif

target_version_txt: internals_jar
	java -cp $(INTERNALS_JAR) com.google.devtools.cdbg.debuglets.java.GcpDebugletVersion > $(TARGET_VERSION_TXT)

# Generate packages:
# 1. Java Cloud Debugger package used in Managed VM (aka AppEngine v2)
# 2. Java Cloud Debugger package used on GCE with metadata service
#    authentication or service account file authentication
#    (default and recommended solution)
# 3. Java Cloud Debugger package used on GCE with service account authentication
#    (Identical to #2, retained for legacy purposes).
packages: target_agent internals_jar $(APPENGINE_FORMAT_SCRIPT) target_version_txt
	cp $(INTERNALS_JAR) cdbg_java_agent_internals.jar
	cp $(TARGET_VERSION_TXT) version.txt
	tar cfz $(TARGET_TAR_GZ_APPENGINE) $(TARGET_AGENT) cdbg_java_agent_internals.jar $(APPENGINE_FORMAT_SCRIPT) version.txt
	tar cfz $(TARGET_TAR_GZ_GCE) $(TARGET_AGENT) cdbg_java_agent_internals.jar version.txt
	tar cfz $(TARGET_TAR_GZ_SERVICE_ACCOUNT) $(TARGET_AGENT) cdbg_java_agent_internals.jar version.txt
