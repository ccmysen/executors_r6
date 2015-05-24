#ifndef LIMITED_THREAD_PER_TASK_EXECUTOR_H
#define LIMITED_THREAD_PER_TASK_EXECUTOR_H

#include <map>
#include <thread>
#include <utility>

namespace std {
namespace experimental {

class limited_thread_per_task_executor {
 public:
  typedef function_wrapper wrapper_type;

 public:
  limited_thread_per_task_executor(int max_threads)
      : MAX_THREADS(max_threads) {
    atomic_init(&thread_count_, 0);
  }

  limited_thread_per_task_executor(limited_thread_per_task_executor&) = delete;

  virtual ~limited_thread_per_task_executor() {}

 public:
  template<class Func>
  inline void spawn(Func&& func) {
    // Approximate thread count capping since we aren't actually blocking
    // someone from increasing the count
    while (true) {
      int count = thread_count_.load();
      if (count > MAX_THREADS) {
        this_thread::sleep_for(chrono::milliseconds(1));
      } else {
        if (thread_count_.compare_exchange_strong(count, count + 1)) {
          break;
        }
      }
    }
    completion<Func> task(forward<Func>(func), thread_count_);
    thread t(task);
    t.detach();
  }

 private:
  template <typename Func>
  class completion {
   public:
    completion(Func&& func, atomic<int>& count) : func_(func), count_(count) {}
    ~completion() {}

    void operator()() {
      func_();
      count_--;
    }
   private:
    Func func_;
    atomic<int>& count_;
  };

  const int MAX_THREADS;
  atomic<int> thread_count_;
};

}  // namespace experimental
}  // namespace std

#endif  // LIMITED_THREAD_PER_TASK_EXECUTOR_H

