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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_READERS_FACTORY_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_READERS_FACTORY_H_

#include "gmock/gmock.h"

#include "array_reader.h"
#include "fake_instance_field_reader.h"
#include "fake_local_variable_reader.h"
#include "fake_static_field_reader.h"
#include "readers_factory.h"


namespace devtools {
namespace cdbg {

class MockReadersFactory : public ReadersFactory {
 public:
  MOCK_METHOD(std::string, GetEvaluationPointClassName, (), (override));

  MOCK_METHOD(std::unique_ptr<LocalVariableReader>, CreateLocalVariableReader,
              (const std::string& variable_name,
               FormatMessageModel* error_message),
              (override));

  MOCK_METHOD(std::unique_ptr<LocalVariableReader>, CreateLocalInstanceReader,
              (), (override));

  MOCK_METHOD(JniLocalRef, FindClassByName,
              (const std::string&, FormatMessageModel*), (override));

  MOCK_METHOD(bool, IsAssignable, (const std::string&, const std::string&),
              (override));

  MOCK_METHOD(std::unique_ptr<InstanceFieldReader>, CreateInstanceFieldReader,
              (const std::string& class_signature,
               const std::string& field_name,
               FormatMessageModel* error_message),
              (override));

  MOCK_METHOD(std::unique_ptr<StaticFieldReader>, CreateStaticFieldReader,
              (const std::string& field_name,
               FormatMessageModel* error_message),
              (override));

  MOCK_METHOD(std::unique_ptr<StaticFieldReader>, CreateStaticFieldReader,
              (const std::string& class_name, const std::string& field_name,
               FormatMessageModel* error_message),
              (override));

  MOCK_METHOD(std::vector<ClassMetadataReader::Method>,
              FindLocalInstanceMethods, (const std::string& method_name),
              (override));

  MOCK_METHOD(bool, FindInstanceMethods,
              (const std::string& class_signature,
               const std::string& method_name,
               std::vector<ClassMetadataReader::Method>* methods,
               FormatMessageModel* error_message),
              (override));

  MOCK_METHOD(std::vector<ClassMetadataReader::Method>, FindStaticMethods,
              (const std::string& method_name), (override));

  MOCK_METHOD(bool, FindStaticMethods,
              (const std::string& class_name, const std::string& method_name,
               std::vector<ClassMetadataReader::Method>* methods,
               FormatMessageModel* error_message),
              (override));

  MOCK_METHOD(std::unique_ptr<ArrayReader>, CreateArrayReader,
              (const JSignature& array_signature), (override));

  MOCK_METHOD(bool, IsMethodCallAllowed, (const ClassMetadataReader::Method&));

  MOCK_METHOD(bool, IsSafeIterable, (const std::string&));

  //
  // Helper methods to set up expectations for this mock.
  //

  void SetUpDefault() {
    EXPECT_CALL(*this,
                CreateLocalVariableReader(testing::_, testing::NotNull()))
        .WillRepeatedly(testing::Invoke(
            [](const std::string& name, FormatMessageModel* error_message) {
              return nullptr;
            }));

    EXPECT_CALL(*this, CreateLocalInstanceReader())
        .WillRepeatedly(testing::Invoke([] () {
          return nullptr;
        }));

    EXPECT_CALL(*this, CreateInstanceFieldReader(testing::_, testing::_,
                                                 testing::NotNull()))
        .WillRepeatedly(testing::Invoke(
            [](const std::string& class_signature,
               const std::string& field_name,
               FormatMessageModel* error_message) { return nullptr; }));

    EXPECT_CALL(*this, CreateStaticFieldReader(testing::_, testing::NotNull()))
        .WillRepeatedly(testing::Invoke(
            [](const std::string& field_name,
               FormatMessageModel* error_message) { return nullptr; }));

    EXPECT_CALL(*this, CreateStaticFieldReader(testing::_, testing::_,
                                               testing::NotNull()))
        .WillRepeatedly(testing::Invoke(
            [](const std::string& class_signature,
               const std::string& field_name,
               FormatMessageModel* error_message) { return nullptr; }));

    EXPECT_CALL(*this, FindLocalInstanceMethods(testing::_))
        .WillRepeatedly(testing::Return(
            std::vector<ClassMetadataReader::Method>()));

    EXPECT_CALL(
        *this,
        FindInstanceMethods(testing::_, testing::_, testing::_, testing::_))
        .WillRepeatedly(testing::Return(true));

    EXPECT_CALL(*this, FindStaticMethods(testing::_))
        .WillRepeatedly(testing::Return(
            std::vector<ClassMetadataReader::Method>()));

    EXPECT_CALL(
        *this,
        FindStaticMethods(testing::_, testing::_, testing::_, testing::_))
        .WillRepeatedly(testing::Return(true));
  }

  // Sets up expectation for a fake numeric local variable.
  template <typename T>
  void AddFakeLocal(const std::string& name, T value) {
    EXPECT_CALL(*this, CreateLocalVariableReader(testing::StrEq(name),
                                                 testing::NotNull()))
        .WillRepeatedly(testing::Invoke([this, value](
                                            const std::string& name,
                                            FormatMessageModel* error_message) {
          JVariant var = JVariant::Primitive<T>(value);
          return FakeLocalVariableReader::CreateLocal(
              name,
              { var.type() },
              var);
        }));
  }

  // Sets up expectation for a fake object variable. "value" has to be
  // available throughout the test case.
  void AddFakeLocal(const std::string& name,
                    const std::string& object_signature,
                    const JVariant& value) {
    EXPECT_CALL(*this, CreateLocalVariableReader(testing::StrEq(name),
                                                 testing::NotNull()))
        .WillRepeatedly(testing::Invoke([this, object_signature, &value](
                                            const std::string& name,
                                            FormatMessageModel* error_message) {
          return FakeLocalVariableReader::CreateLocal(
              name,
              { value.type(), object_signature },
              value);
        }));
  }

  // Sets up expectation for a fake null object variable.
  void AddFakeLocal(const std::string& name,
                    const std::string& object_signature, std::nullptr_t) {
    EXPECT_CALL(*this, CreateLocalVariableReader(testing::StrEq(name),
                                                 testing::NotNull()))
        .WillRepeatedly(testing::Invoke([this, object_signature](
                                            const std::string& name,
                                            FormatMessageModel* error_message) {
          return FakeLocalVariableReader::CreateLocal(
              name,
              { JType::Object, object_signature },
              JVariant::Null());
        }));
  }

  // Sets up expectation for a fake local instance object. "value" has to
  // be available throughout the test case.
  void SetFakeLocalInstance(const std::string& object_signature,
                            JVariant* value) {
    EXPECT_CALL(*this, CreateLocalInstanceReader())
        .WillRepeatedly(testing::Invoke([this, object_signature, value] () {
          return FakeLocalVariableReader::CreateLocal(
              "unused",
              { value->type(), object_signature },
              *value);
        }));
  }

  // Sets up expectation for a fake numeric instance field.
  template <typename T>
  void AddFakeInstanceField(const std::string& class_signature,
                            const std::string& field_name, T value) {
    EXPECT_CALL(*this, CreateInstanceFieldReader(
                           testing::StrEq(class_signature),
                           testing::StrEq(field_name), testing::NotNull()))
        .WillRepeatedly(testing::Invoke([this, value](
                                            const std::string& class_signature,
                                            const std::string& field_name,
                                            FormatMessageModel* error_message) {
          JVariant var = JVariant::Primitive<T>(value);
          return FakeInstanceFieldReader::Create(
              field_name,
              { var.type() },
              var);
        }));
  }

  // Sets up expectation for a fake object instance field. "value" has to be
  // available throughout the test case.
  void AddFakeInstanceField(const std::string& class_signature,
                            const std::string& field_name,
                            const std::string& field_object_signature,
                            const JVariant& value) {
    EXPECT_CALL(*this, CreateInstanceFieldReader(
                           testing::StrEq(class_signature),
                           testing::StrEq(field_name), testing::NotNull()))
        .WillRepeatedly(testing::Invoke([this, &value, field_object_signature](
                                            const std::string& class_signature,
                                            const std::string& field_name,
                                            FormatMessageModel* error_message) {
          return FakeInstanceFieldReader::Create(
              field_name,
              { value.type(), field_object_signature },
              value);
        }));
  }

  // Sets up expectation for a fake numeric instance field.
  template <typename T>
  void AddFakeStaticField(const std::string& class_signature,
                          const std::string& field_name, T value) {
    EXPECT_CALL(*this, CreateStaticFieldReader(testing::StrEq(class_signature),
                                               testing::StrEq(field_name),
                                               testing::NotNull()))
        .WillRepeatedly(testing::Invoke([this, value](
                                            const std::string& class_signature,
                                            const std::string& field_name,
                                            FormatMessageModel* error_message) {
          JVariant var = JVariant::Primitive<T>(value);
          return FakeStaticFieldReader::Create(
              field_name,
              { var.type() },
              var);
        }));
  }

  template <typename T>
  void AddFakeStaticField(const std::string& field_name, T value) {
    EXPECT_CALL(*this, CreateStaticFieldReader(testing::StrEq(field_name),
                                               testing::NotNull()))
        .WillRepeatedly(testing::Invoke([this, value](
                                            const std::string& field_name,
                                            FormatMessageModel* error_message) {
          JVariant var = JVariant::Primitive<T>(value);
          return FakeStaticFieldReader::Create(
              field_name,
              { var.type() },
              var);
        }));
  }

  // Sets up expectation for a fake object instance field. "value" has to be
  // available throughout the test case.
  void AddFakeStaticField(const std::string& class_signature,
                          const std::string& field_name,
                          const std::string& field_object_signature,
                          const JVariant& value) {
    EXPECT_CALL(*this, CreateStaticFieldReader(testing::StrEq(class_signature),
                                               testing::StrEq(field_name),
                                               testing::NotNull()))
        .WillRepeatedly(testing::Invoke([this, &value, field_object_signature](
                                            const std::string& class_signature,
                                            const std::string& field_name,
                                            FormatMessageModel* error_message) {
          return FakeStaticFieldReader::Create(
              field_name,
              { value.type(), field_object_signature },
              value);
        }));
  }

  void AddFakeStaticField(const std::string& field_name,
                          const std::string& field_object_signature,
                          const JVariant& value) {
    EXPECT_CALL(*this, CreateStaticFieldReader(testing::StrEq(field_name),
                                               testing::NotNull()))
        .WillRepeatedly(testing::Invoke([this, &value, field_object_signature](
                                            const std::string& field_name,
                                            FormatMessageModel* error_message) {
          return FakeStaticFieldReader::Create(
              field_name,
              { value.type(), field_object_signature },
              value);
        }));
  }
};

}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_MOCK_READERS_FACTORY_H_

