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

#include "scheduler.h"

#include "gtest/gtest.h"

namespace devtools {
namespace cdbg {

class SchedulerTest : public ::testing::Test {
 protected:
  class Counter {
   public:
    int value() const { return value_; }

    void Increment() {
      ++value_;
    }

   private:
    int value_ { 0 };
  };


  class Unexpected {
   public:
    void Do() {
      FAIL() << "Unexpected";
    }
  };


  class ActionDuringFire {
   public:
    explicit ActionDuringFire(std::function<void()> action) : action_(action) {}

    void Do() {
      action_();
    }

   private:
    std::function<void()> action_;
  };

 protected:
  time_t clock_ { 0 };
  const std::function<time_t()> fake_clock_ { [this]() { return clock_; } };
};


TEST_F(SchedulerTest, Empty) {
  Scheduler<> scheduler(fake_clock_);
}


TEST_F(SchedulerTest, NotTimeYet) {
  Scheduler<> scheduler(fake_clock_);

  auto unexpected = std::make_shared<Unexpected>();
  scheduler.Schedule(
      10,
      std::weak_ptr<Unexpected>(unexpected),
      &Unexpected::Do);

  clock_ = 9;
  scheduler.Process();
}


TEST_F(SchedulerTest, Fire) {
  Scheduler<> scheduler(fake_clock_);

  auto counter = std::make_shared<Counter>();
  scheduler.Schedule(10, std::weak_ptr<Counter>(counter), &Counter::Increment);
  scheduler.Schedule(11, std::weak_ptr<Counter>(counter), &Counter::Increment);
  scheduler.Schedule(11, std::weak_ptr<Counter>(counter), &Counter::Increment);
  scheduler.Schedule(11, std::weak_ptr<Counter>(counter), &Counter::Increment);
  scheduler.Schedule(15, std::weak_ptr<Counter>(counter), &Counter::Increment);

  clock_ = 11;
  scheduler.Process();

  EXPECT_EQ(4, counter->value());

  scheduler.Process();
  scheduler.Process();

  EXPECT_EQ(4, counter->value());
}


TEST_F(SchedulerTest, Expiration) {
  Scheduler<> scheduler(fake_clock_);

  auto counter = std::make_shared<Counter>();
  scheduler.Schedule(10, std::weak_ptr<Counter>(counter), &Counter::Increment);

  clock_ = 1000;
  scheduler.Process();
  scheduler.Process();
  scheduler.Process();

  EXPECT_EQ(1, counter->value());
}


TEST_F(SchedulerTest, Cancellation) {
  Scheduler<> scheduler(fake_clock_);

  auto counter = std::make_shared<Counter>();

  scheduler.Schedule(9, std::weak_ptr<Counter>(counter), &Counter::Increment);
  auto id = scheduler.Schedule(
      10,
      std::weak_ptr<Counter>(counter),
      &Counter::Increment);
  scheduler.Schedule(10, std::weak_ptr<Counter>(counter), &Counter::Increment);
  scheduler.Schedule(10, std::weak_ptr<Counter>(counter), &Counter::Increment);

  EXPECT_TRUE(scheduler.Cancel(id));

  clock_ = 9;
  scheduler.Process();
  EXPECT_EQ(1, counter->value());

  clock_ = 10;
  scheduler.Process();
  EXPECT_EQ(3, counter->value());
}


TEST_F(SchedulerTest, DoubleCancellation) {
  Scheduler<> scheduler(fake_clock_);

  auto unexpected = std::make_shared<Unexpected>();
  auto id = scheduler.Schedule(
      10,
      std::weak_ptr<Unexpected>(unexpected),
      &Unexpected::Do);

  EXPECT_TRUE(scheduler.Cancel(id));
  EXPECT_FALSE(scheduler.Cancel(id));
}


TEST_F(SchedulerTest, NullIdCancellation) {
  Scheduler<> scheduler(fake_clock_);

  auto counter = std::make_shared<Counter>();
  scheduler.Schedule(0, std::weak_ptr<Counter>(counter), &Counter::Increment);

  EXPECT_FALSE(scheduler.Cancel(Scheduler<>::NullId));

  scheduler.Process();
  EXPECT_EQ(1, counter->value());
}


TEST_F(SchedulerTest, CancellationAfterFire) {
  Scheduler<> scheduler(fake_clock_);

  auto counter = std::make_shared<Counter>();
  auto id = scheduler.Schedule(
      10,
      std::weak_ptr<Counter>(counter),
      &Counter::Increment);

  clock_ = 10;
  scheduler.Process();

  EXPECT_EQ(1, counter->value());

  EXPECT_FALSE(scheduler.Cancel(id));
}


TEST_F(SchedulerTest, CancellationDuringFire) {
  Scheduler<> scheduler(fake_clock_);

  int counter = 0;
  Scheduler<>::Id id;
  auto target = std::make_shared<ActionDuringFire>(
      [&counter, &scheduler, &id]() {
    ++counter;
    EXPECT_FALSE(scheduler.Cancel(id));
  });

  id = scheduler.Schedule(
      10,
      std::weak_ptr<ActionDuringFire>(target),
      &ActionDuringFire::Do);

  clock_ = 10;
  scheduler.Process();

  EXPECT_EQ(1, counter);
}


TEST_F(SchedulerTest, ScheduleDuringFire) {
  Scheduler<> scheduler(fake_clock_);

  int counter = 0;
  auto increment_target = std::make_shared<ActionDuringFire>([&counter] {
    ++counter;
  });
  auto schedule_target = std::make_shared<ActionDuringFire>(
      [increment_target, &counter, &scheduler]() {
    ++counter;
    scheduler.Schedule(
        10,
        std::weak_ptr<ActionDuringFire>(increment_target),
        &ActionDuringFire::Do);
  });

  scheduler.Schedule(
      10,
      std::weak_ptr<ActionDuringFire>(schedule_target),
      &ActionDuringFire::Do);

  clock_ = 10;

  scheduler.Process();
  EXPECT_EQ(1, counter);

  scheduler.Process();
  EXPECT_EQ(2, counter);
}


TEST_F(SchedulerTest, FireExpiredObject) {
  Scheduler<> scheduler(fake_clock_);

  int counter = 0;
  auto increment_target = std::make_shared<ActionDuringFire>([&counter] {
    ++counter;
  });

  scheduler.Schedule(
      10,
      std::weak_ptr<ActionDuringFire>(increment_target),
      &ActionDuringFire::Do);

  scheduler.Schedule(
      20,
      std::weak_ptr<ActionDuringFire>(increment_target),
      &ActionDuringFire::Do);

  clock_ = 11;
  scheduler.Process();

  EXPECT_EQ(1, counter);

  increment_target = nullptr;

  clock_ = 21;
  scheduler.Process();

  EXPECT_EQ(1, counter);
}


TEST_F(SchedulerTest, Parameter) {
  class Parameterized {
   public:
    int counter() const { return counter_; }

    void Do(int a, std::string b, void* c) {
      EXPECT_EQ(23875, a);
      EXPECT_EQ("skdfjhbsdfg", b);
      EXPECT_EQ(nullptr, c);

      ++counter_;
    }

   private:
    int counter_ { 0 };
  };

  Scheduler<int, std::string, void*> scheduler(fake_clock_);

  auto parameterized = std::make_shared<Parameterized>();
  scheduler.Schedule(
      10,
      std::weak_ptr<Parameterized>(parameterized),
      &Parameterized::Do);

  clock_ = 11;
  scheduler.Process(23875, "skdfjhbsdfg", nullptr);

  EXPECT_EQ(1, parameterized->counter());
}


}  // namespace cdbg
}  // namespace devtools
