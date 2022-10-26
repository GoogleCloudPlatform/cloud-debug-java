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

#include "src/agent/worker.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <thread>

#include "gtest/gtest.h"
#include "src/agent/agent_thread.h"
#include "src/agent/callbacks_monitor.h"
#include "src/agent/format_queue.h"
#include "src/agent/mutex.h"
#include "src/agent/semaphore.h"
#include "src/agent/statistician.h"
#include "tests/agent/fake_jni.h"
#include "tests/agent/mock_bridge.h"
#include "tests/agent/mock_class_path_lookup.h"
#include "tests/agent/mock_jvmti_env.h"
#include "tests/agent/mock_worker_provider.h"


//#include "thread/thread.h"
//#include "util/functional/to_callback.h"

using testing::_;
using testing::AtLeast;
using testing::DoAll;
using testing::Exactly;
using testing::InSequence;
using testing::Invoke;
using testing::NiceMock;
using testing::NotNull;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;

namespace devtools {
namespace cdbg {

class FakeTimeSemaphore : public Semaphore {
 public:
  FakeTimeSemaphore() {}

  bool Initialize() override {
    return true;
  }

  bool Acquire(int timeout_ms) override {
    absl::MutexLock lock(&mu_);
    if (permits_ > 0) {
      --permits_;
      return true;
    }

    return false;
  }

  int DrainPermits() override {
    absl::MutexLock lock(&mu_);
    int current_permits = permits_;
    permits_ = 0;
    return current_permits;
  }

  void Release() override {
    absl::MutexLock lock(&mu_);
    ++permits_;
  }

 private:
  absl::Mutex mu_;
  int permits_ { 0 };

  DISALLOW_COPY_AND_ASSIGN(FakeTimeSemaphore);
};


class TestAgentThread : public AgentThread {
 public:
  TestAgentThread() {}

  bool IsStarted() const override {
    return thread_ != nullptr;
  };

  // Starts the thread. The "thread_name" argument is not used in this test
  // implementation, but kept to meet the AgentThread Start() API.
  bool Start(const std::string& /*thread_name*/,
             std::function<void()> thread_proc) override {

    thread_.reset(new std::thread(thread_proc));
    return true;
  }

  // Waits for the thread to complete and then releases all the references.
  void Join() override {
    thread_->join();
  }

  // Stalls the thread that called "Sleep" function. This might not be the
  // thread created by "Start" function. The function may return prematurely
  // if the sleep was interrupted.
  void Sleep(int ms) override {
    ADD_FAILURE() << "AgentThread::Sleep unexpected";
  }

 private:
  std::unique_ptr<std::thread> thread_;

  DISALLOW_COPY_AND_ASSIGN(TestAgentThread);
};


class WorkerTest : public ::testing::Test {
 protected:
  WorkerTest()
      : global_jvm_(fake_jni_.jvmti(), fake_jni_.jni()),
        bridge_(new NiceMock<MockBridge>) {
  }

  void SetUp() override {
    InitializeStatisticians();
    CallbacksMonitor::InitializeSingleton(1000);

    ON_CALL(*bridge_, IsEnabled(_))
      .WillByDefault(DoAll(SetArgPointee<0>(true), Return(true)));

    worker_.reset(new Worker(
        &provider_,
        [this] () {
          std::unique_ptr<Semaphore> semaphore(new FakeTimeSemaphore);
          return std::unique_ptr<AutoResetEvent>(
              new AutoResetEvent(std::move(semaphore)));
        },
        [this] () {
           return std::unique_ptr<AgentThread>(
               new TestAgentThread());
        },
        &class_path_lookup_,
        std::unique_ptr<Bridge>(bridge_),
        &format_queue_));
  }

  void TearDown() override {
    worker_->Shutdown();
    worker_ = nullptr;

    CallbacksMonitor::CleanupSingleton();
    CleanupStatisticians();
  }

 protected:
  FakeJni fake_jni_;
  GlobalJvmEnv global_jvm_;
  NiceMock<MockWorkerProvider> provider_;
  StrictMock<MockClassPathLookup> class_path_lookup_;
  MockBridge* const bridge_;
  FormatQueue format_queue_;
  std::unique_ptr<Worker> worker_;
};


TEST_F(WorkerTest, ShutdownNoStart) {
}


TEST_F(WorkerTest, SuccessfulFlow) {
  EXPECT_CALL(*bridge_, Bind(&class_path_lookup_))
      .WillOnce(Return(true));

  EXPECT_CALL(provider_, OnIdle())
      .Times(AtLeast(10));

  EXPECT_CALL(*bridge_, RegisterDebuggee(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(true), Return(true)));

  worker_->Start();
  usleep(100000);
}


TEST_F(WorkerTest, InitializationFailureOnWorkerReady) {
  EXPECT_CALL(provider_, OnWorkerReady(_))
      .WillOnce(Return(false));

  EXPECT_CALL(*bridge_, RegisterDebuggee(_, _))
      .Times(0);
  EXPECT_CALL(*bridge_, ListActiveBreakpoints(_))
      .Times(0);

  worker_->Start();
  usleep(100000);
}


TEST_F(WorkerTest, InitializationFailureBridgeBind) {
  EXPECT_CALL(*bridge_, Bind(&class_path_lookup_))
      .WillOnce(Return(false));

  EXPECT_CALL(*bridge_, RegisterDebuggee(_, _))
      .Times(0);
  EXPECT_CALL(*bridge_, ListActiveBreakpoints(_))
      .Times(0);

  worker_->Start();
  usleep(100000);
}

TEST_F(WorkerTest, InitializationFailureIsDisabled) {
  EXPECT_CALL(*bridge_, Bind(&class_path_lookup_))
      .WillOnce(Return(true));

  EXPECT_CALL(*bridge_, IsEnabled(_))
      .WillOnce(DoAll(SetArgPointee<0>(false), Return(true)));

  EXPECT_CALL(*bridge_, RegisterDebuggee(_, _))
      .Times(0);
  EXPECT_CALL(*bridge_, ListActiveBreakpoints(_))
      .Times(0);

  worker_->Start();
  usleep(100000);
}


TEST_F(WorkerTest, RegisterDebuggeeFailure) {
  // Simulate failure in first 3 calls to "RegisterDebuggee" then
  // a single success.
  EXPECT_CALL(*bridge_, RegisterDebuggee(_, _))
      .WillOnce(Return(false))
      .WillOnce(Return(false))
      .WillOnce(Return(false))
      .WillOnce(DoAll(SetArgPointee<0>(true), Return(true)));

  worker_->Start();
  usleep(100000);
}


TEST_F(WorkerTest, RegisterDebuggeeSuccessDebuggerDisabled) {
  // Simulate failure in first 3 calls to "RegisterDebuggee" then
  // a single success.
  EXPECT_CALL(*bridge_, RegisterDebuggee(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(false), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(false), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(false), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(true), Return(true)));

  worker_->Start();
  usleep(100000);
}


TEST_F(WorkerTest, TransmitBreakpointUpdates) {
  EXPECT_CALL(*bridge_, RegisterDebuggee(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(true), Return(true)));

  EXPECT_CALL(*bridge_, ListActiveBreakpoints(_))
      .WillRepeatedly(Invoke([] (
          std::vector<std::unique_ptr<BreakpointModel>>* breakpoints) {
        breakpoints->clear();
        breakpoints->push_back(
            std::unique_ptr<BreakpointModel>(new BreakpointModel));
        return Bridge::HangingGetResult::SUCCESS;
      }));

  worker_->Start();
  usleep(100000);

  EXPECT_CALL(*bridge_, EnqueueBreakpointUpdateProxy(_))
      .Times(Exactly(1));

  EXPECT_CALL(*bridge_, HasPendingMessages())
      .Times(AtLeast(10))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*bridge_, TransmitBreakpointUpdates())
      .Times(AtLeast(10));

  format_queue_.Enqueue(
      std::unique_ptr<BreakpointModel>(new BreakpointModel),
      nullptr);

  usleep(100000);
}


}  // namespace cdbg
}  // namespace devtools


