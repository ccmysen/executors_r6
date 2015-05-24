// Copyright 2014 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Test of the core executor libraries.
#include <atomic>
#include <functional>
#include <future>
#include <set>
#include <thread>
#include <utility>
#include <vector>

#include "executor.h"
#include "gtest/gtest.h"
#include "loop_executor.h"
#include "serial_executor.h"
#include "system_executor.h"
#include "thread_per_task_executor.h"
#include "thread_pool_executor.h"

using namespace std;

class max_counter_task {
 public:
  max_counter_task(int num_tasks)
    : concurrency_(0), max_concurrency_(0),
      release_count_(num_tasks), start_count_(num_tasks) {}

  void operator()() {
    unique_lock<mutex> count_lock(mutex_); 
    thread_ids_.insert(this_thread::get_id());
    concurrency_++;
    if (concurrency_ > max_concurrency_) {
      max_concurrency_ = concurrency_;
    }
    if (!release_internal()) {
      // Wait to release until we are the only remaining releaser.
      cout << "Block, max_concurrency: " << max_concurrency_
           << " release count: " << release_count_ << endl;
      release_cv_.wait(count_lock, [this] { return release_count_ == 0; });
    }
    count_lock.unlock();
  }

  void release() {
    unique_lock<mutex> count_lock(mutex_); 
    release_internal();
  }

  // NOTE: be careful to only call reset after all threads who are waiting have
  // returned or else this may cause an ABA problem.
  void reset() {
    unique_lock<mutex> count_lock(mutex_); 
    release_count_ = start_count_;
    concurrency_ = 0;
    // Doesn't reset max_concurrency_ explicitly to allow for testing of concurrency across resets.
  }

  int release_count() {
    unique_lock<mutex> count_lock(mutex_);
    return release_count_;
  }

  int max_concurrency() {
    unique_lock<mutex> count_lock(mutex_); 
    return max_concurrency_;
  }

  int num_threads() {
    unique_lock<mutex> lock(mutex_); 
    return thread_ids_.size();
  }

  bool ran_in_thread(thread::id thread_id) {
    unique_lock<mutex> lock(mutex_); 
    return thread_ids_.find(thread_id) != thread_ids_.end();
  }
 private:
  // Caller should lock the mutex.
  // Returns true if release hit 0.
  bool release_internal() {
    release_count_--;
    if (release_count_ == 0) {
      release_cv_.notify_all();
      return true;
    }
    return false;
  }

  mutex mutex_;
  condition_variable release_cv_;
  int concurrency_;
  int max_concurrency_;
  int release_count_;
  int start_count_;
  set<thread::id> thread_ids_;
};

TEST(ThreadPerTaskExecutorTest, ConcurrencyTest) {
  constexpr int NUM_TASKS = 100;
  max_counter_task mct(NUM_TASKS);

  experimental::thread_per_task_executor& tpte =
    experimental::thread_per_task_executor::get_executor();

  // Should be able to create an arbitrary amount of concurrency.
  for (int i = 0; i < NUM_TASKS; ++i) {
    tpte.spawn(bind(&max_counter_task::operator(), &mct));
  }

  while (mct.release_count() > 0) {
    this_thread::sleep_for(chrono::milliseconds(10));
  }
  EXPECT_EQ(NUM_TASKS, mct.max_concurrency());
}

TEST(ThreadPoolExecutorTest, ConcurrencyTest) {
  constexpr int NUM_TASKS = 20;
  constexpr int MAX_CONCURRENCY = 10;
  max_counter_task mct(MAX_CONCURRENCY + 1);
  max_counter_task mct2(MAX_CONCURRENCY);
  experimental::thread_pool_executor tpe(MAX_CONCURRENCY);

  // Force all tasks to wait until signalled by the test.
  for (int i = 0; i < MAX_CONCURRENCY; ++i) {
    tpe.spawn(bind(&max_counter_task::operator(), &mct));
  }
  for (int i = 0; i < (NUM_TASKS - MAX_CONCURRENCY); ++i) {
    tpe.spawn(bind(&max_counter_task::operator(), &mct2));
  }

  // Wait until the test is the only signal remaining.
  while (mct.release_count() > 1) {
    this_thread::sleep_for(chrono::milliseconds(10));
  }
  // This should be 10 since that's the concurrency of the pool.
  EXPECT_EQ(MAX_CONCURRENCY, mct.max_concurrency());
  // Expect no executions on mct2 yet.
  EXPECT_EQ(0, mct2.max_concurrency());
  mct.release();

  while (mct2.release_count() > 0) {
    this_thread::sleep_for(chrono::milliseconds(10));
  }
  EXPECT_EQ(MAX_CONCURRENCY, mct2.max_concurrency());
}

TEST(LocalQueueThreadPoolExecutorTest, ConcurrencyTest) {
  constexpr int NUM_TASKS = 20;
  constexpr int MAX_CONCURRENCY = 10;
  max_counter_task mct(MAX_CONCURRENCY + 1);
  max_counter_task mct2(MAX_CONCURRENCY);
  experimental::thread_pool_executor tpe(MAX_CONCURRENCY);

  // Force all tasks to wait until signalled by the test.
  for (int i = 0; i < MAX_CONCURRENCY; ++i) {
    tpe.spawn(bind(&max_counter_task::operator(), &mct));
  }
  for (int i = 0; i < (NUM_TASKS - MAX_CONCURRENCY); ++i) {
    tpe.spawn(bind(&max_counter_task::operator(), &mct2));
  }

  // Wait until the test is the only signal remaining.
  while (mct.release_count() > 1) {
    this_thread::sleep_for(chrono::milliseconds(10));
  }
  // This should be 10 since that's the concurrency of the pool.
  EXPECT_EQ(MAX_CONCURRENCY, mct.max_concurrency());
  // Expect no executions on mct2 yet.
  EXPECT_EQ(0, mct2.max_concurrency());
  mct.release();

  while (mct2.release_count() > 0) {
    this_thread::sleep_for(chrono::milliseconds(10));
  }
  EXPECT_EQ(MAX_CONCURRENCY, mct2.max_concurrency());
}
TEST(SerialExecutorTest, ConcurrencyTest) {
  constexpr int NUM_TASKS = 20;
  constexpr int MAX_CONCURRENCY = 1;
  max_counter_task mct(MAX_CONCURRENCY + 1);
  experimental::thread_pool_executor tpe(MAX_CONCURRENCY + 5);
  experimental::serial_executor<experimental::thread_pool_executor> se(tpe);

  for (int i = 0; i < NUM_TASKS; ++i) {
    // Continuation which resets the counter before the next task starts up.
    // Reset shouldn't be called, though, until release_count reaches 0 on the
    // current operation.
    spawn(se, bind(&max_counter_task::operator(), &mct),
          [&mct] { mct.reset(); });
  }

  atomic<bool> done_flag;
  atomic_init(&done_flag, false);
  se.spawn([&done_flag] { done_flag.store(true);});

  int num_releases = 0;
  while (!done_flag.load()) {
    // Waiting on final release at this point, so actually release it.
    if (mct.release_count() == 1) {
      EXPECT_EQ(MAX_CONCURRENCY, mct.max_concurrency());
      ++num_releases;
      mct.release();
    }
  }
  EXPECT_EQ(NUM_TASKS, num_releases);
}

TEST(LoopExecutorTest, ConcurrencyTest) {
  // Similar to the serial executor test, but also with some checks that
  // everything runs on the same thread and that additional spawns after
  // looping are not run until the next loop cycle.
  constexpr int NUM_TASKS = 5;
  constexpr int MAX_CONCURRENCY = 1;
  max_counter_task mct(MAX_CONCURRENCY + 1);
  experimental::loop_executor le;

  // Add a started flag to indicate that the work is actually started. Allows
  // us to not do anything until after run_queued_closures() starts.
  // TODO: consider making this concept part of the interface.
  atomic<bool> started_flag;
  atomic_init(&started_flag, false);
  le.spawn([&] { started_flag.store(true); });
  for (int i = 0; i < NUM_TASKS; ++i) {
    // Continuation which resets the counter before the next task starts up.
    spawn(le, bind(&max_counter_task::operator(), &mct),
          [&mct] { mct.reset(); });
  }
  atomic<bool> done_flag;
  atomic_init(&done_flag, false);
  le.spawn([&done_flag] {done_flag.store(true);});

  // Start the executor thread.
  thread t1([&] { le.run_queued_closures(); });
  thread::id run_id = t1.get_id();

  // Wait for the first task to start.
  while (!started_flag.load()) {}

  // None of the counter tasks should run since the queued closures have
  // started running.
  // NOTE: there's probably a race for the start of run_queued_closures here.
  atomic<int> counter;
  atomic_init(&counter, 0);
  for (int i = 0; i < NUM_TASKS; ++i) {
    le.spawn([&counter] {counter++;});
  }

  int num_releases = 0;
  while (!done_flag.load()) {

    // Waiting on final release at this point, so actually release it.
    if (mct.release_count() == 1) {
      EXPECT_EQ(MAX_CONCURRENCY, mct.max_concurrency());
      ++num_releases;
      mct.release();
    }
  }

  EXPECT_EQ(NUM_TASKS, num_releases);
  EXPECT_EQ(0, counter.load());
  le.make_loop_exit();
  EXPECT_EQ(MAX_CONCURRENCY, mct.num_threads());
  EXPECT_TRUE(mct.ran_in_thread(run_id));

  t1.join();
}
