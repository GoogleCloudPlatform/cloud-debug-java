package(default_visibility = ["//visibility:public"])

cc_library(
    name = "antlr",
    srcs = [
        "ANTLRUtil.cpp",
        "ASTFactory.cpp",
        "ASTNULLType.cpp",
        "ASTRefCount.cpp",
        "BaseAST.cpp",
        "BitSet.cpp",
        "CharBuffer.cpp",
        "CharScanner.cpp",
        "CommonAST.cpp",
        "CommonASTWithHiddenTokens.cpp",
        "CommonHiddenStreamToken.cpp",
        "CommonToken.cpp",
        "InputBuffer.cpp",
        "LLkParser.cpp",
        "MismatchedCharException.cpp",
        "MismatchedTokenException.cpp",
        "NoViableAltException.cpp",
        "NoViableAltForCharException.cpp",
        "Parser.cpp",
        "RecognitionException.cpp",
        "String.cpp",
        "Token.cpp",
        "TokenBuffer.cpp",
        "TokenRefCount.cpp",
        "TokenStreamBasicFilter.cpp",
        "TokenStreamHiddenTokenFilter.cpp",
        "TokenStreamRewriteEngine.cpp",
        "TokenStreamSelector.cpp",
        "TreeParser.cpp",
    ],
    copts = ["-Ithird_party/antlr/lib/cpp"],
    deps = [
      "//third_party/antlr/lib/cpp/antlr:headers",
    ]
)
