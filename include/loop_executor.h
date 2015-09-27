#ifndef LOOP_EXECUTOR_H
#define LOOP_EXECUTOR_H

#include <condition_variable>
#include <mutex>
#include <queue>

#include "executor_helper.h"

namespace std {
namespace experimental {

class loop_executor {
 public:
  typedef executors::work wrapper_type;

 public:
  explicit loop_executor()
    : work_queue_(new queue<wrapper_type>()) {
    atomic_init(&exit_loop_signal_, false);
    atomic_init(&run_lock_, false);
  }

  virtual ~loop_executor() {}

  void loop() {
    if (!get_run_lock()) return;

    while (!exit_loop_signal_.load()) {
      unique_lock<mutex> queue_lock(work_queue_mu_);
      loop_signal_cv_.wait(queue_lock,
          [this] { return exit_loop_signal_.load() || !work_queue_->empty(); });
      if (exit_loop_signal_.load()) break;

      // Not being asked to exit.
      (work_queue_->front())();
      work_queue_->pop();
    }

    exit_loop_signal_.store(false);
    release_run_lock();
  }

  void run_queued_closures() {
    if (!get_run_lock()) return;

    // Functionally we can only guarantee what is processed once we can get the
    // lock so the wording on the loop_executor might be a little too strict
    // w.r.t. ordering of spawn vs. loop.
    unique_lock<mutex> queue_lock(work_queue_mu_);
    unique_ptr<queue<wrapper_type>> active_queue(
        new queue<wrapper_type>());
    // Active queue becomes a local queue so we can work on it.
    active_queue.swap(work_queue_);
    queue_lock.unlock();

    while (!exit_loop_signal_.load() && !active_queue->empty()) {
      (active_queue->front())();
      active_queue->pop();
    }

    // Exited early so need to put the active closures back at the head of the
    // queue.
    if (!active_queue->empty()) {
      exit_loop_signal_.store(false);

      queue_lock.lock();
      while(!work_queue_->empty()) {
        active_queue->push(move(work_queue_->front()));
        work_queue_->pop();
      }
      work_queue_.swap(active_queue);
      queue_lock.unlock();
    }
    release_run_lock();
  }

  bool try_run_one_closure() {
    if (!get_run_lock()) return false;
    unique_lock<mutex> queue_lock(work_queue_mu_);

    bool return_value = false;
    if (!work_queue_->empty()) {
      // Execute and pop the work we're going to run...
      (work_queue_->front())();
      work_queue_->pop();
      return_value = true;
    }

    queue_lock.unlock();
    release_run_lock();

    return return_value;
  }

  void make_loop_exit() {
    // If this is called when loop isn't active, what do we do? Probably want
    // it to be a NOP, but this is more expensive.
    bool old_val = false;
    if (exit_loop_signal_.compare_exchange_strong(old_val, true)) {
      // Signal anyone to wake up...
      loop_signal_cv_.notify_all();    
    }
  }

 public:
  // Executor interface
  template<class Func>
  void spawn(Func&& func) {
    // TODO: hook this up to the blocking queue object when we have that in the
    // TS.
    unique_lock<mutex> queue_lock(work_queue_mu_);
    work_queue_->emplace(forward<Func>(func));
  }

 private:
  bool get_run_lock() {
    // Bit to prevent other loop/run calls.
    bool old_val = false;
    if (!run_lock_.compare_exchange_strong(old_val, true)) {
      return false;
    }
    return true;
  } 

  void release_run_lock() {
    bool old_val = true;
    // This probably doesn't need cmpxchg since the presumption is that we had
    // the lock already.
    if (!run_lock_.compare_exchange_strong(old_val, false)) {
      return;
    }
  }

  mutex work_queue_mu_;
  condition_variable queue_empty_cv_;

  // Ideally manage the queue internally to the task itself.
  unique_ptr<queue<wrapper_type>> work_queue_;
  condition_variable loop_signal_cv_;
  atomic<bool> exit_loop_signal_;
  atomic<bool> run_lock_;
};

}  // namespace experimental
}  // namespace std

#endif
