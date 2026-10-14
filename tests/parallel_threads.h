#pragma once
#include <thread>
#include <type_traits>
#include <vector>

/// Runs body(i), or body() when it takes no index, on n threads. join() and the destructor wait for all of them
struct parallel_threads
{
    std::vector<std::thread> threads;

    template<class Body> parallel_threads(int n, Body body)
    {
        threads.reserve(n);
        for (int i = 0; i < n; ++i)
        {
            if constexpr (std::is_invocable_v<Body, int>) threads.emplace_back(body, i);
            else threads.emplace_back(body);
        }
    }
    ~parallel_threads() { join(); }
    parallel_threads(const parallel_threads&) = delete;
    parallel_threads& operator=(const parallel_threads&) = delete;

    /// Joins every thread which still runs
    void join()
    {
        for (std::thread& t : threads)
            if (t.joinable()) t.join();
    }
};
