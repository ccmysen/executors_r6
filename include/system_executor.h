#ifndef SYSTEM_EXECUTOR_H
#define SYSTEM_EXECUTOR_H

#include <thread>
#include <utility>

#include "thread_pool_executor.h"

namespace std {
namespace experimental {

class system_executor {
 public:
  typedef function_wrapper wrapper_type;

 public:
  static system_executor& get_executor() {
    static system_executor instance(
        THREAD_RATIO * thread::hardware_concurrency());
    return instance;
  }

  // Destructor of the system executor should probably kill stuff...
  virtual ~system_executor() {}

 public:
  template<class Func>
  inline void spawn(Func&& func) {
    pool_.spawn(forward<Func>(func));
  }

 private:
  // Big enough to be useful, small enough to not completely kill performance
  // if you max out the CPU.
  static constexpr unsigned int THREAD_RATIO = 5;

  system_executor(size_t N) : pool_(N) {}

  thread_pool_executor pool_;
};

}  // namespace experimental
}  // namespace std

#endif
