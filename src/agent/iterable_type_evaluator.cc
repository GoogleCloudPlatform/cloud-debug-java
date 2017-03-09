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

#include "iterable_type_evaluator.h"

#include "jni_proxy_iterable.h"
#include "messages.h"
#include "model.h"
#include "model_util.h"

namespace devtools {
namespace cdbg {

IterableTypeEvaluator::IterableTypeEvaluator()
    : iterable_iterator_(InstanceMethod(
          "Ljava/lang/Iterable;",
          "iterator",
          "()Ljava/util/Iterator;")),
      iterator_has_next_(InstanceMethod(
          "Ljava/util/Iterator;",
          "hasNext",
          "()Z")),
      iterator_next_(InstanceMethod(
          "Ljava/util/Iterator;",
          "next",
          "()Ljava/lang/Object;")) {
}


bool IterableTypeEvaluator::IsIterable(jclass cls) const {
  if (cls == nullptr) {
    return false;
  }

  return jni()->IsAssignableFrom(cls, jniproxy::Iterable()->GetClass());
}


void IterableTypeEvaluator::Evaluate(
    MethodCaller* method_caller,
    jobject obj,
    bool is_watch_expression,
    std::vector<NamedJVariant>* members) {
  members->clear();

  std::unique_ptr<FormatMessageModel> error_message;

  // Iterator<E> iterator = obj.iterator();
  ErrorOr<JVariant> iterator = method_caller->Invoke(
      iterable_iterator_,
      JVariant::BorrowedRef(obj),
      {});
  if (iterator.is_error()) {
    members->push_back(NamedJVariant::ErrorStatus(iterator.error_message()));
    return;
  }

  if (!iterator.value().has_non_null_object()) {
    // This is highly unlikely to happen. It indicates some very rudimentary
    // problem with the collection.
    members->push_back(
        NamedJVariant::ErrorStatus({ NullPointerDereference }));
    return;
  }

  // while (iterator.hasNext()) ...
  for (;;) {
    const int limit = is_watch_expression ?
        kMaxCaptureExpressionElements : kMaxCaptureObjectElements;
    if (members->size() >= limit) {
      members->push_back(NamedJVariant::InfoStatus({
        is_watch_expression ?
            ExpressionCollectionNotAllItemsCaptured :
            LocalCollectionNotAllItemsCaptured,
        { std::to_string(members->size()) }
      }));
      break;
    }

    ErrorOr<JVariant> has_next = method_caller->Invoke(
        iterator_has_next_,
        iterator.value(),
        {});
    if (has_next.is_error()) {
      members->push_back(
          NamedJVariant::ErrorStatus(has_next.error_message()));
      break;
    }

    jboolean has_next_bool = false;
    has_next.value().get<jboolean>(&has_next_bool);
    if (!has_next_bool) {
      // Enumeration of all collection items completed successfully.
      break;
    }

    // E item = iterator.next();
    ErrorOr<JVariant> next = method_caller->Invoke(
        iterator_next_,
        iterator.value(),
        {});
    if (next.is_error()) {
      members->push_back(NamedJVariant::ErrorStatus(next.error_message()));
      break;
    }

    NamedJVariant item;
    item.name = FormatArrayIndexName(members->size());
    item.value = ErrorOr<JVariant>::detach_value(std::move(next));
    item.value.change_ref_type(JVariant::ReferenceKind::Global);

    members->push_back(std::move(item));
  }

  if (members->empty()) {
    members->push_back(NamedJVariant::InfoStatus({ EmptyCollection }));
  }
}

}  // namespace cdbg
}  // namespace devtools


