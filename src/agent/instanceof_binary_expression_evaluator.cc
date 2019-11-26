#include "instanceof_binary_expression_evaluator.h"

#include "class_indexer.h"
#include "common.h"
#include "jvmti_buffer.h"
#include "messages.h"
#include "model.h"
#include "numeric_cast_evaluator.h"
#include "readers_factory.h"
#include "type_util.h"

namespace devtools {
namespace cdbg {

InstanceofBinaryExpressionEvaluator::InstanceofBinaryExpressionEvaluator(
    std::unique_ptr<ExpressionEvaluator> source,
    const std::string& reference_type)
    : source_(std::move(source)),
      reference_type_(reference_type),
      reference_class_(nullptr),
      computer_(nullptr) {
  result_type_.type = JType::Boolean;
}

InstanceofBinaryExpressionEvaluator::~InstanceofBinaryExpressionEvaluator() {
  if (reference_class_ != nullptr) {
    jni()->DeleteGlobalRef(reference_class_);
    reference_class_ = nullptr;
  }
}

bool InstanceofBinaryExpressionEvaluator::Compile(
    ReadersFactory* readers_factory, FormatMessageModel* error_message) {
  if (!source_->Compile(readers_factory, error_message)) {
    return false;
  }

  // If the left operand is not a reference type, stop the evaluation.
  if (source_->GetStaticType().type != JType::Object) {
    *error_message = {ReferenceTypeNotFound,
                      {TypeNameFromSignature(source_->GetStaticType())}};
    return false;
  }

  JniLocalRef reference_type_local_ref =
      readers_factory->FindClassByName(reference_type_, error_message);
  if (reference_type_local_ref == nullptr) {
    return false;
  }

  computer_ = &InstanceofBinaryExpressionEvaluator::InstanceofComputer;
  reference_class_ = jni()->NewGlobalRef(reference_type_local_ref.get());

  JvmtiBuffer<char> signature;
  if (jvmti()->GetClassSignature(static_cast<jclass>(reference_class_),
                                 signature.ref(),
                                 nullptr) != JVMTI_ERROR_NONE) {
    *error_message = INTERNAL_ERROR_MESSAGE;
    return false;
  }

  return true;
}

ErrorOr<JVariant> InstanceofBinaryExpressionEvaluator::Evaluate(
    const EvaluationContext& evaluation_context) const {
  ErrorOr<JVariant> source_result = source_->Evaluate(evaluation_context);
  if (source_result.is_error()) {
    return source_result;
  }

  return (this->*computer_)(source_result.value());
}

ErrorOr<JVariant> InstanceofBinaryExpressionEvaluator::InstanceofComputer(
    const JVariant& source) const {
  jobject source_value = nullptr;
  if (!source.get<jobject>(&source_value)) {
    LOG(ERROR) << "Couldn't extract the source value as an Object: "
               << source.ToString(false);
    return INTERNAL_ERROR_MESSAGE;
  }

  return JVariant::Boolean(
      jni()->IsInstanceOf(source_value, static_cast<jclass>(reference_class_)));
}

}  // namespace cdbg
}  // namespace devtools
