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

#include "src/agent/format_queue.h"

#include "gtest/gtest.h"
#include "src/agent/model_util.h"
#include "src/agent/statistician.h"
#include "tests/agent/fake_jni.h"
#include "tests/agent/mock_jvmti_env.h"

namespace devtools {
namespace cdbg {

class FormatQueueTest : public ::testing::Test {
 protected:
  FormatQueueTest() : global_jvm_(fake_jni_.jvmti(), fake_jni_.jni()) {}

  void SetUp() override {
    InitializeStatisticians();
  }

  void TearDown() override {
    CleanupStatisticians();
  }

 protected:
  FakeJni fake_jni_;
  GlobalJvmEnv global_jvm_;
};


TEST_F(FormatQueueTest, IncorrectPop) {
  FormatQueue format_queue;
  EXPECT_EQ(nullptr, format_queue.FormatAndPop());
}


TEST_F(FormatQueueTest, EnqueueAndDequeue) {
  std::unique_ptr<BreakpointModel> breakpoint(new BreakpointModel);
  BreakpointModel* breakpoint_ptr = breakpoint.get();

  FormatQueue format_queue;
  format_queue.Enqueue(std::move(breakpoint), nullptr);
  EXPECT_EQ(breakpoint_ptr, format_queue.FormatAndPop().get());
  EXPECT_EQ(nullptr, format_queue.FormatAndPop());
}


TEST_F(FormatQueueTest, MaxLimit) {
  FormatQueue format_queue;
  for (int i = 0; i < kMaxFormatQueueSize + 10; ++i) {
    std::unique_ptr<BreakpointModel> breakpoint(new BreakpointModel);
    breakpoint->id = "ID" + std::to_string(i);

    format_queue.Enqueue(std::move(breakpoint), nullptr);
  }

  int count = 0;
  while (format_queue.FormatAndPop() != nullptr) {
    ++count;
  }

  EXPECT_EQ(kMaxFormatQueueSize, count);
}


TEST_F(FormatQueueTest, ExpressionNames_Copy) {
  std::unique_ptr<BreakpointModel> breakpoint = BreakpointBuilder()
      .set_id("ID")
      .set_expressions({ "1+1", "2+2", "3+3" })
      .add_evaluated_expression(VariableBuilder().set_value("2"))
      .add_evaluated_expression(VariableBuilder().set_value("4"))
      .add_evaluated_expression(VariableBuilder().set_value("6"))
      .build();

  FormatQueue format_queue;
  format_queue.Enqueue(std::move(breakpoint), nullptr);

  breakpoint = format_queue.FormatAndPop();
  EXPECT_EQ(3, breakpoint->evaluated_expressions.size());
  EXPECT_EQ("1+1", breakpoint->evaluated_expressions[0]->name);
  EXPECT_EQ("2+2", breakpoint->evaluated_expressions[1]->name);
  EXPECT_EQ("3+3", breakpoint->evaluated_expressions[2]->name);
}


TEST_F(FormatQueueTest, ExpressionNames_NoEvaluatedExpressions) {
  std::unique_ptr<BreakpointModel> breakpoint = BreakpointBuilder()
      .set_id("ID")
      .set_expressions({ "1+1", "2+2", "3+3" })
      .build();

  FormatQueue format_queue;
  format_queue.Enqueue(std::move(breakpoint), nullptr);

  breakpoint = format_queue.FormatAndPop();
  EXPECT_EQ(0, breakpoint->evaluated_expressions.size());
}


TEST_F(FormatQueueTest, RemoveAll) {
  std::unique_ptr<BreakpointModel> breakpoint(new BreakpointModel);

  FormatQueue format_queue;
  format_queue.Enqueue(std::move(breakpoint), nullptr);

  format_queue.RemoveAll();

  EXPECT_EQ(nullptr, format_queue.FormatAndPop());
}


TEST_F(FormatQueueTest, EnqueueEvent) {
  FormatQueue format_queue;

  int events_counter = 0;
  auto cookie = format_queue.SubscribeOnItemEnqueuedEvents(
      [&events_counter]() { ++events_counter; });

  for (int i = 0; i < 7; ++i) {
    std::unique_ptr<BreakpointModel> breakpoint(new BreakpointModel);
    breakpoint->id = "ID" + std::to_string(i);

    format_queue.Enqueue(std::move(breakpoint), nullptr);
  }

  format_queue.UnsubscribeOnItemEnqueuedEvents(std::move(cookie));

  format_queue.Enqueue(
      std::unique_ptr<BreakpointModel>(new BreakpointModel),
      nullptr);

  format_queue.RemoveAll();

  EXPECT_EQ(7, events_counter);
}


TEST_F(FormatQueueTest, RepeatedEnqueueNonFinalState) {
  FormatQueue format_queue;

  for (int i = 0; i < 1000; ++i) {
    format_queue.Enqueue(
        BreakpointBuilder()
            .set_id("ID")
            .set_location("path", i)
            .build(),
        nullptr);
  }

  std::unique_ptr<BreakpointModel> breakpoint = format_queue.FormatAndPop();
  EXPECT_NE(nullptr, breakpoint);
  EXPECT_EQ(999, breakpoint->location->line);

  EXPECT_EQ(nullptr, format_queue.FormatAndPop());
}


TEST_F(FormatQueueTest, RepeatedEnqueueNonFinalStateReplacedByFinal) {
  FormatQueue format_queue;

  format_queue.Enqueue(
      BreakpointBuilder()
          .set_id("ID")
          .set_location("interim", 0)
          .build(),
      nullptr);

  format_queue.Enqueue(
      BreakpointBuilder()
          .set_id("ID")
          .set_is_final_state(true)
          .set_location("final", 0)
          .build(),
      nullptr);

  std::unique_ptr<BreakpointModel> breakpoint = format_queue.FormatAndPop();
  EXPECT_NE(nullptr, breakpoint);
  EXPECT_EQ("final", breakpoint->location->path);

  EXPECT_EQ(nullptr, format_queue.FormatAndPop());
}


TEST_F(FormatQueueTest, RepeatedEnqueueFinalState) {
  FormatQueue format_queue;

  for (int i = 0; i < 1000; ++i) {
    std::unique_ptr<BreakpointModel> breakpoint(new BreakpointModel);
    breakpoint->id = "ID";
    breakpoint->is_final_state = true;

    format_queue.Enqueue(std::move(breakpoint), nullptr);
  }

  EXPECT_NE(nullptr, format_queue.FormatAndPop());
  EXPECT_EQ(nullptr, format_queue.FormatAndPop());
}


}  // namespace cdbg
}  // namespace devtools

