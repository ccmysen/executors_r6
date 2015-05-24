#ifndef SYSTEM_EXECUTOR
#define SYSTEM_EXECUTOR

#include "executor.h"
#include "thread_helper.h"

using namespace std;

class system_executor {
private:
    shared_ptr<internal::functional_pool> pool;

    // Private default constructor: users must access system_executor by calling get_executor
    system_executor() : pool(new internal::functional_pool()) {}

public:
    static system_executor& get_executor() {
        static system_executor instance;
        return instance;
    }

    template<class Func>
    void spawn(Func&& closure) {
        pool->submit(std::move(closure));
    }
};

namespace internal {
    extern system_executor g_system_executor;
}

#endif
