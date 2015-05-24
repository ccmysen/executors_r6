#ifndef LOCAL_QUEUE_THREAD_POOL_EXECUTOR_H
#define LOCAL_QUEUE_THREAD_POOL_EXECUTOR_H

#include <iostream>

#include <condition_variable>
#include <deque>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "executor_helper.h"

namespace std {
namespace experimental {

// Default to the type erasing wrapper.
// Add an allocator to the template to allow custom allocation of internal
// objects?
class local_queue_thread_pool_executor {
 public:
  typedef function_wrapper wrapper_type;

 public:
  // thread pools are not copyable/default constructable
  local_queue_thread_pool_executor() = delete;
  local_queue_thread_pool_executor(
      const local_queue_thread_pool_executor&) = delete;

  explicit local_queue_thread_pool_executor(size_t N)
      : threads_(N) {
    atomic_init(&in_shutdown_, NOT_SHUTDOWN);
    atomic_init(&spawn_count_, 0);
    for (size_t i = 0; i < N; ++i) {
      threads_.emplace_back(std::bind(
          &local_queue_thread_pool_executor::run_thread, this));
    }
  }

  virtual ~local_queue_thread_pool_executor() {
    set_shutdown(SOFT_SHUTDOWN);

    // Shut down the workers
    // Wait for them to join
    for (auto& t: threads_) {
      if (t.joinable()) {
        t.join();
      }
    }
  }

  virtual void shutdown_hard() {
    set_shutdown(HARD_SHUTDOWN);

    // Wait for them to join
    for (auto& t: threads_) {
      if (t.joinable()) {
        t.join();
      }
    }
  } 

  template<class Collect>
  void spawn_all(Collect& functions) {
    unique_lock<mutex> queue_lock(work_queue_mu_);
    if (in_hard_shutdown()) return;

    bool notify = work_queue_.empty();
    for (auto&& func : functions) {
      work_queue_.emplace(forward<typename Collect::value_type>(func));
    }
    cout << "Add batch" << endl;
    queue_lock.unlock();

    if (notify) {
      queue_empty_cv_.notify_all();
    }
  }

 public:
  // Executor interface
  template<class Func>
  inline void spawn(Func&& func) {
    unique_lock<mutex> queue_lock(work_queue_mu_);
    if (in_hard_shutdown()) return;

    bool notify = work_queue_.empty();
    work_queue_.emplace(forward<Func>(func));
    queue_lock.unlock();

    if (notify) {
      queue_empty_cv_.notify_one();
    }
  }

 private:
  // Shutdown immediately after current tasks complete
  static constexpr int NOT_SHUTDOWN = 0;
  // Shutdown threads as the queue empties
  static constexpr int SOFT_SHUTDOWN = 1;
  static constexpr int HARD_SHUTDOWN = 2;

  // Allow a thread to request tasks from the main queue. If there isn't much
  // contention on the main queue then this should be relatively cheap.
  // Returns true if any tasks were transferred.
  //
  // NOTE: this assumes the caller has taken the queue lock.
  bool transfer_tasks(deque<wrapper_type>& dest_queue, int max_tasks) {
    // Really nothing to do if shutting down or empty queue
    if ((in_shutdown() && work_queue_.empty()) || in_hard_shutdown()) {
      return false;
    }

    // Special logic for deciding how many tasks to transfer, just a rough
    // algorithm to avoid transferring too many tasks to any given thread (hard
    // to know for sure). This is in lieu of an actual work stealing algorithm.
    int transfer_tasks = max_tasks;
    int queue_size = work_queue_.size();
    if (queue_size <= 2) {
      transfer_tasks = queue_size;
    } else if ((2*transfer_tasks) > queue_size) {
      // roughly 1/3 to prevent a single thread from being the long pole
      transfer_tasks = queue_size / 3 + 1;
    } // else just transfer the total requested.

    for (int i = 0; i < transfer_tasks && !in_hard_shutdown(); ++i) {
      dest_queue.push_back(move(work_queue_.front()));
      work_queue_.pop();
    }

    return true;
  }

  void run_thread() {
    deque<wrapper_type> local_queue;
    constexpr int NUM_TRANSFER_TASKS = 100;

    while (true) {
      while (!local_queue.empty()) {
        wrapper_type next_fn(local_queue.front());
        local_queue.pop_front();
      }

      // Exhausted the local queue, so transfer more tasks.
      unique_lock<mutex> queue_lock(work_queue_mu_);
      queue_empty_cv_.wait(
          queue_lock, [this]{ return in_shutdown() || !work_queue_.empty(); });
      if ((in_shutdown() && work_queue_.empty()) ||
          in_shutdown_ == HARD_SHUTDOWN) {
        break;
      }
      transfer_tasks(local_queue, NUM_TRANSFER_TASKS);
      queue_lock.unlock();
    }
  }

  // Lock must be held by this thread before calling.
  inline bool in_shutdown() {
    return in_shutdown_.load() > NOT_SHUTDOWN;
  }

  inline bool in_hard_shutdown() {
    return in_shutdown_.load() == HARD_SHUTDOWN;
  }

  void set_shutdown(int shutdown_type) {
    int expected = NOT_SHUTDOWN;
    bool exchanged =
        in_shutdown_.compare_exchange_strong(expected, shutdown_type);
    if (!exchanged) return;
    queue_empty_cv_.notify_all();
  }

  vector<thread> threads_;
  mutex work_queue_mu_;
  condition_variable queue_empty_cv_;
  queue<wrapper_type> work_queue_;
  atomic<int> in_shutdown_;
  atomic<int> spawn_count_;
};

}  // namespace experimental
}  // namespace std

#endif
