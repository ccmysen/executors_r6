#ifndef SERIAL_EXECUTOR
#define SERIAL_EXECUTOR

#include "executor.h"

//#include <concurrent_queue.h>
#include <ppltasks.h>
#include <functional>
#include <thread>

using namespace std;

class serial_executor {
    concurrency::task<void> next_task;
private:
    abstract_executor_ref m_executor;
    volatile bool running;
    volatile bool cancelled;
    volatile bool done;

public:
    explicit serial_executor(abstract_executor_ref underlying_executor) :
        next_task(concurrency::task_from_result()),
        m_executor(underlying_executor), running(false), done(false), cancelled(false) {}

    abstract_executor_ref underlying_executor() {
        return m_executor;
    }

    virtual ~serial_executor() {
        next_task.wait();
    }

    void add(function<void()> closure) {
        concurrency::task_completion_event<void> tce;
        next_task.then([=] {
            m_executor.add([=]() {
                closure();
                tce.set();
            });
        });

        next_task = concurrency::create_task(tce);
    }
};

#endif