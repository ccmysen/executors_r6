#ifndef THREAD_UTIL
#define THREAD_UTIL

#include <windows.h>
#include <functional>
#include <chrono>
#include <ctime>

template <typename Type, typename Traits>
class unique_handle {
    struct boolean_struct { int member; };
    typedef int boolean_struct::* boolean_type;

    unique_handle(unique_handle const &);
    Type m_value;

    bool operator==(unique_handle const &);
    bool operator!=(unique_handle const &);
    unique_handle & operator=(unique_handle const &);

    void close() throw() {
        if (*this) {
            Traits::close(m_value);
        }
    }

public:

    explicit unique_handle(Type value = Traits::invalid()) throw() : m_value(value) {}

    ~unique_handle() throw() {
        close();
    }

    unique_handle(unique_handle && other) throw() : m_value(other.release()) {}

    Type get() const throw() {
        return m_value;
    }

    bool reset(Type value = Traits::invalid()) throw() {
        if (m_value != value) {
            close();
            m_value = value;
        }

        return *this;
    }

    Type release() throw() {
        auto value = m_value;
        m_value = Traits::invalid();
        return value;
    }

    /*Miscellaneous functions */
    operator boolean_type() const throw() {
        return Traits::invalid() != m_value ? &boolean_struct::member : nullptr;
    }

    unique_handle & operator=(unique_handle && other) throw() {
        reset(other.release());
        return *this;
    }
};

class environment {
    environment(environment const &);
    environment & operator=(environment const &);

    TP_CALLBACK_ENVIRON m_value;

public:

    environment() throw() {
        InitializeThreadpoolEnvironment(&m_value);
    }

    ~environment() throw() {
        DestroyThreadpoolEnvironment(&m_value);
    }

    PTP_CALLBACK_ENVIRON get() throw() {
        return &m_value;
    }
};

inline DWORD absolute_to_relative_milli(const chrono::system_clock::time_point& abs_time) {
    using namespace std::chrono;

    time_t present;

    // convert to time_point:
    time_t future = system_clock::to_time_t(abs_time);
    time(&present);

    return (DWORD)(1000 * difftime(future, present));
}

inline DWORD relative_to_relative_milli(const chrono::system_clock::duration& rel_time) {
    using namespace std::chrono;
    typedef std::chrono::duration<DWORD, std::ratio<1, 1000>> milli;

    return duration_cast<milli>(rel_time).count();
}

#endif