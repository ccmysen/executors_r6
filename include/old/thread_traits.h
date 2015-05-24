#ifndef THREAD_TRAITS
#define THREAD_TRAITS

#include "thread_util.h"

#include <windows.h>

struct pool_traits
{
    static PTP_POOL invalid() throw()
    {
        return nullptr;
    }

    static void close(PTP_POOL value) throw()
    {
        CloseThreadpool(value);
    }
};
typedef unique_handle<PTP_POOL, pool_traits> pool;

struct cleanup_group_traits
{
    static PTP_CLEANUP_GROUP invalid() throw()
    {
        return nullptr;
    }

    static void close(PTP_CLEANUP_GROUP value) throw()
    {
        CloseThreadpoolCleanupGroup(value);
    }
};
typedef unique_handle<PTP_CLEANUP_GROUP, cleanup_group_traits> cleanup_group;

struct timer_queue_traits
{
    static HANDLE invalid() throw()
    {
        return nullptr;
    }

    static void close(HANDLE value) throw()
    {
        DeleteTimerQueueEx(value, INVALID_HANDLE_VALUE); //Waits for all callback functions to complete with INVALID_HANDLE_VALUE
    }
};
typedef unique_handle<HANDLE, timer_queue_traits> timer_queue;

#endif