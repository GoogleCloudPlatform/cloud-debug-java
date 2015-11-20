# Disable this warning for ANTLR source code. Bring this warning back
# once ANTLR runtime is compiled separately.
ANTLR_DISABLED_WARNINGS = -Wno-unused-but-set-variable

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
ASM_JAR_PATH ?= /usr/lib/asm-5.0.4/lib/asm-5.0.4.jar
ASM_COMMONS_JAR_PATH = /usr/lib/asm-5.0.4/lib/asm-commons-5.0.4.jar
ASM_UTIL_JAR_PATH ?= /usr/lib/asm-5.0.4/lib/asm-util-5.0.4.jar
GOOGLE_API_JAVA_CLIENT_PATH ?= /usr/lib/google-api-java-client
JARJAR_PATH ?= ../../build/jarjar.jar
JAVA_BUILD ?= maven
BUILD_TARGET_PATH ?= .

OPT_FLAGS = -O3 -D NDEBUG
LDFLAGS += -shared
LDS_FLAGS = -Wl,-z,defs -Wl,--version-script=cdbg_java_agent.lds

THIRD_PARTY_PATH = ../../third_party
ANTLR_TOOL_PATH = $(THIRD_PARTY_PATH)/antlr
ANTLR_CPP_LIB = $(THIRD_PARTY_PATH)/antlr/lib/cpp
ANTLR_CPP_LIB_SRC = $(ANTLR_CPP_LIB)/src/v2_7_2
ANTLR_CPP_LIB_INCLUDE = $(ANTLR_CPP_LIB)/v2_7_2/

INCLUDES = \
	-I/usr/lib/jvm/java-7-openjdk-amd64/include \
	-I$(THIRD_PARTY_INCLUDE_PATH) \
	-I$(ANTLR_CPP_LIB_INCLUDE) \

TARGET_AGENT = cdbg_java_agent.so
INTERNALS_CLASS_LOADER = cdbg_java_agent_internals_loader.class
INTERNALS_JAR_FULL = internals_full.jar
INTERNALS_JAR_STRIPPED = internals_stripped.jar
SERVICE_ACCOUNT_AUTH_TOOL = $(BUILD_TARGET_PATH)/cdbg_service_account_auth.jar
TARGET_TAR_GZ_APPENGINE = $(BUILD_TARGET_PATH)/cdbg_java_agent.tar.gz
TARGET_TAR_GZ_GCE = $(BUILD_TARGET_PATH)/cdbg_java_agent_gce.tar.gz
TARGET_TAR_GZ_SERVICE_ACCOUNT = $(BUILD_TARGET_PATH)/cdbg_java_agent_service_account.tar.gz
TARGET_VERSION_TXT = $(BUILD_TARGET_PATH)/version.txt
CDBG_CONFIG_XML = cdbg_config.xml
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
	JavaExpressionCompiler.cc \
	JavaExpressionLexer.cc \
	JavaExpressionParser.cc \

ANTLR_JAVA_EXPRESSION_GENERATED_HEADERS = \
	JavaExpressionCompiler.hpp \
	JavaExpressionCompilerTokenTypes.hpp \
	JavaExpressionLexer.hpp \
	JavaExpressionLexerTokenTypes.hpp \
	JavaExpressionParser.hpp \
	JavaExpressionParserTokenTypes.hpp \

JNI_PROXIES_GENERATED_SOURCES = \
	jni_proxy_*.cc \

JNI_PROXIES_GENERATED_HEADERS = \
	jni_proxy_*.h \

SOURCES = \
	*.cc \
	$(ANTLR_RUNTIME_SOURCES) \

HEADERS = \
	*.h \

LIBS= \
	-lrt \
	-pthread \
	$(THIRD_PARTY_LIB_PATH)/libjsoncpp.a \
	$(THIRD_PARTY_LIB_PATH)/libgflags.a \
	$(THIRD_PARTY_LIB_PATH)/libglog.a \

GOOGLE_API_CLIENT_JARS = \
	$(GOOGLE_API_JAVA_CLIENT_PATH)/commons-logging-1.1.1.jar \
	$(GOOGLE_API_JAVA_CLIENT_PATH)/google-api-client-1.19.0.jar \
	$(GOOGLE_API_JAVA_CLIENT_PATH)/google-http-client-1.19.0.jar \
	$(GOOGLE_API_JAVA_CLIENT_PATH)/google-http-client-jackson2-1.19.0.jar \
	$(GOOGLE_API_JAVA_CLIENT_PATH)/google-oauth-client-1.19.0.jar \
	$(GOOGLE_API_JAVA_CLIENT_PATH)/jackson-core-2.1.3.jar \

EMPTY :=
SPACE := $(empty) $(empty)
GOOGLE_API_CLIENT_JARS_CLASS_PATH := $(subst $(SPACE),:,$(GOOGLE_API_CLIENT_JARS))

all: \
	$(TARGET_TAR_GZ_APPENGINE) \
	$(TARGET_TAR_GZ_GCE) \
	$(TARGET_TAR_GZ_SERVICE_ACCOUNT) \
	$(SERVICE_ACCOUNT_AUTH_TOOL) \
	$(TARGET_VERSION_TXT) \

$(ANTLR_JAVA_EXPRESSION_GENERATED_SOURCES) $(ANTLR_JAVA_EXPRESSION_GENERATED_HEADERS): $(JAVA_EXPRESSION_GRAMMAR_FILE)
	java -classpath $(ANTLR_TOOL_PATH)/libantlr.jar antlr.Tool $(JAVA_EXPRESSION_GRAMMAR_FILE)

$(JNI_PROXIES_GENERATED_SOURCES) $(JNI_PROXIES_GENERATED_HEADERS): $(INTERNALS_JAR_FULL)
ifeq ($(JAVA_BUILD),maven)
	cd ../codegen && mvn clean install
	java -cp "../codegen/target/jni_proxy_code_gen_tool-SNAPSHOT-jar-with-dependencies.jar" devtools.cdbg.debuglets.java.codegen.JniProxyCodeGen ../codegen/config.json .
else
	mkdir -p ../codegen/target
	cd ../codegen ; javac -g -cp "$(ASM_JAR_PATH):$(GSON_JAR_PATH):$(FREE_MARKER_JAR_PATH):$(GUAVA_JAR_PATH)" -d target src/main/java/devtools/cdbg/debuglets/java/codegen/*.java
	cp ../codegen/src/main/java/devtools/cdbg/debuglets/java/codegen/*.tpl ../codegen/target/devtools/cdbg/debuglets/java/codegen
	java -cp "../codegen/target:$(ASM_JAR_PATH):$(GSON_JAR_PATH):$(FREE_MARKER_JAR_PATH):$(GUAVA_JAR_PATH):$(INTERNALS_JAR_FULL)" devtools.cdbg.debuglets.java.codegen.JniProxyCodeGen ../codegen/config.json .
endif

$(TARGET_AGENT): $(SOURCES) $(HEADERS) $(ANTLR_JAVA_EXPRESSION_GENERATED_SOURCES) $(ANTLR_JAVA_EXPRESSION_GENERATED_HEADERS) $(JNI_PROXIES_GENERATED_SOURCES) $(JNI_PROXIES_GENERATED_HEADERS)
	$(CC) $(INCLUDES) $(CFLAGS) $(OPT_FLAGS) $(LDFLAGS) $(SOURCES) $(LIBS) -o $(TARGET_AGENT) $(LDS_FLAGS)

$(INTERNALS_CLASS_LOADER):
ifeq ($(JAVA_BUILD),maven)
	cd internals-class-loader ; mvn clean install
else
	mkdir -p internals-class-loader/target
	cd internals-class-loader ; javac -g:none -d target src/main/java/com/google/devtools/cdbg/*.java
endif
	cp internals-class-loader/target/com/google/devtools/cdbg/debuglets/java/InternalsClassLoader.class $(INTERNALS_CLASS_LOADER)

$(INTERNALS_JAR_FULL):
ifeq ($(JAVA_BUILD),maven)
	cd internals ; mvn clean install
	cp internals/target/cdbg_java_agent_internals-SNAPSHOT-jar-with-dependencies.jar $(INTERNALS_JAR_FULL)
else
	mkdir -p internals/target
	cd internals ; javac -g:none -cp $(ASM_JAR_PATH):$(ASM_COMMONS_JAR_PATH):$(ASM_UTIL_JAR_PATH):$(GSON_JAR_PATH):$(GOOGLE_API_CLIENT_JARS_CLASS_PATH) -d target src/main/java/com/google/devtools/cdbg/debuglets/java/*.java
	cd internals/target ; jar xf $(ASM_JAR_PATH) ; rm -rf META-INF
	cd internals/target ; jar xf $(ASM_COMMONS_JAR_PATH) ; rm -rf META-INF
	cd internals/target ; jar xf $(ASM_UTIL_JAR_PATH) ; rm -rf META-INF
	cd internals/target ; jar xf $(GSON_JAR_PATH) ; rm -rf META-INF
	cd internals/target ; echo "$(GOOGLE_API_CLIENT_JARS)" | xargs -n 1 jar xf
	jar cvf $(INTERNALS_JAR_FULL) -C internals/target/ .
endif

# Remove service account authentication classes from internals.jar. They take
# a lot of space and usually not needed. We will use the stripped JAR in all
# cases when service account authentication is not used.
# We remove classes with jarjar and then remove non class files that we don't
# need with zip utility.
$(INTERNALS_JAR_STRIPPED): $(INTERNALS_JAR_FULL)
	java -jar $(JARJAR_PATH) process strip_service_account_auth_jarjar_rules.txt $(INTERNALS_JAR_FULL) $(INTERNALS_JAR_STRIPPED)

# Java Cloud Debugger package used in Managed VM (aka AppEngine v2)
$(TARGET_TAR_GZ_APPENGINE): $(TARGET_AGENT) $(INTERNALS_CLASS_LOADER) $(INTERNALS_JAR_STRIPPED) $(CDBG_CONFIG_XML) $(APPENGINE_FORMAT_SCRIPT)
	cp $(INTERNALS_JAR_STRIPPED) cdbg_java_agent_internals.jar
	tar cfvz $(TARGET_TAR_GZ_APPENGINE) $(TARGET_AGENT) $(INTERNALS_CLASS_LOADER) cdbg_java_agent_internals.jar $(CDBG_CONFIG_XML) $(APPENGINE_FORMAT_SCRIPT)

# Java Cloud Debugger package used on GCE with metadata service 
# authentication (default and recommended solution)
$(TARGET_TAR_GZ_GCE): $(TARGET_AGENT) $(INTERNALS_CLASS_LOADER) $(INTERNALS_JAR_STRIPPED) $(CDBG_CONFIG_XML)
	cp $(INTERNALS_JAR_STRIPPED) cdbg_java_agent_internals.jar
	tar cfvz $(TARGET_TAR_GZ_GCE) $(TARGET_AGENT) $(INTERNALS_CLASS_LOADER) cdbg_java_agent_internals.jar $(CDBG_CONFIG_XML)

# Java Cloud Debugger package used on GCE with service account authentication 
# (fallback solution used when metadata service authentication can't be used). 
$(TARGET_TAR_GZ_SERVICE_ACCOUNT): $(TARGET_AGENT) $(INTERNALS_CLASS_LOADER) $(INTERNALS_JAR_FULL) $(CDBG_CONFIG_XML)
	cp $(INTERNALS_JAR_FULL) cdbg_java_agent_internals.jar
	tar cfvz $(TARGET_TAR_GZ_SERVICE_ACCOUNT) $(TARGET_AGENT) $(INTERNALS_CLASS_LOADER) cdbg_java_agent_internals.jar $(CDBG_CONFIG_XML)

# Separate tool to obtain OAuth access token given service account email
# and p12 file. This tool is used by GCE bootstrap script when service account
# authentication is enabled.
$(SERVICE_ACCOUNT_AUTH_TOOL): $(INTERNALS_JAR_FULL)
ifeq ($(JAVA_BUILD),maven)
	cd service-account-auth ; mvn clean install
	cp service-account-auth/target/cdbg_service_account_auth-SNAPSHOT-jar-with-dependencies.jar $(SERVICE_ACCOUNT_AUTH_TOOL)
else
	mkdir -p service-account-auth/target
	cd service-account-auth ; javac -g:none -cp ../$(INTERNALS_JAR_FULL) -d target src/main/java/com/google/devtools/cdbg/*.java
	cd service-account-auth/target ; jar xf ../../$(INTERNALS_JAR_FULL) ; rm -rf META-INF
	jar cvfe $(SERVICE_ACCOUNT_AUTH_TOOL) com.google.devtools.cdbg.debuglets.java.ServiceAccountAuthTool -C service-account-auth/target/ .
endif

$(TARGET_VERSION_TXT): $(INTERNALS_JAR_STRIPPED)
	java -cp $(INTERNALS_JAR_STRIPPED) com.google.devtools.cdbg.debuglets.java.GcpDebugletVersion > $@
