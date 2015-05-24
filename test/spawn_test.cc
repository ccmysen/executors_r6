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
#include <iostream>
#include <random>
#include <thread>

#include "executor.h"
#include "gtest/gtest.h"
#include "local_queue_thread_pool_executor.h"
#include "randomized_thread_pool_executor.h"
#include "thread_pool_executor.h"
#include "thread_per_task_executor.h"

using namespace std;

const int MAX_CONCURRENCY = 8;
const int LOG_MAX_SPAWNS = 12;

inline void fib() {
  constexpr int MAX_FIB = 4000;
  long long first = 0, second = 1;
  volatile long long next;
  for (int c = 0 ; c < MAX_FIB; c++ ) {
    if (c < 2) {
      next = c;
    } else {
      next = first + second;
      first = second;
      second = next;
    }
  }
}

TEST(SpawnTest, NoExecutor) {
  const int MAX_SPAWNS = 1 << LOG_MAX_SPAWNS;
  for (int i = 0; i < MAX_SPAWNS; ++i) {
    fib();
  }
  cout << "Total Spawns: " << MAX_SPAWNS << endl;
}

template <typename Exec>
void spn(Exec& exec, int depth, atomic<int>& count) {
  fib();
  // Spawn 2 for the one current tasks.
  if (depth > 0) {
    exec.spawn(bind(&spn<Exec>, ref(exec), depth - 1, ref(count)));
    exec.spawn(bind(&spn<Exec>, ref(exec), depth - 1, ref(count)));
  }
  count++;
}

TEST(SpawnTest, BasicSpawnBloom) {
  constexpr int MAX_SPAWNS = LOG_MAX_SPAWNS - 1;

  atomic<int> spawn_count;
  atomic_init(&spawn_count, 0);
  {
    experimental::thread_pool_executor tpe(MAX_CONCURRENCY);
    tpe.spawn(bind(&spn<decltype(tpe)>,
                   ref(tpe), MAX_SPAWNS, ref(spawn_count)));
  }
  cout << "Total Spawns: " << spawn_count.load() << endl;
}

class EmptyFunction {
 public:
  EmptyFunction() {}
  EmptyFunction(EmptyFunction& other) {}
  EmptyFunction(EmptyFunction&& other) {}
  void operator()() { fib(); }
};

TEST(SpawnTest, BigSpawn) {
  constexpr int MAX_SPAWNS = (1<<LOG_MAX_SPAWNS);

  {
    // Custom thread pool functor (saves the cost of type erasure, but requires
    // a type-erased executor).
    experimental::thread_pool_executor tpe(MAX_CONCURRENCY);
    for (int i = 0; i < MAX_SPAWNS; ++i) {
      tpe.spawn(&fib);
    }
  }
  cout << "Total Spawns: " << MAX_SPAWNS << endl;
}

TEST(SpawnTest, BigSpawnThreadPerTask) {
  constexpr int MAX_SPAWNS = (1<<LOG_MAX_SPAWNS);

  {
    experimental::thread_per_task_executor& tpte =
        experimental::thread_per_task_executor::get_executor();
    for (int i = 0; i < MAX_SPAWNS; ++i) {
      tpte.spawn(&fib);
    }
  }
  cout << "Total Spawns: " << MAX_SPAWNS << endl;
}

TEST(SpawnTest, BigSpawnRandomizedThreadPool) {
  constexpr int MAX_SPAWNS = (1<<LOG_MAX_SPAWNS);

  {
    // thread pools block waiting for tasks upon shutdown.
    experimental::randomized_thread_pool_executor rtpe(MAX_CONCURRENCY);
    for (int i = 0; i < MAX_SPAWNS; ++i) {
      rtpe.spawn(&fib);
    }
  }
  cout << "Total Spawns: " << MAX_SPAWNS << endl;
}

TEST(SpawnTest, BigSpawnLocalQueues) {
  constexpr int BATCH_COUNT = 16;
  constexpr int MAX_SPAWNS = (1<<LOG_MAX_SPAWNS) / BATCH_COUNT;

  {
    // Custom thread pool functor (saves the cost of type erasure, but requires
    // a type-erased executor).
    experimental::local_queue_thread_pool_executor lqtpe(MAX_CONCURRENCY);
    for (int b = 0; b < BATCH_COUNT; ++b) {
      vector<void(*)()> batch(MAX_SPAWNS);
      for (int i = 0; i < MAX_SPAWNS; ++i) {
        batch.push_back(&fib);
      }
      lqtpe.spawn_all(batch);
    }
  }
  cout << "Total Spawns: " << MAX_SPAWNS * BATCH_COUNT << endl;
}

TEST(SpawnTest, MutexingCounter) {
  constexpr int MAX_SPAWNS = (1<<LOG_MAX_SPAWNS) / MAX_CONCURRENCY;

  vector<thread> threads;
  mutex mu;
  int counter;
  for (int i = 0; i < MAX_CONCURRENCY; ++i) {
    threads.emplace_back([=, &mu, &counter] {
      for (int i = 0; i < MAX_SPAWNS; ++i) {
        fib();
        unique_lock<mutex> lock(mu);
        counter++;
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }
  cout << "Total Spawns: " << counter << endl;
}

TEST(SpawnTest, MutexingFunctionCounter) {
  constexpr int MAX_SPAWNS = (1<<LOG_MAX_SPAWNS) / MAX_CONCURRENCY;

  vector<thread> threads;
  mutex mu;
  int counter;
  for (int i = 0; i < MAX_CONCURRENCY; ++i) {
    threads.emplace_back([=, &mu, &counter] {
      for (int i = 0; i < MAX_SPAWNS; ++i) {
        std::function<void()> f([] { fib(); });
        f();
        unique_lock<mutex> lock(mu);
        counter++;
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }
  cout << "Total Spawns: " << counter << endl;
}

TEST(SpawnTest, MutexingFunctionCounterUncontended) {
  constexpr int MAX_SPAWNS = (1<<LOG_MAX_SPAWNS);

  mutex mu;
  int counter;
  thread t([=, &mu, &counter] {
    std::function<void()> f([] { fib(); });
    for (int i = 0; i < MAX_SPAWNS; ++i) {
      f();
      unique_lock<mutex> lock(mu);
      counter++;
    }
  });

  t.join();
  cout << "Total Spawns: " << counter << endl;
}

TEST(SpawnTest, MutexingFunctionCounterUncontended_Reinit) {
  constexpr int MAX_SPAWNS = (1<<LOG_MAX_SPAWNS);

  mutex mu;
  int counter;
  thread t([=, &mu, &counter] {
    for (int i = 0; i < MAX_SPAWNS; ++i) {
      std::function<void()> f([] { fib(); });
      f();
      unique_lock<mutex> lock(mu);
      counter++;
    }
  });

  t.join();
  cout << "Total Spawns: " << counter << endl;
}
