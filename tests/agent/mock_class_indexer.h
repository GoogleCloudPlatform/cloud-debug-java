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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_CLASS_INDEXER_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_CLASS_INDEXER_H_

#include "gmock/gmock.h"
#include "src/agent/class_indexer.h"

namespace devtools {
namespace cdbg {

class MockClassIndexer : public ClassIndexer {
 public:
  OnClassPreparedEvent::Cookie SubscribeOnClassPreparedEvents(
      OnClassPreparedEvent::Callback fn) override {
    return on_class_prepared_.Subscribe(fn);
  }

  void UnsubscribeOnClassPreparedEvents(
      OnClassPreparedEvent::Cookie cookie) override {
    on_class_prepared_.Unsubscribe(std::move(cookie));
  }

  MOCK_METHOD(JniLocalRef, FindClassBySignature, (const std::string&),
              (override));

  MOCK_METHOD(JniLocalRef, FindClassByName, (const std::string&), (override));

  MOCK_METHOD(std::shared_ptr<Type>, GetPrimitiveType, (JType), (override));

  MOCK_METHOD(std::shared_ptr<Type>, GetReference, (const std::string&),
              (override));

  void FireOnClassPrepared(const std::string& type_name,
                           const std::string& class_signature) {
    return on_class_prepared_.Fire(type_name, class_signature);
  }

 private:
  OnClassPreparedEvent on_class_prepared_;
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_CLASS_INDEXER_H_
