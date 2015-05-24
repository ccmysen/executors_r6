#ifndef EXECUTOR_H
#define EXECUTOR_H

#include <functional>
#include <future>
#include <type_traits>
#include <utility>

#include "executor_helper.h"

namespace std {
namespace experimental {


// Wrapper class which takes an executor and a function and creates a functor
// which just spawns the task on a particular executor when called. Useful for
// creating continuations which spawn tasks when they complete.
template <typename Exec, typename Func>
class executor_wrapper {
 public:
  executor_wrapper(Exec& exec, Func&& func)
    : exec_(exec), func_(forward<Func>(func)) {}

  executor_wrapper(executor_wrapper&& other)
    : exec_(other.exec_), func_(forward<Func>(other.func_)) {}

  ~executor_wrapper() {}
    
  template <class ...Args>
  void operator()(Args... args) {
    // Probably not the perfect thing to make func an rvalue reference here?
    exec_.spawn(bind(contained_function(), args...));
  }

  Exec& get_executor() {
    return exec_;
  }

  // Allows optimizations where func is run on the same executor as the caller
  // and thus doesn't need to call spawn.
  Func&& contained_function() {
    return move(func_);
  }
 private:
  Exec& exec_;
  Func func_;
};

// TODO(mysen): add some tests for wrap
template <typename Exec, typename Func>
executor_wrapper<Exec, typename decay<Func>::type>&& wrap(Exec& exec, Func&& func) {
  return executor_wrapper<Exec, Func>(exec, forward<Func>(func));
}

// Helper which contains an executor in a reference for easy copyability.
template <typename Exec, typename CopyEnable=void>
class executor_ref {
 public:
  typedef typename Exec::wrapper_type wrapper_type;
 public:
  executor_ref() = delete;
  executor_ref(Exec& exec) : exec_(exec) {}
  executor_ref(const executor_ref& other) : exec_(other.exec_) {}
  virtual ~executor_ref() {}

  template <typename Func>
  inline void spawn(Func&& func) {
    exec_.spawn(forward<Func>(func));
  }

  Exec& get_contained_executor() {
    return exec_;
  }

 private:
  Exec& exec_;
};

// If the executor is copy constructible, then remove the reference and make a
// copy instead. This eliminates the bit of overhead of references are used
// unnecessarily.
template <typename Exec>
class executor_ref<Exec,
                   typename enable_if<is_copy_constructible<Exec>::value>::type>
{
 public:
  typedef typename Exec::wrapper_type wrapper_type;
 public:
  executor_ref() = delete;
  executor_ref(Exec& exec) : exec_(exec) {}
  executor_ref(const executor_ref& other) : exec_(other.exec_) {}
  virtual ~executor_ref() {}

  template <typename Func>
  inline void spawn(Func&& func) {
    exec_.spawn(forward<Func>(func));
  }

  Exec& get_contained_executor() {
    return exec_;
  }

 private:
  Exec exec_;
};

// Fully type erased executor which ends up taking in a concrete wrapper which
// can accept both normal and move-only types.
class executor {
 public:
  typedef function_wrapper wrapper_type;
 public:
  executor() = delete;
  executor(const executor& other) : exec_(other.exec_) {}
  executor(executor&& other) : exec_() {
    other.exec_.swap(exec_);
  }

  template <typename Exec>
  executor(Exec& exec) : exec_(new executor_reference<Exec>(exec)) {}

  inline void spawn(function_wrapper&& fn) {
    // Preferrably, the executor should generally move construct the erase
    // wrapper from the input directly.
    exec_->spawn(forward<function_wrapper>(fn));
  }

 private:
  class executor_concept {
   public:
    virtual void spawn(function_wrapper&& func) = 0;
  };
  template <typename Exec>
  class executor_reference : public executor_concept {
   public:
    executor_reference(Exec& exec) : exec_(exec) {}
    virtual void spawn(function_wrapper&& func) {
      exec_.spawn(forward<function_wrapper>(func));
    }
   private:
    Exec& exec_;
  };

  shared_ptr<executor_concept> exec_;
};

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
  exec.spawn(function_wrapper([&] { func(); continuation(); }));
}

template <typename Func>
void spawn(executor&& exec, Func&& func) {
  exec.spawn(func);
}


}  // namespace experimental
}  // namespace std

#endif
