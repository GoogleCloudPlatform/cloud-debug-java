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

#include "observable.h"

#include "gtest/gtest.h"

namespace devtools {
namespace cdbg {

TEST(ObservableTest, FireEmpty) {
  Observable<int, std::string> observable;
  observable.Fire(1, "abc");
}


TEST(ObservableTest, FireSubscribed) {
  Observable<int, std::string> observable;

  int counter1 = 0;
  auto cookie1 = observable.Subscribe([&counter1](int n, std::string s) {
    ++counter1;

    EXPECT_EQ(1, n);
    EXPECT_EQ("abc", s);
  });

  int counter2 = 0;
  auto cookie2 = observable.Subscribe([&counter2](int n, std::string s) {
    ++counter2;

    EXPECT_EQ(1, n);
    EXPECT_EQ("abc", s);
  });

  observable.Fire(1, "abc");

  observable.Unsubscribe(std::move(cookie1));

  observable.Fire(1, "abc");

  observable.Unsubscribe(std::move(cookie2));

  observable.Fire(2, "def");

  EXPECT_EQ(1, counter1);
  EXPECT_EQ(2, counter2);
}


TEST(ObservableTest, NullCookie) {
  Observable<int, std::string> observable;
  observable.Unsubscribe(nullptr);
}


TEST(ObservableTest, RecursiveFire) {
  Observable<> observable;

  int counter = 0;

  auto cookie = observable.Subscribe([&observable, &counter] () {
    ++counter;
    if (counter == 1) {
      observable.Fire();
    }
  });

  observable.Fire();

  EXPECT_EQ(2, counter);

  observable.Unsubscribe(std::move(cookie));
}

}  // namespace cdbg
}  // namespace devtools
