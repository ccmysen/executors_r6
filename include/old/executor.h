#ifndef EXECUTOR
#define EXECUTOR

#include <cassert>
#include <chrono>
#include <functional>
#include <memory>
#include <new>

using namespace std;

namespace detail
{
    class base_type
    {
    public:

        virtual void spawn(std::function<void(void)>) = 0;
        virtual std::unique_ptr<base_type> copy() const = 0;
        virtual ~base_type() { }
    };

    template <typename Executor>
    class implementation_type : public base_type
    {
    public:

        implementation_type(Executor executor)
            : _executor(std::move(executor))
        {
        }

        virtual void spawn(std::function<void(void)> f) override
        {
            return _executor.spawn(std::move(f));
        }

        virtual std::unique_ptr<base_type> copy() const override
        {
            return std::make_unique<implementation_type<Executor>>(*this);
        }

    private:

        Executor _executor;
    };

    class ref_base_type
    {
    public:

        virtual void spawn(std::function<void(void)>) = 0;
        virtual std::unique_ptr<base_type> copy_target() const = 0;
        virtual ~ref_base_type() { }
    };

    template <typename Executor>
    class ref_implementation_type : public ref_base_type
    {
    public:

        ref_implementation_type(Executor* executor)
            : _executor(executor)
        {
        }

        virtual void spawn(std::function<void(void)> f) override
        {
            _executor->spawn(std::move(f));
        }

        virtual std::unique_ptr<base_type> copy_target() const override
        {
            return std::make_unique<implementation_type<Executor>>(*_executor);
        }

    private:

        Executor* _executor;
    };
}

class abstract_executor_ref
{
public:

    template <typename Executor>
    abstract_executor_ref(Executor* const executor)
    {
        static_assert(sizeof(detail::ref_implementation_type<Executor>) == implementation_size, "uh oh");
        assert(executor);
        new (implementation()) detail::ref_implementation_type<Executor>(executor);
    }

    void spawn(std::function<void(void)> f)
    {
        return implementation()->spawn(std::move(f));
    }

private:

    friend class abstract_executor;

    enum : size_t { implementation_size = sizeof(void*)* 2 };

    using base_type = detail::ref_base_type;

    base_type*       implementation()       { return reinterpret_cast<base_type*>(&_implementation); }
    base_type const* implementation() const { return reinterpret_cast<base_type const*>(&_implementation); }

    std::aligned_storage_t<implementation_size> _implementation;
};

class abstract_executor
{
public:

    template <typename Executor>
    abstract_executor(Executor executor)
        : _implementation(std::make_unique<implementation_type<Executor>>(std::move(executor)))
    {
    }

    abstract_executor(abstract_executor_ref const executor_ref)
        : _implementation(executor_ref.implementation()->copy_target())
    {
    }

    abstract_executor(abstract_executor const& other)
        : _implementation(other._implementation->copy())
    {
    }

    abstract_executor(abstract_executor&& other)
        : _implementation(std::move(other._implementation))
    {
    }

    abstract_executor& operator=(abstract_executor other)
    {
        _implementation = std::move(other._implementation);
        return *this;
    }

    void spawn(std::function<void(void)> f)
    {
        _implementation->spawn(std::move(f));
    }

private:

    using base_type = detail::base_type;

    template <typename Executor>
    using implementation_type = detail::implementation_type<Executor>;

    std::unique_ptr<base_type> _implementation;
};

#endif
