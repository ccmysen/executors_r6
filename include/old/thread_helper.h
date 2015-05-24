#ifndef THREAD_HELPER
#define THREAD_HELPER

#include "thread_traits.h"
#include "thread_util.h"

#include <atomic>
#include <functional>
#include <vector>
#include <mutex>
#include <condition_variable>

using namespace std;

namespace internal {

/* For passing std::function as a void pointer */
template <class derived>
class fnc_wrapper_interface {
    void run() {
        static_cast<derived*>(this)->start();
    }
};

class functional_pool;

class fnc_wrapper : fnc_wrapper_interface<fnc_wrapper> {
    function<void()> m_ptr;
    functional_pool * m_pool;
public:
    template<class Func>
    fnc_wrapper(Func&& wrapper, functional_pool* pool) : m_ptr(wrapper), m_pool(pool) {}
    void run() { m_ptr(); }
    functional_pool* pool() { return m_pool; }
};

class functional_pool
{
private:
    pool m_pool;                // Represents the internal threadpool
    cleanup_group cg;           // Responsible for cleaning up objects in the m_pool environment
    environment e;              // Custom environment for threadpool

    condition_variable all_tasks_finished_cv;
    mutex              all_tasks_finished_mutex;

    void start_task()
    {
        m_uninitiated_task_count--;
    }

    void finish_task()
    {
        m_unfinished_task_count--;
        if (m_unfinished_task_count == 0) {
            all_tasks_finished_cv.notify_all();
        }
    }

    static void CALLBACK callback(PTP_CALLBACK_INSTANCE, void * context)
    {
        run(context);
    }

protected:
    atomic<int> m_uninitiated_task_count;
    atomic<int> m_unfinished_task_count;

    // Runs closure from callback
    static void run(void * context)
    {
        auto q = reinterpret_cast<fnc_wrapper *>(context);
        q->pool()->start_task();
        q->run();
        q->pool()->finish_task();
        delete q;
    }

public:

    functional_pool()
      : m_pool(nullptr), cg(nullptr),
        m_uninitiated_task_count(0), m_unfinished_task_count(0) {}

    functional_pool(int num_threads)
      : m_pool(CreateThreadpool(nullptr)), cg(CreateThreadpoolCleanupGroup()),
        m_uninitiated_task_count(0), m_unfinished_task_count(0), e()
    {
        SetThreadpoolCallbackPool(e.get(), m_pool.get());
        SetThreadpoolCallbackCleanupGroup(e.get(), cg.get(), nullptr);

        SetThreadpoolThreadMinimum(m_pool.get(), num_threads);
        SetThreadpoolThreadMaximum(m_pool.get(), num_threads);
    }

    ~functional_pool()
    {
        std::unique_lock<std::mutex> lk(all_tasks_finished_mutex);
        all_tasks_finished_cv.wait(lk, [this]{ return m_unfinished_task_count == 0; });

        if (cg.get())
            CloseThreadpoolCleanupGroupMembers(cg.get(), false, nullptr);
    }

    // Submit work item to custom threadpool
    template<class Func>
    void submit(Func&& closure)
    {
        m_unfinished_task_count++;
        m_uninitiated_task_count++;
        fnc_wrapper * wrapper = new fnc_wrapper(std::move(closure), this);
        TrySubmitThreadpoolCallback(callback, wrapper, e.get());
    }

    size_t uninitiated_task_count() const
    {
        return (size_t)m_uninitiated_task_count;
    }
};

}

#endif
