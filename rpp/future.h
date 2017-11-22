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

namespace rpp
{
    using namespace std;

    template<class T> class NODISCARD composable_future;

    ////////////////////////////////////////////////////////////////////////////////


    template<class T, class Work>
    auto then(std::future<T> f, Work w) -> composable_future<decltype(w(f.get()))>
    {
        return std::async([](future<T> f, Work w)
        { 
            return w(f.get());
        }, move(f), move(w));
    }


    template<class Work>
    auto then(std::future<void> f, Work w) -> composable_future<decltype(w())>
    {
        return std::async([](future<void> f, Work w)
        { 
            f.get();
            return w();
        }, move(f), move(w));
    }


    template<class T, class Work, class Except>
    auto then(std::future<T> f, Work w, Except handler) -> composable_future<decltype(w(f.get()))>
    {
        return std::async([](future<T> f, Work w, Except handler)
        { 
            try {
                return w(f.get());
            } catch (exception& e) {
                return handler(e);
            }
        }, move(f), move(w), move(handler));
    }


    template<class Work, class Except>
    auto then(std::future<void> f, Work w, Except handler) -> composable_future<decltype(w())>
    {
        return std::async([](future<void> f, Work w, Except handler)
        { 
            try {
                f.get();
                return w();
            } catch (exception& e) {
                return handler(e);
            }
        }, move(f), move(w), move(handler));
    }


    ////////////////////////////////////////////////////////////////////////////////


    template<class T, class Work> void continue_with(std::future<T> f, Work w)
    {
        auto lambda = [](std::future<T> f, Work w) {
            w(f.get());
        };
        #if RPP_FUTURE_USE_THREADPOOL
            rpp::parallel_task(move(lambda), move(f), move(w));
        #else
            std::thread{move(lambda), move(f), move(w)}.detach();
        #endif
    }


    template<class Work> void continue_with(std::future<void> f, Work w)
    {
        auto lambda = [](std::future<void> f, Work w) {
            f.get();
            w();
        };
        #if RPP_FUTURE_USE_THREADPOOL
            rpp::parallel_task(move(lambda), move(f), move(w));
        #else
            std::thread{move(lambda), move(f), move(w)}.detach();
        #endif
    }


    template<class T, class Work, class Except> void continue_with(std::future<T> f, Work w, Except handler)
    {
        auto lambda = [](std::future<T> f, Work w, Except handler) {
            try {
                return w(f.get());
            } catch (exception& e) {
                return handler(e);
            }
        };
        #if RPP_FUTURE_USE_THREADPOOL
            rpp::parallel_task(move(lambda), move(f), move(w), move(handler));
        #else
            std::thread{move(lambda), move(f), move(w), move(handler)}.detach();
        #endif
    }


    template<class Work, class Except> void continue_with(std::future<void> f, Work w, Except handler)
    {
        auto lambda = [](std::future<void> f, Work w, Except handler) {
            try {
                f.get();
                return w();
            } catch (exception& e) {
                return handler(e);
            }
        };
        #if RPP_FUTURE_USE_THREADPOOL
            rpp::parallel_task(move(lambda), move(f), move(w), move(handler));
        #else
            std::thread{move(lambda), move(f), move(w), move(handler)}.detach();
        #endif
    }


    ////////////////////////////////////////////////////////////////////////////////


    template<class T> class NODISCARD composable_future : public std::future<T>
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

        template<class Work> auto then(Work w)
        {
            return rpp::then(move(*this), move(w));
        }

        template<class Work, class Except> auto then(Work w, Except e)
        {
            return rpp::then(move(*this), move(w), move(e));
        }

        // similar to .then(), but doesn't return a future
        template<class Work> void continue_with(Work w)
        {
            rpp::continue_with(move(*this), move(w));
        }

        template<class Work, class Except> void continue_with(Work w, Except e)
        {
            rpp::continue_with(move(*this), move(w), move(e));
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
