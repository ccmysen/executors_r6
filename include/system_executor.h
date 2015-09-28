#ifndef SYSTEM_EXECUTOR_H
#define SYSTEM_EXECUTOR_H

#include <thread>
#include <utility>

#include "thread_pool_executor.h"

namespace std {
namespace experimental {

class system_executor {
 public:
  typedef executors::work wrapper_type;

 public:
  static system_executor& get_executor() {
    // Destroys the system executor during static destruction, though may need
    // some special logic to handle doing this before statics (probably need
    // something built in).
    //
    // TODO: consider making an executor registry which is just a static
    // registry of executors which need to be deleted during static
    // destruction. This would allow non-singleton executors to delete during
    // this same phase.
    static system_executor instance(
        THREAD_RATIO * thread::hardware_concurrency());
    return instance;
  }

  // NOTE: by default the internal thread pool will
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

  // TODO: make the executor actually grow depending on forward progress of the
  // threads (need to figure out a reasonable mechanism for guaranteeing
  // forward progress.
  thread_pool_executor pool_;
};

}  // namespace experimental
}  // namespace std

#endif
