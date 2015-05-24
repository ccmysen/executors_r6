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
#include <algorithm>
#include <complex>
#include <iostream>
#include <random>
#include <thread>

#include "executor.h"
#include "gtest/gtest.h"
#include "thread_pool_executor.h"

using namespace std;

// Pivot from [min, max], return end of the left half.
int pivot(int* arr, int min, int max, int pivot) {
  int rear = max;
  int front = min;

  while (true) {
    //     .     *
    // --==+=+--+=
    //       . *  
    // --====+--++
    //        *.  
    // --====--+++
    while (arr[rear] > pivot) { --rear; }
    while (arr[front] <= pivot) { ++front; }

    if (front < rear) {
      swap(arr[front], arr[rear]);     
    } else {
      break;
    }
  }

  // End with all the values equal or less than the pivot ending at rear and
  // all greater starting at front.
  return rear;
}

template <typename Rand>
void quicksort_single(int* arr, int min, int max, Rand& rand) {
  std::uniform_int_distribution<int> dist(min, max);
  int pivot_val = arr[dist(rand)];
  int split_point = pivot(arr, min, max, pivot_val);

  if (min < split_point) quicksort_single(arr, min, split_point, rand);
  if (max > split_point) quicksort_single(arr, split_point + 1, max, rand);
}

template <typename Exec, typename Start, typename Done, int MIN_SIZE>
void quicksort1(Exec& exec,
                int* arr, int min, int max,
                Start& start_call,
                Done& done_call) {
  int delta = max - min;
  
  std::random_device rd;
  std::uniform_int_distribution<int> dist(min, max);

  // Check if we will spawn new parallel tasks or not.
  if (delta < MIN_SIZE) {
    quicksort_single(arr, min, max, rd);
  } else {
    int pivot_val = arr[dist(rand)];
    int split_point = pivot(arr, min, max, pivot_val);

    if (min < split_point) {
      start_call();
      exec.spawn(bind(&quicksort1,
                      exec, arr, min, split_point, start_call, done_call));
    }
    if (max > split_point) {
      start_call();
      exec.spawn(bind(&quicksort1,
                      exec, arr, split_point + 1, max, start_call, done_call));
    }
  }
  done_call();
}

TEST(QuicksortTest, ConcurrencyTest) {
  constexpr int NUM_VALUES = 100;
  int values[NUM_VALUES];

  std::random_device rd;
  std::uniform_int_distribution<int> dist(0, NUM_VALUES);

  for (int i = 0; i < NUM_VALUES; ++i) {
    values[i] = dist(rd);
  }

  int values_out[NUM_VALUES];
  memcpy(values_out, values, NUM_VALUES * sizeof(int));
  quicksort_single(values, 0, NUM_VALUES - 1, rd);

  for (int i = 0; i < NUM_VALUES; ++i) {
    cout << values_out[i] << endl;
  }
}
