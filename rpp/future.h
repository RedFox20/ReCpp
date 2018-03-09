#pragma once
/**
 * Minimal resumable future extensions for C++11 std::future, Copyright (c) 2017-2018, Jorma Rebane
 * Distributed under MIT Software License
 * @note Original implementation description by Herb Sutter at C++ and beyond 2012
 */
#include <memory> // shared_ptr
#include <future>
#include "thread_pool.h"

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

    template<class T> class NODISCARD composable_future;

    ////////////////////////////////////////////////////////////////////////////////

    template<class T, class Task> struct promise_setter
    {
        static FINLINE void set(promise<T>& p, Task& task)
        {
            p.set_value(task());
        }
    };
    template<class Task> struct promise_setter<void, Task>
    {
        static FINLINE void set(promise<void>& p, Task& task)
        {
            task();
            p.set_value();
        }
    };

    template<class T, class Task> struct compose_task
    {
        static FINLINE auto get(future<T>& f, Task& task) -> decltype(task(f.get()))
        {
            return task(f.get());
        }
        template<class Except>
        static FINLINE auto get(future<T>& f, Task& task, Except& handler) -> decltype(task(f.get()))
        {
            try { 
                return task(f.get());
            } catch (std::exception& e) { return handler(e); }
        }
    };
    template<class Task> struct compose_task<void, Task>
    {
        static FINLINE auto get(future<void>& f, Task& task) -> decltype(task())
        {
            f.get();
            return task();
        }
        template<class Except>
        static FINLINE auto get(future<void>& f, Task& task, Except& handler) -> decltype(task())
        {
            try {
                f.get();
                return task();
            } catch (std::exception& e) { return handler(e); }
        }
    };

    ////////////////////////////////////////////////////////////////////////////////
    
    template<class Task>
    auto async_task(Task&& task) -> composable_future<decltype(task())>
    {
        using T = decltype(task());
        promise<T> p;
        future<T> f = p.get_future();
        rpp::parallel_task([move_args(p, task)]() mutable {
            try {
                promise_setter<T, Task>::set(p, task);
            } catch (...) {
                p.set_exception(std::current_exception());
            }
        });
        return f;
    }

    template<class T, class Work>
    auto then(future<T> f, Work w) -> composable_future<decltype(compose_task<T, Work>::get(f, w))>
    {
        return rpp::async_task([move_args(f, w)]() mutable {
            return compose_task<T, Work>::get(f, w);
        });
    }

    template<class T, class Work, class Except>
    auto then(future<T> f, Work w, Except handler) -> composable_future<decltype(compose_task<T, Work>::get(f, w, handler))>
    {
        return rpp::async_task([move_args(f, w, handler)]() mutable {
            return compose_task<T, Work>::get(f, w, handler);
        });
    }

    template<class T, class Work> void continue_with(future<T> f, Work w)
    {
        rpp::parallel_task([move_args(f, w)]() mutable {
            compose_task<T, Work>::get(f, w);
        });
    }

    template<class T, class Work, class Except> void continue_with(future<T> f, Work w, Except handler)
    {
        rpp::parallel_task([move_args(f, w, handler)]() mutable {
            compose_task<T, Work>::get(f, w, handler);
        });
    }

    ////////////////////////////////////////////////////////////////////////////////


    template<class T> class NODISCARD composable_future : public future<T>
    {
    public:
        composable_future(future<T>&& f) noexcept : future<T>(move(f))
        {
        }
        composable_future(composable_future&& f) noexcept : future<T>(move(f))
        {
        }
        composable_future& operator=(future<T>&& f) noexcept
        {
            future<T>::operator=(move(f));
            return *this;
        }
        composable_future& operator=(composable_future&& f) noexcept
        {
            future<T>::operator=(move(f));
            return *this;
        }
        ~composable_future() noexcept // always block if future is still incomplete
        {
            if (this->valid())
                this->wait();
        }

        template<class Work>
        auto then(Work w) -> composable_future<decltype(compose_task<T, Work>::get(*this, w))>
        {
            return rpp::async_task([f=move(*this), move_args(w)]() mutable {
                return compose_task<T, Work>::get(f, w);
            });
        }

        template<class Work, class Except> auto then(
            Work w, Except handler) -> composable_future<decltype(compose_task<T, Work>::get(*this, w, handler))>
        {
            return async_task([f=move(*this), move_args(w, handler)]() mutable {
                return compose_task<T, Work>::get(f, w, handler);
            });
        }

        // similar to .then(), but doesn't return a future
        template<class Work> void continue_with(Work w)
        {
            rpp::parallel_task([f=move(*this), move_args(w)]() mutable {
                compose_task<T, Work>::get(f, w);
            });
        }

        template<class Work, class Except> void continue_with(
            Work w, Except handler)
        {
            rpp::parallel_task([f=move(*this), move_args(w, handler)]() mutable {
                compose_task<T, Work>::get(f, w, handler);
            });
        }
    };


    template<class T> composable_future<T> make_ready_future(T&& value)
    {
        promise<T> p;
        p.set_value(move(value));
        return p.get_future();
    }

    template<class T, class E> composable_future<T> make_exceptional_future(E&& e)
    {
        promise<T> p;
        p.set_exception(make_exception_ptr(std::forward<E>(e)));
        return p.get_future();
    }
}
