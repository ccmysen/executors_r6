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
#include <array>
#include <atomic>
#include <complex>
#include <iostream>
#include <thread>

#include "executor.h"
#include "gtest/gtest.h"
#include "thread_pool_executor.h"

using namespace std;

template <int BLOCK_SIZE>
void mandelbrot(int* image,
                int x_min, int y_min,
                int x_max, int y_max,
                double cx_min, double cx_max,
                double cy_min, double cy_max,
                unsigned int max_iter) {
  int sum_iter = 0;
  for (int x = x_min; x < (x_min + BLOCK_SIZE); ++x) {
    for (int y = y_min; y < (y_min + BLOCK_SIZE); ++y) {
      complex<double> c(cx_min + x / (x_max - 1.0) * (cx_max - cx_min),
                        cy_min + y / (y_max - 1.0)*(cy_max - cy_min));
      complex<double> z = 0;
      unsigned int iterations = max_iter;
      for (; iterations > 0 && abs(z) <= 2.0; --iterations) {
        z = z*z + c;
      }

      const int index = y * BLOCK_SIZE + x;
      image[index] = iterations | (iterations << 8);
      sum_iter += iterations;
    }
  }
  cout << "Done one " << sum_iter << endl;
}

// Big single threaded join operation.
template <int BLOCK_SIZE, int X_DIM, int Y_DIM>
void merge_blocks(int* images[],
                  int result[],
                  int num_x, int num_y,
                  int block_size) {

}

TEST(MandelbrotTest, ConcurrencyTest) {
  constexpr int MAX_CONCURRENCY = 1;
  constexpr int X_SIZE = 2000;
  constexpr int Y_SIZE = 2000;
  constexpr int BLOCK_SIZE = 100;
  experimental::thread_pool_executor tpe(MAX_CONCURRENCY);

  constexpr int NUM_X = X_SIZE / BLOCK_SIZE;
  constexpr int NUM_Y = Y_SIZE / BLOCK_SIZE;
  constexpr int NUM_BLOCKS = NUM_X * NUM_Y;
  atomic<int> done_count;
  atomic_init(&done_count, NUM_BLOCKS);

  // Blocks in row-major order.
  int* image_blocks[NUM_BLOCKS];
  for (int i = 0; i < NUM_BLOCKS; ++i) {
    image_blocks[i] = new int[BLOCK_SIZE * BLOCK_SIZE];
  }

  for (int x = 0; x < NUM_X; ++x) {
    for (int y = 0; y < NUM_Y; ++y) {
      cout << "Starting block " << x * BLOCK_SIZE << " " << y * BLOCK_SIZE << endl;
      spawn(tpe, [=] {
          mandelbrot<BLOCK_SIZE>(image_blocks[y * NUM_X + x],
                     x * BLOCK_SIZE, y * BLOCK_SIZE,
                     X_SIZE, Y_SIZE,
                     -2.0, 1.0, -1.0, 1.0,
                     1000U);
        },
        [&] { done_count--; });
    }
  }

  cout << "Waiting" << endl;
  chrono::milliseconds duration(100);
  while (done_count.load() > 0) {
    this_thread::sleep_for(duration);
  }

  // All done, merge.
  int result[X_SIZE * Y_SIZE];
  merge_blocks<BLOCK_SIZE, X_SIZE, Y_SIZE>(image_blocks, result, NUM_X, NUM_Y, BLOCK_SIZE);
}
