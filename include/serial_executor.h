#ifndef SERIAL_EXECUTOR_H
#define SERIAL_EXECUTOR_H

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <utility>

#include "executor_helper.h"

namespace std {
namespace experimental {

template <typename Exec>
class serial_executor {
 public:
  typedef typename Exec::wrapper_type wrapper_type;

 public:
  explicit serial_executor(Exec& underlying_executor)
    : underlying_executor_(underlying_executor),
      in_shutdown_(false),
      last_task_done_(true) { }

  virtual ~serial_executor() {
    unique_lock<mutex> queue_lock(work_queue_mu_);
    in_shutdown_ = true;

    queue_cleared_cv_.wait(
        queue_lock, [this]{ return in_shutdown_ && work_queue_.empty(); });
  }

  Exec& underlying_executor() {
    return underlying_executor_;
  }

 public:
  // Executor interface
  template<class Func>
  void spawn(Func&& func) {
    unique_lock<mutex> queue_lock(work_queue_mu_);
    work_queue_.emplace(forward<Func>(func));
    if (last_task_done_) {
      // Nothing waiting so go straight to running.
      start_next_task();
    }
  }

 private:
  template <typename Func>
  class spawn_task {
   public:
    spawn_task(Func&& func, serial_executor* exec)
      : func_(forward<Func>(func)), exec_(exec) {}
    spawn_task(spawn_task&& other)
      : func_(forward<Func>(other.func_)), exec_(other.exec_) {
      other.exec_ = nullptr;
    }

    void operator()() {
      func_();
      exec_->complete_task();
    }
   private:
    Func func_;
    serial_executor* exec_;
  };

  void complete_task() {
    unique_lock<mutex> queue_lock(work_queue_mu_);
    last_task_done_ = true;
    if (!in_shutdown_) {
      start_next_task();
    } else {
      // Clean up the work queue
      while (!work_queue_.empty()) {
        work_queue_.pop();
      }
      queue_cleared_cv_.notify_one();
    }
  }

  // Assumes that the work queue mutex is already locked by the caller.
  inline void start_next_task() {
    if (!work_queue_.empty() && last_task_done_) {
      last_task_done_ = false;  // set up the exclusion on starting work.
      spawn_task<typename Exec::wrapper_type> task(
          move(work_queue_.front()), this);
      work_queue_.pop();
      underlying_executor_.spawn(move(task));
    }
  }

  Exec& underlying_executor_;

  mutex work_queue_mu_;

  // Ideally manage the queue internally to the task itself.
  queue<typename Exec::wrapper_type> work_queue_;
  condition_variable queue_cleared_cv_;
  // shutdown, so finish up current task and shut down.
  bool in_shutdown_;
  // whether anyone is currently working on a task. if false then anyone can
  // start work.
  bool last_task_done_;
};

}  // namespace experimental
}  // namespace std

#endif
