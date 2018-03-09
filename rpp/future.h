#pragma once
/**
 * Minimal resumable future extensions for C++11 std::future, Copyright (c) 2017-2018, Jorma Rebane
 * Distributed under MIT Software License
 * @note Original implementation description by Herb Sutter at C++ and beyond 2012
 */
#include <memory> // shared_ptr
#include <future>
#if __has_include("thread_pool.h")
    #define CFUTURE_USE_RPP_THREAD_POOL 1
    #include "thread_pool.h"
    #include <type_traits>
#else
    #include <thread>
#endif

#ifndef NODISCARD
    #if __clang__
        #if __clang_major__ >= 4 || (__clang_major__ == 3 && __clang_minor__ == 9) // since 3.9
            #define NODISCARD [[nodiscard]]
        #else
            #define NODISCARD // not supported in clang <= 3.8
        #endif
    #else
        #define NODISCARD [[nodiscard]]
    #endif
#endif

#ifndef FINLINE
#  ifdef _MSC_VER
#    define FINLINE __forceinline
#  elif __APPLE__
#    define FINLINE inline __attribute__((always_inline))
#  else
#    define FINLINE __attribute__((always_inline))
#  endif
#endif

namespace rpp
{
    using std::future;
    using std::promise;
    using std::exception;
    using std::move;

    template<class T> class NODISCARD cfuture;

    ////////////////////////////////////////////////////////////////////////////////


    template<class T> struct cpromise : public std::promise<T>
    {
        template<class Task> void compose(Task& task)
        {
            T value = task();
            // run task destructor before calling continuation
            // provides deterministic sequencing: DownloadAndSaveFile().then(OpenAndParseFile);
            if (!std::is_trivially_destructible_v<Task>) {
                Task t = move(task);
            }
            std::promise<T>::set_value(move(value));
        }
    };

    template<> struct cpromise<void> : public std::promise<void>
    {
        template<class Task> void compose(Task& task)
        {
            task();
            // run task destructor before calling continuation
            // provides deterministic sequencing: DownloadAndSaveFile().then(OpenAndParseFile);
            if (!std::is_trivially_destructible_v<Task>) {
                Task t = move(task);
            }
            std::promise<void>::set_value();
        }
    };


#if CFUTURE_USE_RPP_THREAD_POOL
    template<class Task>
    auto async_task(Task&& task) -> cfuture<decltype(task())>
    {
        using T = decltype(task());
        cpromise<T> p;
        future<T> f = p.get_future();
        rpp::parallel_task([move_args(p, task)]() mutable
        {
            try {
                p.compose(task);
            } catch (...) {
                p.set_exception(std::current_exception());
            }
        });
        return f;
    }
#else // std::async is incredibly slower
    template<class Task>
    auto async_task(Task&& task) -> cfuture<decltype(task())>
    {
        return std::async(std::launch::async, [move_args(task)]() mutable
        {
            return task();
        });
    }
#endif


    ////////////////////////////////////////////////////////////////////////////////


    template<class T> class NODISCARD cfuture : public future<T>
    {
    public:
        cfuture(future<T>&& f) noexcept : future<T>(move(f))
        {
        }
        cfuture(cfuture&& f) noexcept : future<T>(move(f))
        {
        }
        cfuture& operator=(future<T>&& f) noexcept
        {
            future<T>::operator=(move(f));
            return *this;
        }
        cfuture& operator=(cfuture&& f) noexcept
        {
            future<T>::operator=(move(f));
            return *this;
        }
        ~cfuture() noexcept // always block if future is still incomplete
        {
            if (this->valid())
                this->wait();
        }

        template<class Task>
        auto then(Task&& task) -> cfuture<decltype(task(get()))>
        {
            return rpp::async_task([f=move(*this), move_args(task)]() mutable {
                return task(f.get());
            });
        }

        template<class Task, class Except>
        auto then(Task&& task, Except&& handler) -> cfuture<decltype(task(get()))>
        {
            return async_task([f=move(*this), move_args(task, handler)]() mutable {
                try { return task(f.get()); } 
                catch (std::exception& e) { return handler(e); }
            });
        }

        // similar to .then(), but doesn't return a future
        template<class Task>
        void continue_with(Task&& task)
        {
            rpp::parallel_task([f=move(*this), move_args(task)]() mutable {
                (void)task(f.get());
            });
        }

        template<class Task, class Except>
        void continue_with(Task&& task, Except&& handler)
        {
            rpp::parallel_task([f=move(*this), move_args(task, handler)]() mutable {
                try { (void)task(f.get()); }
                catch (std::exception& e) { (void)handler(e); }
            });
        }
    };


    template<> class NODISCARD cfuture<void> : public future<void>
    {
    public:
        cfuture(future<void>&& f) noexcept : future<void>(move(f))
        {
        }
        cfuture(cfuture&& f) noexcept : future<void>(move(f))
        {
        }
        cfuture& operator=(future<void>&& f) noexcept
        {
            future<void>::operator=(move(f));
            return *this;
        }
        cfuture& operator=(cfuture&& f) noexcept
        {
            future<void>::operator=(move(f));
            return *this;
        }
        ~cfuture() noexcept // always block if future is still incomplete
        {
            if (this->valid())
                this->wait();
        }

        template<class Task>
        auto then(Task&& task) -> cfuture<decltype(task())>
        {
            return rpp::async_task([f=move(*this), move_args(task)]() mutable {
                f.get();
                return task();
            });
        }

        template<class Task, class Except>
        auto then(Task&& task, Except&& handler) -> cfuture<decltype(task())>
        {
            return async_task([f=move(*this), move_args(task, handler)]() mutable {
                try {
                    f.get();
                    return task();
                } catch (std::exception& e) { return handler(e); }
            });
        }

        // similar to .then(), but doesn't return a future
        template<class Task>
        void continue_with(Task&& task)
        {
            rpp::parallel_task([f=move(*this), move_args(task)]() mutable {
                f.get();
                (void)task();
            });
        }

        template<class Task, class Except>
        void continue_with(Task&& task, Except&& handler)
        {
            rpp::parallel_task([f=move(*this), move_args(task, handler)]() mutable {
                try {
                    f.get();
                    (void)task();
                } catch (std::exception& e) { (void)handler(e); }
            });
        }
    };


    template<class T> cfuture<T> make_ready_future(T&& value)
    {
        promise<T> p;
        p.set_value(move(value));
        return p.get_future();
    }

    template<class T, class E> cfuture<T> make_exceptional_future(E&& e)
    {
        promise<T> p;
        p.set_exception(make_exception_ptr(std::forward<E>(e)));
        return p.get_future();
    }
}
