#pragma once
#include <rpp/threads.h> // rpp::yield
#include <atomic>
#include <thread>
#include <vector>

inline constexpr int TORN_WRITERS = 2;
inline constexpr int TORN_READERS = 2;
inline constexpr int TORN_WRITES = 10000;

/// @brief Calls write(j) TORN_WRITES times on each writer while the readers call is_torn().
///        The readers start first, so every write lands while they read.
/// @returns The count of reads which is_torn() flagged.
template<class Write, class IsTorn>
int count_torn_reads(Write write, IsTorn is_torn)
{
    std::atomic<int> readers_ready { 0 };
    std::atomic<int> writers_left { TORN_WRITERS };
    std::atomic<int> torn { 0 };
    std::vector<std::thread> threads;
    threads.reserve(TORN_READERS + TORN_WRITERS);
    for (int i = 0; i < TORN_READERS; ++i)
    {
        threads.emplace_back([&]
        {
            ++readers_ready;
            while (writers_left > 0) if (is_torn()) ++torn;
        });
    }
    for (int i = 0; i < TORN_WRITERS; ++i)
    {
        threads.emplace_back([&]
        {
            while (readers_ready < TORN_READERS) rpp::yield();
            for (int j = 0; j < TORN_WRITES; ++j) write(j);
            --writers_left;
        });
    }
    for (std::thread& t : threads) t.join();
    return torn;
}
