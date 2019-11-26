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

#include "java_expression.h"

#include <iomanip>

#include "array_expression_evaluator.h"
#include "binary_expression_evaluator.h"
#include "conditional_operator_evaluator.h"
#include "expression_evaluator.h"
#include "field_evaluator.h"
#include "identifier_evaluator.h"
#include "instanceof_binary_expression_evaluator.h"
#include "literal_evaluator.h"
#include "messages.h"
#include "method_call_evaluator.h"
#include "model.h"
#include "string_evaluator.h"
#include "type_cast_operator_evaluator.h"
#include "unary_expression_evaluator.h"

namespace devtools {
namespace cdbg {

// Single character de-escaping for "UnescapeJavaString". Returns the de-escaped
// character and iterator to the next byte in the input sequence.
static void UnescapeCharacter(std::string::const_iterator it,
                              std::string::const_iterator end,
                              jchar* unicode_character,
                              std::string::const_iterator* next) {
  DCHECK(it < end);

  if ((*it == '\\') && (it + 1 != end)) {
    switch (*(it + 1)) {
      case 't':
        *unicode_character = '\t';
        *next = it + 2;
        return;

      case 'b':
        *unicode_character = '\b';
        *next = it + 2;
        return;

      case 'n':
        *unicode_character = '\n';
        *next = it + 2;
        return;

      case 'r':
        *unicode_character = '\r';
        *next = it + 2;
        return;

      case 'f':
        *unicode_character = '\f';
        *next = it + 2;
        return;

      case '\'':
        *unicode_character = '\'';
        *next = it + 2;
        return;

      case '"':
        *unicode_character = '"';
        *next = it + 2;
        return;

      case '\\':
        *unicode_character = '\\';
        *next = it + 2;
        return;

      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7': {
        int num_length = 1;
        char num[4] = { *(it + 1) };
        if ((it + 2 < end) && (*(it + 2) >= '0') && (*(it + 2) <= '7')) {
          num[num_length++] = *(it + 2);
          if ((it + 3 < end) && (*(it + 3) >= '0') && (*(it + 3) <= '7')) {
            num[num_length++] = *(it + 3);
          }
        }

        num[num_length] = '\0';

        *unicode_character = strtol(num, nullptr, 8);  // NOLINT

        // Octal escape code can only represent ASCII characters.
        if (*unicode_character <= 0xFF) {
          *next = it + (1 + num_length);
          return;
        }

        break;
      }

      case 'u':
        if ((it + 6 <= end) &&
            isxdigit(*(it + 2)) &&
            isxdigit(*(it + 3)) &&
            isxdigit(*(it + 4)) &&
            isxdigit(*(it + 5))) {
          char num[] = {
            *(it + 2),
            *(it + 3),
            *(it + 4),
            *(it + 5),
            '\0'
          };

          *unicode_character = strtol(num, nullptr, 16);  // NOLINT
          *next = it + 6;
          return;
        }

        break;
    }
  }

  *unicode_character = *it;
  *next = it + 1;
}

// Converts escaped ASCII string to Java Unicode string. Escaping includes:
//    1. C-style escape codes: \r, \n, \\.
//    2. Octal escape codes: \3, \71, \152.
//    3. Uncode escape codes: \u883C.
static std::vector<jchar> UnescapeJavaString(
    const std::string& escaped_string) {
  std::vector<jchar> unicode_string;
  unicode_string.reserve(escaped_string.size());  // Pessimistic estimation.

  auto it = escaped_string.begin();
  while (it != escaped_string.end()) {
    jchar unicode_character = 0;
    std::string::const_iterator next;
    UnescapeCharacter(it, escaped_string.end(), &unicode_character, &next);

    unicode_string.push_back(unicode_character);
    it = next;
  }

  return unicode_string;
}

// Prints single expression to a stream.
static void SafePrintChild(
    std::ostream* os,
    JavaExpression* expression,
    bool concise) {
  DCHECK(expression != nullptr);
  if (expression == nullptr) {
    (*os) << "<NULL>";
  } else {
    expression->Print(os, concise);
  }
}


// Escapes and prints a single Java Unicode character. This function is only
// used for debugging purposes.
static void PrintCharacter(std::ostream* os, jchar ch) {
  if ((ch >= ' ') &&
      (ch < 127) &&
      (ch != '\\') &&
      (ch != '"') &&
      (ch != '\'') &&
      (ch != '\n') &&
      (ch != '\r') &&
      (ch != '\t') &&
      (ch != '\b')) {
    (*os) << static_cast<char>(ch);
  } else {  // Print in Unicode encoding
    (*os) << "\\u"
          << std::setfill('0') << std::setw(4)
          << std::hex << std::noshowbase << ch;
  }
}


ConditionalJavaExpression::ConditionalJavaExpression(
    JavaExpression* condition,
    JavaExpression* if_true,
    JavaExpression* if_false)
    : condition_(condition),
      if_true_(if_true),
      if_false_(if_false) {
}


void ConditionalJavaExpression::Print(std::ostream* os, bool concise) {
  if (!concise) {
    (*os) << '(';
  }

  SafePrintChild(os, condition_.get(), concise);
  (*os) << " ? ";
  SafePrintChild(os, if_true_.get(), concise);
  (*os) << " : ";
  SafePrintChild(os, if_false_.get(), concise);

  if (!concise) {
    (*os) << ')';
  }
}


CompiledExpression ConditionalJavaExpression::CreateEvaluator() {
  CompiledExpression comp_condition = condition_->CreateEvaluator();
  if (comp_condition.evaluator == nullptr) {
    return comp_condition;
  }

  CompiledExpression comp_if_true = if_true_->CreateEvaluator();
  if (comp_if_true.evaluator == nullptr) {
    return comp_if_true;
  }

  CompiledExpression comp_if_false = if_false_->CreateEvaluator();
  if (comp_if_false.evaluator == nullptr) {
    return comp_if_false;
  }

  return {
    std::unique_ptr<ExpressionEvaluator>(
        new ConditionalOperatorEvaluator(
            std::move(comp_condition.evaluator),
            std::move(comp_if_true.evaluator),
            std::move(comp_if_false.evaluator)))
  };
}


BinaryJavaExpression::BinaryJavaExpression(
    Type type,
    JavaExpression* a,
    JavaExpression* b)
    : type_(type),
      a_(a),
      b_(b) {
}


void BinaryJavaExpression::Print(std::ostream* os, bool concise) {
  if (!concise) {
    (*os) << '(';
  }

  SafePrintChild(os, a_.get(), concise);
  (*os) << ' ';

  switch (type_) {
    case Type::add:
      (*os) << '+';
      break;

    case Type::sub:
      (*os) << '-';
      break;

    case Type::mul:
      (*os) << '*';
      break;

    case Type::div:
      (*os) << '/';
      break;

    case Type::mod:
      (*os) << '%';
      break;

    case Type::conditional_and:
      (*os) << "&&";
      break;

    case Type::conditional_or:
      (*os) << "||";
      break;

    case Type::eq:
      (*os) << "==";
      break;

    case Type::ne:
      (*os) << "!=";
      break;

    case Type::le:
      (*os) << "<=";
      break;

    case Type::ge:
      (*os) << ">=";
      break;

    case Type::lt:
      (*os) << '<';
      break;

    case Type::gt:
      (*os) << '>';
      break;

    case Type::bitwise_and:
      (*os) << '&';
      break;

    case Type::bitwise_or:
      (*os) << '|';
      break;

    case Type::bitwise_xor:
      (*os) << '^';
      break;

    case Type::shl:
      (*os) << "<<";
      break;

    case Type::shr_s:
      (*os) << ">>";
      break;

    case Type::shr_u:
      (*os) << ">>>";
      break;
  }

  (*os) << ' ';
  SafePrintChild(os, b_.get(), concise);

  if (!concise) {
    (*os) << ')';
  }
}


CompiledExpression BinaryJavaExpression::CreateEvaluator() {
  CompiledExpression arg1 = a_->CreateEvaluator();
  if (arg1.evaluator == nullptr) {
    return arg1;
  }

  CompiledExpression arg2 = b_->CreateEvaluator();
  if (arg2.evaluator == nullptr) {
    return arg2;
  }

  return {std::unique_ptr<ExpressionEvaluator>(new BinaryExpressionEvaluator(
      type_, std::move(arg1.evaluator), std::move(arg2.evaluator)))};
}

void InstanceofBinaryJavaExpression::Print(std::ostream* os, bool concise) {
  if (!concise) {
    (*os) << '(';
  }

  SafePrintChild(os, source_.get(), concise);

  (*os) << " instanceof " << reference_type_;

  if (!concise) {
    (*os) << ')';
  }
}

CompiledExpression InstanceofBinaryJavaExpression::CreateEvaluator() {
  CompiledExpression arg = source_->CreateEvaluator();
  if (arg.evaluator == nullptr) {
    return arg;
  }

  return {std::unique_ptr<ExpressionEvaluator>(
      new InstanceofBinaryExpressionEvaluator(std::move(arg.evaluator),
                                              reference_type_))};
}

UnaryJavaExpression::UnaryJavaExpression(Type type, JavaExpression* a)
    : type_(type),
      a_(a) {
}


void UnaryJavaExpression::Print(std::ostream* os, bool concise) {
  switch (type_) {
    case Type::plus:
      (*os) << '+';
      break;

    case Type::minus:
      (*os) << '-';
      break;

    case Type::bitwise_complement:
      (*os) << '~';
      break;

    case Type::logical_complement:
      (*os) << '!';
      break;
  }

  SafePrintChild(os, a_.get(), concise);
}


CompiledExpression UnaryJavaExpression::CreateEvaluator() {
  CompiledExpression arg = a_->CreateEvaluator();
  if (arg.evaluator == nullptr) {
    return arg;
  }

  return {
    std::unique_ptr<ExpressionEvaluator>(
        new UnaryExpressionEvaluator(type_, std::move(arg.evaluator)))
  };
}

bool JavaIntLiteral::ParseString(const std::string& str, int base) {
  const char* node_cstr = str.c_str();
  char* literal_end = nullptr;

  errno = 0;
  n_ = strtoll(node_cstr, &literal_end, base);  // NOLINT
  if (errno == ERANGE) {
    LOG(WARNING) << "Number " << str << " in base " << base
                 << " could not be parsed";
    return false;
  }

  if (*literal_end == 'l' || *literal_end == 'L') {
    ++literal_end;
    is_long_ = true;
  }

  if (*literal_end != '\0') {
    LOG(WARNING) << "Unexpected trailing characters after number parser: "
                 << str;
    return false;
  }

  if (!is_long_) {
    if (static_cast<jint>(n_) != n_) {
      LOG(WARNING) << "Number can't be represented as jint: " << str;
      return false;
    }
  }

  return true;
}

void JavaIntLiteral::Print(std::ostream* os, bool concise) {
  if (!concise) {
    if (is_long_) {
      (*os) << "<long>";
    } else {
      (*os) << "<int>";
    }
  }

  (*os) << n_;

  if (is_long_) {
    (*os) << "L";
  }
}


CompiledExpression JavaIntLiteral::CreateEvaluator() {
  if (is_long_) {
    JVariant value = JVariant::Long(n_);
    return {
      std::unique_ptr<ExpressionEvaluator>(new LiteralEvaluator(&value))
    };
  } else {
    JVariant value = JVariant::Int(static_cast<jint>(n_));
    return {
      std::unique_ptr<ExpressionEvaluator>(new LiteralEvaluator(&value))
    };
  }
}

bool JavaFloatLiteral::ParseString(const std::string& str) {
  const char* node_cstr = str.c_str();
  char* literal_end = nullptr;

  d_ = strtod(node_cstr, &literal_end);

  if (*literal_end == 'f' || *literal_end == 'F') {
    ++literal_end;
    is_double_ = false;
  } else if (*literal_end == 'd' || *literal_end == 'D') {
    ++literal_end;
  }

  if (*literal_end != '\0') {
    return false;
  }

  return true;
}

void JavaFloatLiteral::Print(std::ostream* os, bool concise) {
  if (!concise) {
    if (!is_double_) {
      (*os) << "<float>";
    } else {
      (*os) << "<double>";
    }
  }

  (*os) << d_;

  if (!is_double_) {
    (*os) << "F";
  }
}


CompiledExpression JavaFloatLiteral::CreateEvaluator() {
  if (is_double_) {
    JVariant value = JVariant::Double(d_);
    return {
      std::unique_ptr<ExpressionEvaluator>(new LiteralEvaluator(&value))
    };
  } else {
    JVariant value = JVariant::Float(static_cast<jfloat>(d_));
    return {
      std::unique_ptr<ExpressionEvaluator>(new LiteralEvaluator(&value))
    };
  }
}

bool JavaCharLiteral::ParseString(const std::string& str) {
  std::vector<jchar> java_str = UnescapeJavaString(str);
  if (java_str.size() != 1) {
    return false;
  }

  ch_ = java_str[0];

  return true;
}

void JavaCharLiteral::Print(std::ostream* os, bool concise) {
  if (!concise) {
    (*os) << "<char>'";
  } else {
    (*os) << '\'';
  }

  PrintCharacter(os, ch_);

  if (!concise) {
    (*os) << "'";
  } else {
    (*os) << '\'';
  }
}


CompiledExpression JavaCharLiteral::CreateEvaluator() {
  JVariant value = JVariant::Char(ch_);
  return {
    std::unique_ptr<ExpressionEvaluator>(new LiteralEvaluator(&value))
  };
}

bool JavaStringLiteral::ParseString(const std::string& str) {
  str_ = UnescapeJavaString(str);
  return true;
}

void JavaStringLiteral::Print(std::ostream* os, bool concise) {
  (*os) << '"';

  for (jchar ch : str_) {
    PrintCharacter(os, ch);
  }

  (*os) << '"';
}


CompiledExpression JavaStringLiteral::CreateEvaluator() {
  return {
    std::unique_ptr<ExpressionEvaluator>(new StringEvaluator(str_))
  };
}


void JavaBooleanLiteral::Print(std::ostream* os, bool concise) {
  switch (n_) {
    case JNI_FALSE:
      (*os) << "false";
      break;

    case JNI_TRUE:
      (*os) << "true";
      break;

    default:
      (*os) << "bad_boolean";
      break;
  }
}


CompiledExpression JavaBooleanLiteral::CreateEvaluator() {
  JVariant value = JVariant::Boolean(n_);
  return {
    std::unique_ptr<ExpressionEvaluator>(new LiteralEvaluator(&value))
  };
}


void JavaNullLiteral::Print(std::ostream* os, bool concise) {
  (*os) << "null";
}


CompiledExpression JavaNullLiteral::CreateEvaluator() {
  JVariant value = JVariant::Null();
  return {
    std::unique_ptr<ExpressionEvaluator>(new LiteralEvaluator(&value))
  };
}


void JavaIdentifier::Print(std::ostream* os, bool concise) {
  if (concise) {
    (*os) << identifier_;
  } else {
    (*os) << '\'' << identifier_ << '\'';
  }
}


CompiledExpression JavaIdentifier::CreateEvaluator() {
  return {
    std::unique_ptr<ExpressionEvaluator>(new IdentifierEvaluator(identifier_))
  };
}

bool JavaIdentifier::TryGetTypeName(std::string* name) const {
  *name = identifier_;
  return true;
}

void TypeCastJavaExpression::Print(std::ostream* os, bool concise) {
  if (concise) {
    (*os) << '(' << type_ << ") ";
  } else {
    (*os) << "cast<" << type_ << ">(";
  }

  SafePrintChild(os, source_.get(), concise);

  if (!concise) {
    (*os) << ')';
  }
}


CompiledExpression TypeCastJavaExpression::CreateEvaluator() {
  CompiledExpression arg = source_->CreateEvaluator();
  if (arg.evaluator == nullptr) {
    return arg;
  }

  return {
    std::unique_ptr<ExpressionEvaluator>(
        new TypeCastOperatorEvaluator(std::move(arg.evaluator), type_))
  };
}


void JavaExpressionIndexSelector::Print(std::ostream* os, bool concise) {
  SafePrintChild(os, source_.get(), concise);

  (*os) << '[';
  SafePrintChild(os, index_.get(), concise);
  (*os) << ']';
}


CompiledExpression JavaExpressionIndexSelector::CreateEvaluator() {
  CompiledExpression source_evaluator = source_->CreateEvaluator();
  if (source_evaluator.evaluator == nullptr) {
    return source_evaluator;
  }

  CompiledExpression index_evaluator = index_->CreateEvaluator();
  if (index_evaluator.evaluator == nullptr) {
    return index_evaluator;
  }

  return {
    std::unique_ptr<ArrayExpressionEvaluator>(
        new ArrayExpressionEvaluator(
            std::move(source_evaluator.evaluator),
            std::move(index_evaluator.evaluator)))
  };
}


void JavaExpressionMemberSelector::Print(std::ostream* os, bool concise) {
  SafePrintChild(os, source_.get(), concise);

  (*os) << '.' << member_;
}


CompiledExpression JavaExpressionMemberSelector::CreateEvaluator() {
  CompiledExpression source_evaluator = source_->CreateEvaluator();
  if (source_evaluator.evaluator == nullptr) {
    return source_evaluator;
  }

  std::string possible_class_name;
  if (!source_->TryGetTypeName(&possible_class_name)) {
    possible_class_name.clear();
  }

  std::string identifier_name;
  if (!TryGetTypeName(&identifier_name)) {
    identifier_name = member_;
  }

  return {
    std::unique_ptr<ExpressionEvaluator>(
        new FieldEvaluator(
            std::move(source_evaluator.evaluator),
            std::move(identifier_name),
            std::move(possible_class_name),
            std::move(member_))),
  };
}

bool JavaExpressionMemberSelector::TryGetTypeName(std::string* name) const {
  if (!source_->TryGetTypeName(name)) {
    return false;
  }

  (*name) += '.';
  (*name) += member_;

  return true;
}

void MethodCallExpression::Print(std::ostream* os, bool concise) {
  if (!concise) {
    (*os) << "<call>( ";
  }

  if (source_ != nullptr) {
    SafePrintChild(os, source_.get(), concise);
    (*os) << '.';
  }

  (*os) << method_;

  (*os) << '(';

  if (arguments_ != nullptr) {
    bool first_argument = true;
    for (auto& it : *arguments_) {
      if (!first_argument) {
        (*os) << ", ";
      }

      SafePrintChild(os, it.get(), concise);

      first_argument = false;
    }
  } else {
    (*os) << "NULL";
  }

  (*os) << ')';

  if (!concise) {
    (*os) << " )";
  }
}


CompiledExpression MethodCallExpression::CreateEvaluator() {
  CompiledExpression source_evaluator;
  std::string possible_class_name;

  if (source_ != nullptr) {
    source_evaluator = source_->CreateEvaluator();
    if (source_evaluator.evaluator == nullptr) {
      return source_evaluator;
    }

    if (!source_->TryGetTypeName(&possible_class_name)) {
      VLOG(1) << "Couldn't retrieve type name, method: " << method_;
      possible_class_name.clear();
    }
  }

  std::vector<std::unique_ptr<ExpressionEvaluator>> argument_evaluators;
  for (std::unique_ptr<JavaExpression>& argument : *arguments_) {
    CompiledExpression argument_evaluator = argument->CreateEvaluator();
    if (argument_evaluator.evaluator == nullptr) {
      return argument_evaluator;
    }

    argument_evaluators.push_back(std::move(argument_evaluator.evaluator));
  }

  return {
    std::unique_ptr<ExpressionEvaluator>(
        new MethodCallEvaluator(
            method_,
            std::move(source_evaluator.evaluator),
            possible_class_name,
            std::move(argument_evaluators)))
  };
}


}  // namespace cdbg
}  // namespace devtools



