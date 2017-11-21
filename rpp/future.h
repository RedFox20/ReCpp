#pragma once
/**
 * Minimal resumable future extensions for C++11 std::future, Copyright (c) 2017 - Jorma Rebane
 * Original implementation description by Herb Sutter at C++ and beyond 2012
 */
#include <memory> // shared_ptr
#include <functional>
#include <future>

namespace rpp
{
    using namespace std;


    template<class T> class composable_future;


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
    auto then(std::future<void> f, Work w, Except handler) -> composable_future<decltype(w())>
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


    template<class T> class composable_future : public std::future<T>
    {
    public:
        //using std::future::future;

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
