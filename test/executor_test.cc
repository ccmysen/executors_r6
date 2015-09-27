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
#include <functional>
#include <future>
#include <set>
#include <thread>
#include <utility>
#include <vector>

#include "executor.h"
#include "gtest/gtest.h"
#include "serial_executor.h"
#include "system_executor.h"
#include "thread_per_task_executor.h"
#include "thread_pool_executor.h"

using namespace std;

namespace std {
namespace experimental {

thread::id get_id() {
  return this_thread::get_id();
}

int get_next_id(atomic<int>& id_gen) {
  int old_id = id_gen.load();
  while (!id_gen.compare_exchange_strong(old_id, old_id + 1)) {}
  return old_id;
}

TEST(ExecutorTest, SpawnFuture) {
  thread_per_task_executor& tpte =
    thread_per_task_executor::get_executor();
  atomic<int> id_gen;
  atomic_init(&id_gen, 0);

  constexpr int NUM_ITER = 100;
  vector<future<int>> futures;
  for (int i = 0; i < NUM_ITER; ++i) {
    futures.emplace_back(move(spawn(tpte,
        make_package(bind(&get_next_id, ref(id_gen))))));
  }

  EXPECT_EQ(NUM_ITER, futures.size());

  set<int> ids;
  for (int i = 0; i < futures.size(); ++i) {
    int id = futures[i].get();
    ids.insert(id);
  }

  EXPECT_EQ(NUM_ITER, ids.size());
}

TEST(ExecutorTest, ExecutorRefSpawnFuture) {
  thread_per_task_executor& tpte = thread_per_task_executor::get_executor();
  atomic<int> id_gen;
  atomic_init(&id_gen, 0);

  constexpr int NUM_ITER = 100;
  set<int> ids;
  for (int i = 0; i < NUM_ITER; ++i) {
    auto fut = spawn(tpte, make_package(bind(&get_next_id, ref(id_gen))));
    ids.insert(fut.get());
  }

  EXPECT_EQ(NUM_ITER, ids.size());
}

TEST(ExecutorTest, ExecutorWrapper) {
  thread_pool_executor tpe(1);
  // The erased executor should work just the same as any other executor but
  // requires that a function_wrapper be passed in.
  executor_wrapper<thread_pool_executor> exec(tpe);

  constexpr int NUM_ITER = 100;
  set<thread::id> ids;
  atomic<int> run_count;
  atomic_init(&run_count, 0);
  atomic<int> count_down;
  atomic_init(&count_down, NUM_ITER-1);
  for (int i = 0; i < NUM_ITER; ++i) {
    // Specialized spawn
    spawn(
      exec,
      [&run_count] {run_count++;},
      [&count_down] {count_down--;});
  }

  while (count_down.load() >= 0) {
    this_thread::sleep_for(chrono::milliseconds(1));
  }
  EXPECT_EQ(NUM_ITER, run_count.load());

  exec.spawn([&run_count] {run_count++;});
}

TEST(ExecutorTest, SpawnContinuation) {
  thread_pool_executor tpe(1);
  // TODO(mysen): add a test here
}

TEST(ExecutorTest, BaseSpawn) {
  thread_pool_executor tpe(1);
  // TODO(mysen): add a test here
}

}  // namespace std
}  // namespace experimental
