#ifndef THREAD_POOL
#define THREAD_POOL

#include "executor.h"
#include "thread_helper.h"

using namespace std;

class thread_pool {
private:
    shared_ptr<internal::functional_pool> pool;

public:
    explicit thread_pool(int N) : pool(new internal::functional_pool(N)) {}

    template<class Func>
    void spawn(Func&& closure) {
        pool->submit(std::move(closure));
    }
};

#endif
