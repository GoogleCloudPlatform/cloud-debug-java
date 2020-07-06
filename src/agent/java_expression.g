//
// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// ANTLR2 grammar for Java Cloud Debugger expression compiler.
// This grammar was inspired by this ANTLR3 grammar:
//   third_party/antlr3/java_grammar/Java.g

header "post_include_hpp" {
  #include "common.h"
  #include "java_expression.h"
  #include "messages.h"
  #include "model.h"
}

options {
  language="Cpp";
  namespace="devtools::cdbg";
}

//
// LEXER
//

class JavaExpressionLexer extends Lexer;

options {
  k = 3;  // Set lookahead.
  charVocabulary = '\3'..'\377';
}

// Tokens:
tokens {
  // We need a few "imaginary" tokens to inject common AST nodes in different
  // places. This technique allows common treatment for all such places in
  // tree walking.
  STATEMENT;
  BINARY_EXPRESSION;
  INSTANCEOF_BINARY_EXPRESSION;
  UNARY_EXPRESSION;
  PARENTHESES_EXPRESSION;
  TYPE_CAST;
  TYPE_NAME;
  PRIMARY_SELECTOR;
  DOT_SELECTOR;
  METHOD_CALL;
  EXPRESSION_LIST;

  JNULL         = "null";
  TRUE          = "true";
  FALSE         = "false";
  INSTANCEOF     = "instanceof";

  // No explicit lexer rule exists for DOT. See NumericLiteral lexer rule for
  // details.
  DOT;

  // Numeric literals.
  HEX_NUMERIC_LITERAL;
  OCT_NUMERIC_LITERAL;
  FP_NUMERIC_LITERAL;
  DEC_NUMERIC_LITERAL;
}

protected HexDigit
  : '0'..'9' | 'a'..'f' | 'A'..'F'
  ;

protected DecDigit
  : '0'..'9'
  ;

protected OctDigit
  : '0'..'7'
  ;

NumericLiteral
  : ("0x") => "0x" (HexDigit)+  ('l' | 'L')?
      // hex: 0x12AB, 0x34CDL
      { $setType(HEX_NUMERIC_LITERAL); }
  | ('0' OctDigit) => '0' (OctDigit)+  ('l' | 'L')?
      // oct: 01234, 05670L
      { $setType(OCT_NUMERIC_LITERAL); }
  | ((DecDigit)*  '.' DecDigit) => (DecDigit)* '.' (DecDigit)+ ('d' | 'D' | 'f' | 'F')?
      // fp: 12.3, .4, 5.6f, .6d
      { $setType(FP_NUMERIC_LITERAL); }
  | ((DecDigit)+ ('d' | 'D' | 'f' | 'F')) => (DecDigit)+ ('d' | 'D' | 'f' | 'F')
      // fp: 12f, 34d
      { $setType(FP_NUMERIC_LITERAL); }
  | (DecDigit)+ ('l' | 'L')?
      // dec: 123, 456L
      { $setType(DEC_NUMERIC_LITERAL); }
  | '.'
      { $setType(DOT); }
  ;

CharacterLiteral
  : '\''! SingleCharacter '\''!
  | '\''! EscapeSequence '\''!
  ;

protected SingleCharacter
  : ~('\'' | '\\')
  ;

StringLiteral
  : '"'! (StringCharacters)? '"'!
  ;

protected StringCharacters
  : (StringCharacter)+
  ;

protected StringCharacter
  : ~('"' | '\\')
  | EscapeSequence
  ;

protected EscapeSequence
  : '\\' ('b' | 't' | 'n' | 'f' | 'r' | '"' | '\'' | '\\')
  | OctalEscape
  | UnicodeEscape
  ;

protected OctalEscape
  : ('\\' OctDigit) => '\\' OctDigit
  | ('\\' OctDigit OctDigit) => '\\' OctDigit OctDigit
  | '\\' ZeroToThree OctDigit OctDigit
  ;

protected UnicodeEscape
  : '\\' 'u' HexDigit HexDigit HexDigit HexDigit
  ;

protected ZeroToThree
  : ('0'..'3')
  ;


// Only ASCII characters are supported right now.
// TODO: add support for Unicode characters in Java identifiers.
Identifier
  : ( 'a'..'z' | 'A'..'Z' | '$' | '_' )
    ( 'a'..'z' | 'A'..'Z' | '0'..'9' | '$' | '_' )*
  ;

LPAREN          : "(";
RPAREN          : ")";
LBRACE          : "{";
RBRACE          : "}";
LBRACK          : "[";
RBRACK          : "]";
SEMI            : ";";
COMMA           : ",";
ASSIGN          : "=";
CMP_GT          : ">";
CMP_LT          : "<";
BANG            : "!";
TILDE           : "~";
QUESTION        : "?";
COLON           : ":";
EQUAL           : "==";
CMP_LE          : "<=";
CMP_GE          : ">=";
NOTEQUAL        : "!=";
AND             : "&&";
OR              : "||";
ADD             : "+";
SUB             : "-";
MUL             : "*";
DIV             : "/";
BITAND          : "&";
BITOR           : "|";
CARET           : "^";
MOD             : "%";
SHIFT_LEFT      : "<<";
SHIFT_RIGHT_S   : ">>";
SHIFT_RIGHT_U   : ">>>";


// Skip whitespaces and comments.

WS
  : ('\t' | '\r' | '\n' | ' ')+
    { $setType(antlr::Token::SKIP); }
  ;

COMMENT
  : "/*" ( { LA(2) != '/' }? '*' | ~('*') )* "*/"
    { $setType(antlr::Token::SKIP); }
  ;

LINE_COMMENT
  : ("//" (~('\r' | '\n'))*)
    { $setType(antlr::Token::SKIP); }
  ;


//
// PARSER
//

class JavaExpressionParser extends Parser;

options {
  k = 2;  // Set lookahead.
  buildAST = true;
  importVocab = JavaExpressionLexer;
}

{
 public:
  // Init or re-init the parser
  void Init() {
    errors_.clear();
    initializeASTFactory(fact_);
    setASTFactory(&fact_);
  }

  void reportError(const std::string& s) {
    errors_.push_back(s);
  }

  void reportError(const antlr::RecognitionException& ex) {
    errors_.push_back(ex.getMessage());
  }

  int num_errors() { return errors_.size(); }

  const std::vector<std::string>& errors() { return errors_; }

 private:
  std::vector<std::string> errors_;
  antlr::ASTFactory fact_;
}

statement
  : expression
    { #statement = #([STATEMENT, "statement"], #statement); }
    EOF!
  ;

expression
  : conditionalExpression
  ;

expressionList
  : expression
    (
      COMMA! expressionList
    )?
    { #expressionList = #([EXPRESSION_LIST, "expression_list"], #expressionList); }
  ;

conditionalExpression
  : conditionalOrExpression
    (QUESTION^ expression COLON conditionalExpression)?
  ;

conditionalOrExpression
  : conditionalAndExpression
    (
      { #conditionalOrExpression = #([BINARY_EXPRESSION, "binary_expression"], #conditionalOrExpression); }
      OR conditionalAndExpression
    )*
  ;

conditionalAndExpression
  : inclusiveOrExpression
    (
      { #conditionalAndExpression = #([BINARY_EXPRESSION, "binary_expression"], #conditionalAndExpression); }
      AND inclusiveOrExpression
    )*
  ;

inclusiveOrExpression
  : exclusiveOrExpression
    (
      { #inclusiveOrExpression = #([BINARY_EXPRESSION, "binary_expression"], #inclusiveOrExpression); }
      BITOR exclusiveOrExpression
    )*
  ;

exclusiveOrExpression
  : andExpression
    (
      { #exclusiveOrExpression = #([BINARY_EXPRESSION, "binary_expression"], #exclusiveOrExpression); }
      CARET andExpression
    )*
  ;

andExpression
  : equalityExpression
    (
      { #andExpression = #([BINARY_EXPRESSION, "binary_expression"], #andExpression); }
      BITAND equalityExpression
    )*
  ;

equalityExpression
  : instanceofExpression
    (
      { #equalityExpression = #([BINARY_EXPRESSION, "binary_expression"], #equalityExpression); }
      (EQUAL | NOTEQUAL) instanceofExpression
    )*
  ;

instanceofExpression
  : relationalExpression
    (
      { #instanceofExpression = #([INSTANCEOF_BINARY_EXPRESSION, "instanceof_binary_expression"], #instanceofExpression); }
      INSTANCEOF classOrInterfaceType
    )?
  ;

relationalExpression
  : shiftExpression
    (
      { #relationalExpression = #([BINARY_EXPRESSION, "binary_expression"], #relationalExpression); }
      (CMP_LE | CMP_GE | CMP_LT | CMP_GT) shiftExpression
    )*
  ;

shiftExpression
  : additiveExpression
    (
      { #shiftExpression = #([BINARY_EXPRESSION, "binary_expression"], #shiftExpression); }
      (SHIFT_LEFT | SHIFT_RIGHT_S | SHIFT_RIGHT_U) additiveExpression
    )*
  ;

additiveExpression
  : multiplicativeExpression
    (
      { #additiveExpression = #([BINARY_EXPRESSION, "binary_expression"], #additiveExpression); }
      (ADD | SUB) multiplicativeExpression
    )*
  ;

multiplicativeExpression
  : unaryExpression
    (
      { #multiplicativeExpression = #([BINARY_EXPRESSION, "binary_expression"], #multiplicativeExpression); }
      (MUL | DIV | MOD) unaryExpression
    )*
  ;

unaryExpression
  : (ADD | SUB) unaryExpression
    { #unaryExpression = #([UNARY_EXPRESSION, "unary_expression"], #unaryExpression); }
  | unaryExpressionNotPlusMinus
  ;

unaryExpressionNotPlusMinus
  : (TILDE | BANG) unaryExpression
    { #unaryExpressionNotPlusMinus = #([UNARY_EXPRESSION, "unary_expression"], #unaryExpressionNotPlusMinus); }
  | (castExpression) => castExpression
  | primary
    (
      { #unaryExpressionNotPlusMinus = #([PRIMARY_SELECTOR, "primary_selector"], #unaryExpressionNotPlusMinus); }
      selector
    )*
  ;

castExpression
  : LPAREN! classOrInterfaceType RPAREN! unaryExpressionNotPlusMinus
    { #castExpression = #([TYPE_CAST, "type_cast"], #castExpression); }
  ;

primary
  : LPAREN! expression RPAREN!
    { #primary = #([PARENTHESES_EXPRESSION, "parentheses_expression"], #primary); }
  | Identifier
    (
      arguments
      { #primary = #([METHOD_CALL, "method_call"], #primary); }
    )?
  | literal
  ;

arguments
  : LPAREN! (expressionList)? RPAREN!
  ;

selector
  : DOT!
    Identifier
    (
      arguments
      { #selector = #([METHOD_CALL, "method_call"], #selector); }
    )?
    { #selector = #([DOT_SELECTOR, "dot_selector"], #selector); }
  | LBRACK^ expression RBRACK
  ;

literal
  : HEX_NUMERIC_LITERAL
  | OCT_NUMERIC_LITERAL
  | FP_NUMERIC_LITERAL
  | DEC_NUMERIC_LITERAL
  | CharacterLiteral
  | StringLiteral
  | TRUE
  | FALSE
  | JNULL
  ;

// TODO: add support for generic types.
classOrInterfaceType
  : Identifier
    (
      DOT!
      classOrInterfaceType
    )?
    { #classOrInterfaceType = #([TYPE_NAME, "type_name"], #classOrInterfaceType); }
  ;


//
// TREE WALKING (COMPILATION)
//

class JavaExpressionCompiler extends TreeParser;

options {
  importVocab = JavaExpressionLexer;
}

{
 public:
  // Since the process of compilation and evaluation is recursive and uses OS
  // stack, very deep expression trees can cause stack overflow. "MaxTreeDepth"
  // limits the maximum recursion depth to a safe threshold.
  static constexpr int kMaxTreeDepth = 25;

  void Init() {
  }

  std::unique_ptr<JavaExpression> Walk(antlr::RefAST ast) {
    ResetErrorMessage();

    if (!VerifyMaxDepth(ast, kMaxTreeDepth)) {
      LOG(WARNING) << "The parsed expression tree is too deep";
      SetErrorMessage({ ExpressionTreeTooDeep });
      return nullptr;
    }

    std::unique_ptr<JavaExpression> expression(statement(ast));
    if (expression == nullptr) {
      // Set generic error message if specific error wasn't set.
      SetErrorMessage({ ExpressionParserError });
    }

    return expression;
  }

  // Getter for the formatted error message. The error message will only be
  // set when "Walk" fails.
  const FormatMessageModel& error_message() const { return error_message_; }

 private:
  void ResetErrorMessage() {
    error_message_ = FormatMessageModel();
  }

  void SetErrorMessage(FormatMessageModel new_error_message) {
    if (error_message_.format.empty()) {
      error_message_ = std::move(new_error_message);
    }
  }

  void reportError(const antlr::RecognitionException& ex) {
    reportError(ex.getMessage());
  }

  void reportError(const std::string& msg) {
    LOG(ERROR) << "Internal parser error: " << msg;
  }

  // Verifies that the maximum depth of the subtree "node" does
  // not exceed "max_depth".
  static bool VerifyMaxDepth(antlr::RefAST node, int max_depth) {
    if (max_depth == 0) {
      return false;
    }

    DCHECK(node != nullptr);

    antlr::RefAST child_node = node->getFirstChild();
    while (child_node != nullptr) {
      if (!VerifyMaxDepth(child_node, max_depth - 1)) {
        return false;
      }

      child_node = child_node->getNextSibling();
    }

    return true;
  }

 private:
  // Formatted localizable error message reported back to the user. The
  // message template are defined in "messages.h" file. Empty
  // "error_message_.format" indicates that the error message is not available.
  // Only first error message encountered is captured. Subsequent error 
  // messages are ignored.
  FormatMessageModel error_message_;
}


statement returns [JavaExpression* root] {
    root = nullptr;
  }
  : #(STATEMENT root=expression)
  ;

expression returns [JavaExpression* je] {
    JavaExpression* a = nullptr;
    JavaExpression* b = nullptr;
    JavaExpression* c = nullptr;
    JavaExpressionSelector* s = nullptr;
    MethodArguments* r = nullptr;
    BinaryJavaExpression::Type binary_expression_type;
    UnaryJavaExpression::Type unary_expression_type;
    std::list<std::vector<jchar>> string_sequence;
    std::string type;

    je = nullptr;
  }
  : #(QUESTION a=expression b=expression COLON c=expression) {
    if ((a == nullptr) || (b == nullptr) || (c == nullptr)) {
      reportError("NULL argument in conditional expression");
      delete a;
      delete b;
      delete c;
      je = nullptr;
    } else {
      je = new ConditionalJavaExpression(a, b, c);
    }
  }
  | #(PARENTHESES_EXPRESSION je=expression)
  | #(BINARY_EXPRESSION a=expression binary_expression_type=binary_expression_token b=expression) {
    if ((a == nullptr) || (b == nullptr)) {
      reportError("NULL argument in binary expression");
      delete a;
      delete b;
      je = nullptr;
    } else {
      je = new BinaryJavaExpression(binary_expression_type, a, b);
    }
  }
  | #(INSTANCEOF_BINARY_EXPRESSION a=expression INSTANCEOF type=type_name) {
    if (a == nullptr)  {
      reportError("NULL argument in instanceof binary expression");
      delete a;
      je = nullptr;
    } else {
      je = new InstanceofBinaryJavaExpression(a, type);
    }
  }
  | #(UNARY_EXPRESSION unary_expression_type=unary_expression_token a=expression) {
    if (a == nullptr) {
      reportError("NULL argument in unary expression");
      je = nullptr;
    } else {
      je = new UnaryJavaExpression(unary_expression_type, a);
    }
  }
  | identifier_node:Identifier {
    je = new JavaIdentifier(identifier_node->getText());
  }
  | hex_numeric_literal_node:HEX_NUMERIC_LITERAL {
    JavaIntLiteral* nl = new JavaIntLiteral;
    if (!nl->ParseString(hex_numeric_literal_node->getText(), 16)) {
      reportError("Hex integer literal could not be parsed");
      SetErrorMessage({ 
        BadNumericLiteral,
        { hex_numeric_literal_node->getText() }
      });

      delete nl;
      je = nullptr;
    } else {
      je = nl;
    }
  }
  | oct_numeric_literal_node:OCT_NUMERIC_LITERAL {
    JavaIntLiteral* nl = new JavaIntLiteral;
    if (!nl->ParseString(oct_numeric_literal_node->getText(), 8)) {
      reportError("Octal integer literal could not be parsed");
      SetErrorMessage({ 
        BadNumericLiteral,
        { oct_numeric_literal_node->getText() }
      });

      delete nl;
      je = nullptr;
    } else {
      je = nl;
    }
  }
  | fp_numeric_literal_node:FP_NUMERIC_LITERAL {
    JavaFloatLiteral* nl = new JavaFloatLiteral;
    if (!nl->ParseString(fp_numeric_literal_node->getText())) {
      reportError("Floating point literal could not be parsed");
      SetErrorMessage({ 
        BadNumericLiteral,
        { fp_numeric_literal_node->getText() }
      });

      delete nl;
      je = nullptr;
    } else {
      je = nl;
    }
  }
  | dec_numeric_literal_node:DEC_NUMERIC_LITERAL {
    JavaIntLiteral* nl = new JavaIntLiteral;
    if (!nl->ParseString(dec_numeric_literal_node->getText(), 10)) {
      reportError("Decimal integer literal could not be parsed");
      SetErrorMessage({ 
        BadNumericLiteral,
        { dec_numeric_literal_node->getText() }
      });

      delete nl;
      je = nullptr;
    } else {
      je = nl;
    }
  }
  | character_literal_node:CharacterLiteral {
    JavaCharLiteral* cl = new JavaCharLiteral;
    if (!cl->ParseString(character_literal_node->getText())) {
      reportError("Invalid character");
      delete cl;
      je = nullptr;
    } else {
       je = cl;
    }
  }
  | string_literal_node:StringLiteral {
    JavaStringLiteral* sl = new JavaStringLiteral;
    if (!sl->ParseString(string_literal_node->getText())) {
      reportError("Invalid string");
      delete sl;
      je = nullptr;
    } else {
       je = sl;
    }
  }
  | TRUE {
    je = new JavaBooleanLiteral(JNI_TRUE);
  }
  | FALSE {
    je = new JavaBooleanLiteral(JNI_FALSE);
  }
  | JNULL {
    je = new JavaNullLiteral;
  }
  | #(PRIMARY_SELECTOR a=expression s=selector) {
    if ((a == nullptr) || (s == nullptr)) {
      reportError("PRIMARY_SELECTED inner expressions failed to compile");
      delete a;
      delete s;
      je = nullptr;
    } else {
      s->set_source(a);
      je = s;
    }
  }
  | #(TYPE_CAST type=type_name a=expression) {
    if (a == nullptr) {
      reportError("NULL source in type cast expression");
      je = nullptr;
    } else {
      je = new TypeCastJavaExpression(type, a);
    }
  }
  | #(METHOD_CALL method_node:Identifier r=arguments) {
    if (r == nullptr) {
      reportError("NULL method arguments in expression METHOD_CALL");
      je = nullptr;
    } else {
      je = new MethodCallExpression(method_node->getText(), r);
    }
  }
  ;

binary_expression_token returns [BinaryJavaExpression::Type type] {
    type = static_cast<BinaryJavaExpression::Type>(-1);
  }
  : ADD { type = BinaryJavaExpression::Type::add; }
  | SUB { type = BinaryJavaExpression::Type::sub; }
  | MUL { type = BinaryJavaExpression::Type::mul; }
  | DIV { type = BinaryJavaExpression::Type::div; }
  | MOD { type = BinaryJavaExpression::Type::mod; }
  | AND { type = BinaryJavaExpression::Type::conditional_and; }
  | OR { type = BinaryJavaExpression::Type::conditional_or; }
  | EQUAL { type = BinaryJavaExpression::Type::eq; }
  | NOTEQUAL { type = BinaryJavaExpression::Type::ne; }
  | CMP_LE { type = BinaryJavaExpression::Type::le; }
  | CMP_GE { type = BinaryJavaExpression::Type::ge; }
  | CMP_LT { type = BinaryJavaExpression::Type::lt; }
  | CMP_GT { type = BinaryJavaExpression::Type::gt; }
  | BITAND { type = BinaryJavaExpression::Type::bitwise_and; }
  | BITOR { type = BinaryJavaExpression::Type::bitwise_or; }
  | CARET { type = BinaryJavaExpression::Type::bitwise_xor; }
  | SHIFT_LEFT { type = BinaryJavaExpression::Type::shl; }
  | SHIFT_RIGHT_S { type = BinaryJavaExpression::Type::shr_s; }
  | SHIFT_RIGHT_U { type = BinaryJavaExpression::Type::shr_u; }
  ;

unary_expression_token returns [UnaryJavaExpression::Type type] {
    type = static_cast<UnaryJavaExpression::Type>(-1);
  }
  : ADD { type = UnaryJavaExpression::Type::plus; }
  | SUB { type = UnaryJavaExpression::Type::minus; }
  | TILDE { type = UnaryJavaExpression::Type::bitwise_complement; }
  | BANG { type = UnaryJavaExpression::Type::logical_complement; }
  ;

selector returns [JavaExpressionSelector* js] {
    JavaExpression* a = nullptr;
    JavaExpressionSelector* ds = nullptr;

    js = nullptr;
  }
  : #(DOT_SELECTOR ds=dotSelector) {
    if (ds == nullptr) {
      reportError("Failed to parse dot expression");
      js = nullptr;
    } else {
      js = ds;
    }
  }
  | #(LBRACK a=expression RBRACK) {
    if (a == nullptr) {
      reportError("Failed to parse index expression");
      js = nullptr;
    } else {
      js = new JavaExpressionIndexSelector(a);
    }
  }
  ;

dotSelector returns [JavaExpressionSelector* js] {
    MethodArguments* r = nullptr;

    js = nullptr;
  }
  : identifier_node:Identifier {
    js = new JavaExpressionMemberSelector(identifier_node->getText());
  }
  | #(METHOD_CALL method_node:Identifier r=arguments) {
    if (r == nullptr) {
      reportError("NULL method arguments in dotSelector METHOD_CALL");
      js = nullptr;
    } else {
      js = new MethodCallExpression(method_node->getText(), r);
    }
  }
  ;


arguments returns [MethodArguments* args] {
    MethodArguments* tail = nullptr;
    JavaExpression* arg = nullptr;

    args = nullptr;
  }
  : #(EXPRESSION_LIST arg=expression tail=arguments) {
    if ((arg == nullptr) || (tail == nullptr)) {
      reportError("NULL method argument");
      delete arg;
      delete tail;
      args = nullptr;
    } else {
      args = new MethodArguments(arg, tail);
    }
  }
  | {
    args = new MethodArguments();
  }
  ;

type_name returns [std::string t] {
    std::string tail;
  }
  : #(TYPE_NAME n1:Identifier tail=type_name) {
    t = n1->getText();
    if (!tail.empty()) {
      t += '.';
      t += tail;
    }
  }
  | { }
  ;
