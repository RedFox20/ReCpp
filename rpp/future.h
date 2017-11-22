#pragma once
/**
 * Minimal resumable future extensions for C++11 std::future, Copyright (c) 2017 - Jorma Rebane
 * Original implementation description by Herb Sutter at C++ and beyond 2012
 */
#include <memory> // shared_ptr
#include <functional>
#include <future>
#if __has_include("thread_pool.h")
#define RPP_FUTURE_USE_THREADPOOL 1
#include "thread_pool.h"
#else
#include <thread>
#endif

#ifndef NODISCARD
#define NODISCARD [[nodiscard]]
#endif

namespace rpp
{
    using namespace std;



    template<class T> class composable_future;


    template<class T, class Work>
    NODISCARD auto then(std::future<T> f, Work w) -> composable_future<decltype(w(f.get()))>
    {
        return std::async([](future<T> f, Work w)
        { 
            return w(f.get());
        }, move(f), move(w));
    }


    template<class Work>
    NODISCARD auto then(std::future<void> f, Work w) -> composable_future<decltype(w())>
    {
        return std::async([](future<void> f, Work w)
        { 
            f.get();
            return w();
        }, move(f), move(w));
    }


    template<class T, class Work, class Except>
    NODISCARD auto then(std::future<T> f, Work w, Except handler) -> composable_future<decltype(w(f.get()))>
    {
        return std::async([](future<T> f, Work w, Except handler)
        { 
            try
            {
                return w(f.get());
            }
            catch (const exception& e)
            {
                return handler(e);
            }
        }, move(f), move(w), move(handler));
    }


    template<class Work, class Except>
    NODISCARD auto then(std::future<void> f, Work w, Except handler) -> composable_future<decltype(w())>
    {
        return std::async([](future<void> f, Work w, Except handler)
        { 
            try
            {
                f.get();
                return w();
            }
            catch (const exception& e)
            {
                return handler(e);
            }
        }, move(f), move(w), move(handler));
    }


    template<class T, class Work> void continue_with(std::future<T> f, Work w)
    {
        #if RPP_FUTURE_USE_THREADPOOL
            struct state {
                std::future<T> f;
                Work w;
                state(std::future<T>&& f, Work&& w) : f(move(f)), w(move(w)) {}
            };
            rpp::parallel_task([s = make_shared<state>(move(f), move(w))]() mutable {
                s->w(s->f.get());
            });
        #else
            std::thread{[](std::future<T> f, Work w) {
                w(f.get());
            }, move(f), move(w)}.detach();
        #endif
    }


    template<class Work> void continue_with(std::future<void> f, Work w)
    {
        #if RPP_FUTURE_USE_THREADPOOL
            struct state {
                std::future<void> f;
                Work w;
                state(std::future<void>&& f, Work&& w) : f(move(f)), w(move(w)) {}
            };
            rpp::parallel_task([s = make_shared<state>(move(f), move(w))]() mutable {
                s->f.get();
                s->w();
            });
        #else
            std::thread{[](std::future<void> f, Work w) {
                f.get();
                w();
            }, move(f), move(w)}.detach();
        #endif
    }


    template<class T> class composable_future : public std::future<T>
    {
    public:
        composable_future(std::future<T>&& f) noexcept : std::future<T>(move(f))
        {
        }

        composable_future& operator=(std::future<T>&& f) noexcept
        {
            future<T>::operator=(move(f));
            return *this;
        }

        template<class Work> NODISCARD auto then(Work w)
        {
            return rpp::then(move(*this), move(w));
        }

        template<class Work, class Except> NODISCARD auto then(Work w, Except e)
        {
            return rpp::then(move(*this), move(w), move(e));
        }

        template<class Work> void continue_with(Work w)
        {
            rpp::continue_with(move(*this), move(w));
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
