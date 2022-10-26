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

#include "src/agent/statistician.h"

#include <cmath>
#include <cstdint>

#include "gtest/gtest.h"

namespace devtools {
namespace cdbg {

static constexpr double kEpsilon = 1.0E-8;

class StatisticianTest : public ::testing::Test {
 protected:
  void SetUp() override {
    InitializeStatisticians();
  }

  void TearDown() override {
    CleanupStatisticians();
  }
};


TEST(StatisticianTest, Name) {
  Statistician s("zebra");
  EXPECT_EQ(std::string("zebra"), s.name());
}

TEST(StatisticianTest, Empty) {
  Statistician s("");

  EXPECT_EQ(0, s.count());
  EXPECT_TRUE(std::fabs(s.min() - (-1)) < kEpsilon);
  EXPECT_TRUE(std::fabs(s.max() - (-1)) < kEpsilon);
  EXPECT_TRUE(std::fabs(s.mean() - (-1)) < kEpsilon);
  EXPECT_TRUE(std::fabs(s.stdev() - (-1)) < kEpsilon);
}

TEST(StatisticianTest, Add) {
  Statistician s("");
  s.add(4);
  s.add(5);
  s.add(6);

  EXPECT_EQ(3, s.count());
  EXPECT_TRUE(std::fabs(s.min() - 4) < kEpsilon);
  EXPECT_TRUE(std::fabs(s.max() - 6) < kEpsilon);
  EXPECT_TRUE(std::fabs(s.mean() - 5) < kEpsilon);
  EXPECT_TRUE(std::fabs(s.stdev() - 0.81649658092772681) < kEpsilon)
      << s.stdev();
}

TEST(StatisticianTest, ScopedStat) {
  int64_t time_us = 0;
  std::function<int(struct timespec*)> fn_gettime =
      [&time_us](struct timespec* t) {
    t->tv_sec = time_us / 1000;
    t->tv_nsec = time_us * 1000;
    return 0;
  };

  Statistician s("");

  {
    time_us = 0;
    ScopedStat ss(&s, fn_gettime);
    time_us = 4;
  }

  {
    time_us = 0;
    ScopedStat ss(&s, fn_gettime);
    time_us = 5;
  }

  {
    time_us = 0;
    ScopedStat ss(&s, fn_gettime);
    time_us = 6;
  }

  EXPECT_EQ(3, s.count());
  EXPECT_TRUE(std::fabs(s.min() - 4) < kEpsilon);
  EXPECT_TRUE(std::fabs(s.max() - 6) < kEpsilon);
  EXPECT_TRUE(std::fabs(s.mean() - 5) < kEpsilon);
  EXPECT_TRUE(std::fabs(s.stdev() - 0.81649658092772681) < kEpsilon)
      << s.stdev();
}

}  // namespace cdbg
}  // namespace devtools

