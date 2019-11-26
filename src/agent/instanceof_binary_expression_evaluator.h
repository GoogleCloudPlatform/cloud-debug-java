#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_INSTANCEOF_BINARY_EXPRESSION_EVALUATOR_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_INSTANCEOF_BINARY_EXPRESSION_EVALUATOR_H_

#include "common.h"
#include "expression_evaluator.h"
#include "java_expression.h"

namespace devtools {
namespace cdbg {

// Implements Java binary instanceof operators.
class InstanceofBinaryExpressionEvaluator : public ExpressionEvaluator {
 public:
  // Class constructor. The instance will own "source".
  InstanceofBinaryExpressionEvaluator(
      std::unique_ptr<ExpressionEvaluator> source,
      const std::string& reference_type);

  ~InstanceofBinaryExpressionEvaluator() override;

  // Disallow copying and assignment.
  InstanceofBinaryExpressionEvaluator(
      const InstanceofBinaryExpressionEvaluator&) = delete;
  InstanceofBinaryExpressionEvaluator& operator=(
      const InstanceofBinaryExpressionEvaluator&) = delete;

  bool Compile(ReadersFactory* readers_factory,
               FormatMessageModel* error_message) override;

  const JSignature& GetStaticType() const override { return result_type_; }

  Nullable<jvalue> GetStaticValue() const override { return nullptr; }

  ErrorOr<JVariant> Evaluate(
      const EvaluationContext& evaluation_context) const override;

 private:
  // Evaluation method to decide if a object is an instance of a class type.
  ErrorOr<JVariant> InstanceofComputer(const JVariant& source) const;

  // Compiled expression corresponding to the object being checked.
  std::unique_ptr<ExpressionEvaluator> source_;

  // Statically computed resulting type of the expression.
  JSignature result_type_;

  // Class type which is checked against.
  const std::string reference_type_;

  // Class which is derived from reference_type_.
  jobject reference_class_;

  // Pointer to a member function of this class to do the actual evaluation
  // of the binary expression.
  ErrorOr<JVariant> (InstanceofBinaryExpressionEvaluator::*computer_)(
      const JVariant&) const;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_INSTANCEOF_BINARY_EXPRESSION_EVALUATOR_H_
