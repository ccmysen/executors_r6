#ifndef EXECUTOR_HELPER_H
#define EXECUTOR_HELPER_H

#include <iostream>
#include <mutex>
#include <utility>

namespace std {
namespace experimental {

// Specialized type-erasing function wrapper. This basically does what
// std::function does but is not copyable and thus allows for move-only types.
class function_wrapper {
 public:
  function_wrapper() = delete;
  function_wrapper(const function_wrapper&) = delete;
  function_wrapper(function_wrapper&& other) {
    wrapped_.reset(other.wrapped_.release());
  }
  virtual ~function_wrapper() {}

  template <typename T>
  function_wrapper(T&& t) {
    wrapped_.reset(new erase_container<T>(forward<T>(t)));
  }

  void operator()() {
    return (*wrapped_.get())();
  }

 private:
  class erase_concept {
   public:
    virtual void operator()() = 0;
  };
  
  template <typename T>
  class erase_container : public erase_concept {
   public:
    virtual ~erase_container() {
    }
    erase_container(T&& t) : t_(forward<T>(t)) {}
    virtual void operator()() {
      t_();
    }
   private:
    T t_;
  };

  unique_ptr<erase_concept> wrapped_;
};

}  // namespace experimental
}  // namespace std

#endif  // EXECUTOR_HELPER_H

