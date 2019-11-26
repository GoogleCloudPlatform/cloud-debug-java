/**
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JAVA_EXPRESSION_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JAVA_EXPRESSION_H_

#include <list>
#include <memory>

#include "common.h"
#include "expression_util.h"

namespace devtools {
namespace cdbg {

class ExpressionEvaluator;
struct FormatMessageModel;

// Interface representing a node in parsed expression tree.
class JavaExpression {
 public:
  virtual ~JavaExpression() { }

  // Prints the expression subtree to the stream. When "concise" is true, the
  // function prints expression in Java format. When "concise" is false, a
  // much more verbose format is used (this mode is used by unit test to
  // disambiguate different types of expressions that might look the same in
  // concise format).
  virtual void Print(std::ostream* os, bool concise) = 0;

  // Tries to convert the expression subtree into a type name. For
  // example: Member("String", Member("lang", Identifier("java")) can be
  // converted to "java.lang.String". At the same time (a+b) cannot.
  virtual bool TryGetTypeName(std::string* name) const = 0;

  // Compiles the expression into executable format. The caller owns the
  // returned instance. If a particular language feature is not yet supported,
  // the function returns null and prints description in "error_message".
  virtual CompiledExpression CreateEvaluator() = 0;
};


// Represents (a ? b : c) conditional expression.
class ConditionalJavaExpression : public JavaExpression {
 public:
  // Takes ownership over the passed instances of "JavaExpression";
  ConditionalJavaExpression(
      JavaExpression* condition,
      JavaExpression* if_true,
      JavaExpression* if_false);

  void Print(std::ostream* os, bool concise) override;

  bool TryGetTypeName(std::string* name) const override { return false; }

  CompiledExpression CreateEvaluator() override;

 private:
  std::unique_ptr<JavaExpression> condition_;
  std::unique_ptr<JavaExpression> if_true_;
  std::unique_ptr<JavaExpression> if_false_;

  DISALLOW_COPY_AND_ASSIGN(ConditionalJavaExpression);
};

// Represents any kind of binary expression except instanceof (like a + b).
class BinaryJavaExpression : public JavaExpression {
 public:
  enum class Type {
    add,
    sub,
    mul,
    div,
    mod,
    conditional_and,
    conditional_or,
    eq,
    ne,
    le,
    ge,
    lt,
    gt,
    bitwise_and,
    bitwise_or,
    bitwise_xor,
    shl,
    shr_s,
    shr_u
  };

  // Takes ownership over "a" and "b" expression subtrees.
  BinaryJavaExpression(Type type, JavaExpression* a, JavaExpression* b);

  void Print(std::ostream* os, bool concise) override;

  bool TryGetTypeName(std::string* name) const override { return false; }

  CompiledExpression CreateEvaluator() override;

 private:
  const Type type_;
  std::unique_ptr<JavaExpression> a_;
  std::unique_ptr<JavaExpression> b_;

  DISALLOW_COPY_AND_ASSIGN(BinaryJavaExpression);
};

// Represents instanceof binary expression.
class InstanceofBinaryJavaExpression : public JavaExpression {
 public:
  explicit InstanceofBinaryJavaExpression(JavaExpression* source,
                                          std::string reference_type)
      : source_(std::unique_ptr<JavaExpression>(source)),
        reference_type_(reference_type) {}

  void Print(std::ostream* os, bool concise) override;

  bool TryGetTypeName(std::string* name) const override { return false; }

  CompiledExpression CreateEvaluator() override;

 private:
  std::unique_ptr<JavaExpression> source_;
  const std::string reference_type_;

  DISALLOW_COPY_AND_ASSIGN(InstanceofBinaryJavaExpression);
};

// Represents unary expression (like ~a).
class UnaryJavaExpression : public JavaExpression {
 public:
  enum class Type {
    plus,
    minus,
    bitwise_complement,
    logical_complement
  };

  // Takes ownership over "a" expression subtree.
  UnaryJavaExpression(Type type, JavaExpression* a);

  void Print(std::ostream* os, bool concise) override;

  bool TryGetTypeName(std::string* name) const override { return false; }

  CompiledExpression CreateEvaluator() override;

 private:
  const Type type_;
  std::unique_ptr<JavaExpression> a_;

  DISALLOW_COPY_AND_ASSIGN(UnaryJavaExpression);
};


class JavaIntLiteral : public JavaExpression {
 public:
  JavaIntLiteral() : is_long_(false) { }

  // Parses an integer in the given base from the given string.
  bool ParseString(const std::string& str, int base);

  void Print(std::ostream* os, bool concise) override;

  bool TryGetTypeName(std::string* name) const override { return false; }

  CompiledExpression CreateEvaluator() override;

 private:
  // Returns true if this is a long integer.
  bool IsLong() const { return is_long_; }

 private:
  // Indicates whether this is an int or a long.
  bool is_long_;

  jlong n_;

  DISALLOW_COPY_AND_ASSIGN(JavaIntLiteral);
};


class JavaFloatLiteral : public JavaExpression {
 public:
  JavaFloatLiteral() : is_double_(true) { }

  // Parses a floating point number from the given string.
  bool ParseString(const std::string& str);

  void Print(std::ostream* os, bool concise) override;

  bool TryGetTypeName(std::string* name) const override { return false; }

  CompiledExpression CreateEvaluator() override;

  // Returns true if this is a double.
  bool IsDouble() const { return is_double_; }

 private:
  // Indicates whether this is an float or a double.
  bool is_double_;

  jdouble d_;

  DISALLOW_COPY_AND_ASSIGN(JavaFloatLiteral);
};

// Represents character constant. All characters in Java are Unicode, so
// this is a 16 bit integer.
class JavaCharLiteral : public JavaExpression {
 public:
  JavaCharLiteral() : ch_('\0') { }

  // Decodes the potentially escaped character into a Unicode character.
  // Examples for encoding are: '\n', '\\', '\293', '\u5C7f'.
  bool ParseString(const std::string& str);

  void Print(std::ostream* os, bool concise) override;

  bool TryGetTypeName(std::string* name) const override { return false; }

  CompiledExpression CreateEvaluator() override;

 private:
  jchar ch_;

  DISALLOW_COPY_AND_ASSIGN(JavaCharLiteral);
};


// Represents a Java string constant.
class JavaStringLiteral : public JavaExpression {
 public:
  JavaStringLiteral() { }

  // Decodes the potentially escaped characters sequence into Java string.
  // Example of encoded string: "This\n is\t an\" \u1E3B encoded \88 string".
  bool ParseString(const std::string& str);

  void Print(std::ostream* os, bool concise) override;

  bool TryGetTypeName(std::string* name) const override { return false; }

  CompiledExpression CreateEvaluator() override;

 private:
  std::vector<jchar> str_;

  DISALLOW_COPY_AND_ASSIGN(JavaStringLiteral);
};


// Represents a boolean constant.
class JavaBooleanLiteral : public JavaExpression {
 public:
  explicit JavaBooleanLiteral(jboolean n) : n_(n) { }

  void Print(std::ostream* os, bool concise) override;

  bool TryGetTypeName(std::string* name) const override { return false; }

  CompiledExpression CreateEvaluator() override;

 private:
  const jboolean n_;

  DISALLOW_COPY_AND_ASSIGN(JavaBooleanLiteral);
};


// Represents Java "null".
class JavaNullLiteral : public JavaExpression {
 public:
  JavaNullLiteral() { }

  void Print(std::ostream* os, bool concise) override;

  bool TryGetTypeName(std::string* name) const override { return false; }

  CompiledExpression CreateEvaluator() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(JavaNullLiteral);
};


// Represents a local or a static variable.
class JavaIdentifier : public JavaExpression {
 public:
  explicit JavaIdentifier(std::string identifier) : identifier_(identifier) {}

  void Print(std::ostream* os, bool concise) override;

  bool TryGetTypeName(std::string* name) const override;

  CompiledExpression CreateEvaluator() override;

 private:
  const std::string identifier_;

  DISALLOW_COPY_AND_ASSIGN(JavaIdentifier);
};


// Represents a type cast for classes or interfaces.
class TypeCastJavaExpression : public JavaExpression {
 public:
  explicit TypeCastJavaExpression(std::string type, JavaExpression* source)
      : type_(type), source_(std::unique_ptr<JavaExpression>(source)) {}

  void Print(std::ostream* os, bool concise) override;

  bool TryGetTypeName(std::string* name) const override { return false; }

  CompiledExpression CreateEvaluator() override;

 private:
  const std::string type_;
  std::unique_ptr<JavaExpression> source_;

  DISALLOW_COPY_AND_ASSIGN(TypeCastJavaExpression);
};


// Base class representing additional transformation applied to an object. Two
// types of selectors are currently supported:
//    1. Array indexer: a[8].
//    2. Dereferencing a member: a.member.
class JavaExpressionSelector : public JavaExpression {
 public:
  // Setter for "source_". See "source_" comments for explanation. Takes
  // ownership over the "source" expression subtree.
  void set_source(JavaExpression* source) {
    source_.reset(source);
  }

 protected:
  // Represents the base expression on which the selector is applied. In the
  // two examples above, "source_" will be "a". A more complicated example:
  //    (flag ? a1 : a2)[8]
  // In this case "source_" will be the expression corresponding to
  // "flag ? a1 : a2".
  std::unique_ptr<JavaExpression> source_;
};


// Selector for array item. The index is also expression to support
// constructions like a[x + y].
class JavaExpressionIndexSelector : public JavaExpressionSelector {
 public:
  // Takes ownership over the "index" expression subtree.
  explicit JavaExpressionIndexSelector(JavaExpression* index) : index_(index) {
  }

  void Print(std::ostream* os, bool concise) override;

  bool TryGetTypeName(std::string* name) const override { return false; }

  CompiledExpression CreateEvaluator() override;

 private:
  std::unique_ptr<JavaExpression> index_;

  DISALLOW_COPY_AND_ASSIGN(JavaExpressionIndexSelector);
};


// Selector for a class member.
class JavaExpressionMemberSelector : public JavaExpressionSelector {
 public:
  explicit JavaExpressionMemberSelector(const std::string& member)
      : member_(member) {}

  void Print(std::ostream* os, bool concise) override;

  bool TryGetTypeName(std::string* name) const override;

  CompiledExpression CreateEvaluator() override;

 private:
  const std::string member_;

  DISALLOW_COPY_AND_ASSIGN(JavaExpressionMemberSelector);
};


// List of arguments for method invocation.
class MethodArguments {
 public:
  // Empty arguments list.
  MethodArguments() {}

  // Single argument. Takes ownership over "argument".
  MethodArguments(JavaExpression* argument, MethodArguments* tail) {
    if (tail) {
      arguments_.swap(tail->arguments_);
      delete tail;
    }

    arguments_.push_front(std::unique_ptr<JavaExpression>(argument));
  }

  std::list<std::unique_ptr<JavaExpression>>::iterator begin() {
    return arguments_.begin();
  }

  std::list<std::unique_ptr<JavaExpression>>::iterator end() {
    return arguments_.end();
  }

 private:
  std::list<std::unique_ptr<JavaExpression>> arguments_;

  DISALLOW_COPY_AND_ASSIGN(MethodArguments);
};


// Represents method call (with arguments). The method call can be either
// direct like f(1) or through selectors like my.util.f(1) or a.getY().f(1).
// In case of direct method call, "source_" will be null.
class MethodCallExpression : public JavaExpressionSelector {
 public:
  // Takes ownership over "method" and "arguments".
  MethodCallExpression(const std::string& method, MethodArguments* arguments)
      : method_(method),
        arguments_(std::unique_ptr<MethodArguments>(arguments)) {}

  void Print(std::ostream* os, bool concise) override;

  bool TryGetTypeName(std::string* name) const override { return false; }

  CompiledExpression CreateEvaluator() override;

 private:
  const std::string method_;
  std::unique_ptr<MethodArguments> arguments_;

  DISALLOW_COPY_AND_ASSIGN(MethodCallExpression);
};


}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JAVA_EXPRESSION_H_


