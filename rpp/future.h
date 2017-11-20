#pragma once
/**
 * Minimal resumable future extensions for C++11 std::future, Copyright (c) 2017 - Jorma Rebane
 * Original implementation description by Herb Sutter
 *
 */
#include <memory> // shared_ptr
#include <functional>
#include <future>

namespace rpp
{
    using namespace std;

    template<class T> class composable_future;

    template<class T, class Work>
    auto then(future<T> f, Work w) -> composable_future<decltype(w(f.get()))>;

    template<class Work>
    auto then(future<void> f, Work w) -> composable_future<decltype(w())>;


    template<class T> class composable_future : public future<T>
    {
    public:
        using future::future;

        composable_future(future<T>&& f) noexcept : future<T>(move(f))
        {
        }

        composable_future& operator=(future<T>&& f) noexcept
        {
            future<T>::operator=(move(f));
            return *this;
        }

        template<class Work> auto then(Work w)
        {
            return rpp::then(move(*this), move(w));
        }
    };


    template<class T, class Work>
    auto then(future<T> f, Work w) -> composable_future<decltype(w(f.get()))>
    {
        return std::async([](future<T> f, Work w)
        { 
            return w(f.get());
        }, move(f), move(w));
    }

    template<class Work>
    auto then(future<void> f, Work w) -> composable_future<decltype(w())>
    {
        return std::async([](future<void> f, Work w)
        { 
            f.get();
            return w();
        }, move(f), move(w));
    }
}
