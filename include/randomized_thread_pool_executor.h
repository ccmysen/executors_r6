#ifndef RANDOMIZED_THREAD_POOL_EXECUTOR_H
#define RANDOMIZED_THREAD_POOL_EXECUTOR_H

#include <iostream>

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "executor_helper.h"

namespace std {
namespace experimental {

class randomized_thread_pool_executor {
 private:
  typedef function_wrapper wrapper_type;

 public:
  // thread pools are not copyable/default constructable
  randomized_thread_pool_executor() = delete;
  randomized_thread_pool_executor(
      const randomized_thread_pool_executor&) = delete;

  explicit randomized_thread_pool_executor(size_t N)
      : threads_(N), NUM_THREADS(N) {
    for (size_t i = 0; i < N; ++i) {
      blocking_queue* q = new blocking_queue();
      queues_.emplace_back(q);
      threads_.emplace_back(
          std::bind(&randomized_thread_pool_executor::run_thread,
                    this, q)
      );
    }

    atomic_init(&next_queue_, 0);
  }

  virtual ~randomized_thread_pool_executor() {
    do_shutdown(SOFT_SHUTDOWN);
  }

  void shutdown_hard() {
    do_shutdown(HARD_SHUTDOWN);
  }

 public:
  // Executor interface
  template<class Func>
  void spawn(Func&& func) {
    int queue_idx = get_next_queue();
    queues_[queue_idx]->add(forward<Func>(func));
  }

 private:
  static constexpr int NOT_SHUTDOWN = 0;
  // Shutdown threads as the queue empties
  static constexpr int SOFT_SHUTDOWN = 1;
  // Force shutdown immediately after current tasks complete
  static constexpr int HARD_SHUTDOWN = 2;

  class blocking_queue {
   public:
    blocking_queue() {
      unique_lock<mutex> queue_lock(work_queue_mu_);
      shutdown_ = NOT_SHUTDOWN;
    }
    blocking_queue(const blocking_queue&) = delete;
    blocking_queue(blocking_queue&& other)
        : work_queue_(move(other.work_queue_)) {
      unique_lock<mutex> other_queue_lock(other.work_queue_mu_);
      unique_lock<mutex> queue_lock(work_queue_mu_);
      shutdown_ = other.shutdown_;
    }

    ~blocking_queue() {}

    template <typename Func>
    void add(Func&& func) {
      {
        unique_lock<mutex> queue_lock(work_queue_mu_);
        if (shutdown_ == HARD_SHUTDOWN) return;
        work_queue_.emplace(forward<Func>(func));
      }
      queue_empty_cv_.notify_one();
    }

    // What to return if shutdown_ is set??
    bool wait_for_get() {
      unique_lock<mutex> queue_lock(work_queue_mu_);
      queue_empty_cv_.wait(
          queue_lock,
          [this] { return shutdown_ > NOT_SHUTDOWN || !work_queue_.empty(); }
      );

      if (work_queue_.empty() || shutdown_ == HARD_SHUTDOWN) {
        return false;
      }

      // Release the mutex so the get() call can unlock it.
      queue_lock.release();
      return true;
    }

    // If wait_for_get returned true, then you must call this to release the
    // lock.
    wrapper_type get() {
      // Assume the lock is still held...
      lock_guard<mutex> guard(work_queue_mu_, std::adopt_lock);

      wrapper_type front(forward<wrapper_type>(work_queue_.front()));
      work_queue_.pop();
      return front;
    }

    void set_shutdown(int shutdown_type) {
      unique_lock<mutex> lock(work_queue_mu_);
      if (shutdown_type <= NOT_SHUTDOWN) return;
      shutdown_ = shutdown_type;
      lock.unlock();

      // There will only be one thread to notify...
      queue_empty_cv_.notify_one();
    }

    bool empty() {
      unique_lock<mutex> queue_lock(work_queue_mu_);
      return work_queue_.empty();
    }
   private:
    mutex work_queue_mu_;
    condition_variable queue_empty_cv_;
    queue<wrapper_type> work_queue_;
    int shutdown_;
  };

  int get_next_queue() {
    int next_queue_idx = next_queue_.load();
    // Random is basically add some prime number and mod by num threads.
    while (!next_queue_.compare_exchange_strong(next_queue_idx,
              (next_queue_idx + 541) % NUM_THREADS)) {
      next_queue_idx = next_queue_.load();
    }

    return next_queue_idx;
  }

  void do_shutdown(int shutdown_type) {
    for (size_t i = 0; i < NUM_THREADS; ++i) {
      queues_[i]->set_shutdown(shutdown_type);
    }

    // Shut down the workers
    // Wait for them to join
    for (auto& t: threads_) {
      if (t.joinable()) {
        t.join();
      }
    }
  } 

  void run_thread(blocking_queue* thread_queue) {
    while (true) {
      if (!thread_queue->wait_for_get()) {
        break;
      }

      // Run the front of the queue.
      wrapper_type wrap = thread_queue->get();
      wrap();
    }
  }

  vector<thread> threads_;
  vector<unique_ptr<blocking_queue>> queues_;
  mutex work_queue_mu_;
  atomic<int> next_queue_;
  const size_t NUM_THREADS;
};

}  // namespace experimental
}  // namespace std

#endif
