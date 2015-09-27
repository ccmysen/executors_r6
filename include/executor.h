#ifndef EXECUTOR_H
#define EXECUTOR_H

#include <functional>
#include <future>
#include <type_traits>
#include <utility>

#include "executor_helper.h"

namespace std {
namespace experimental {

class executor {
 public:
  virtual void spawn(executors::work&& func) = 0;
};

template <typename Exec>
class executor_wrapper : public executor {
 public:
  executor_wrapper() = delete;
  executor_wrapper(const executor_wrapper& other) = delete;
  executor_wrapper(executor_wrapper&& other) = delete;

  // exec needs to be guaranteed to live as long as this class or bad things
  // will happen.
  executor_wrapper(Exec& exec) : exec_(exec) {}

  inline void spawn(executors::work&& fn) {
    exec_.spawn(forward<executors::work>(fn));
  }

 private:
  Exec& exec_;
};

// TODO: consider moving the free functions into the executors:: namespace.

// Small helper to create a packaged task on behalf of a given function.
// Returns tuple of {future, packaged_task}.
template <typename Func>
auto make_package(Func&& f) -> packaged_task<decltype(f())()> {
  return packaged_task<decltype(f())()>(f);
}

template <typename Exec, typename T>
inline future<T> spawn(Exec&& exec, packaged_task<T()>&& func) {
  future<T> result = func.get_future();
  exec.spawn(forward<packaged_task<T()>>(func));
  return result;
}

template <typename Exec, typename Func, typename Continuation>
void spawn(Exec&& exec, Func&& func, Continuation&& continuation) {
  // TODO: create a container which actually supports move-only objects.
  // Currently this wont move the function which is passed in, only make a
  // reference to it.
  exec.spawn([&] { func(); continuation(); });
}

template <typename Func, typename Continuation>
void spawn(executor&& exec, Func&& func, Continuation&& continuation) {
  exec.spawn(executors::work([&] { func(); continuation(); }));
}

template <typename Func>
void spawn(executor&& exec, Func&& func) {
  exec.spawn(func);
}


}  // namespace experimental
}  // namespace std

#endif
