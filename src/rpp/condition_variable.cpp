#include "condition_variable.h"
#include <atomic>
/**
 * The address wait under rpp::condition_variable and the semaphore flags.
 *
 * Copyright (c) 2023-2026, Jorma Rebane
 * Distributed under MIT Software License
 */
#if _MSC_VER
    #define WIN32_LEAN_AND_MEAN 1
    #include <Windows.h>
    #include <timeapi.h>
    #pragma comment(lib, "Winmm.lib") // timeBeginPeriod
    #pragma comment(lib, "Synchronization.lib") // WaitOnAddress
#elif RPP_BARE_METAL
    #include "threads.h" // rpp::yield
#elif __linux__ && !RPP_ADDRESS_WAIT_PARKING_LOT // a test-only switch, which runs the Apple and Emscripten path on Linux
    #include <linux/futex.h>
    #include <sys/syscall.h>
    #include <unistd.h>
    #include <cerrno>
    #include <climits>
#else
    #include <pthread.h>
    #include <cerrno>
    #include <cstdint>
    #include <ctime>
#endif

namespace rpp::detail
{
#if _MSC_VER

    void address_wait(const void* addr, rpp::uint32 expected) noexcept
    {
        WaitOnAddress(const_cast<void*>(addr), &expected, sizeof(expected), INFINITE);
    }

    bool address_wait_for(const void* addr, rpp::uint32 expected, rpp::Duration timeout) noexcept
    {
        if (timeout.nsec <= 0)
            return false;
        // the default timer tick is ~15.6ms, so a short wait raises the timer resolution
        MMRESULT mmStatus = timeBeginPeriod(1);
        rpp::int64 ms = timeout.nsec / 1'000'000;
        DWORD wait_ms = ms <= 0 ? 1 : ms >= INFINITE ? INFINITE - 1 : static_cast<DWORD>(ms);
        BOOL woken = WaitOnAddress(const_cast<void*>(addr), &expected, sizeof(expected), wait_ms);
        bool timed_out = !woken && GetLastError() == ERROR_TIMEOUT;
        if (mmStatus == TIMERR_NOERROR)
            timeEndPeriod(1);
        return !timed_out;
    }

    void address_wake_one(const void* addr) noexcept { WakeByAddressSingle(const_cast<void*>(addr)); }
    void address_wake_all(const void* addr) noexcept { WakeByAddressAll(const_cast<void*>(addr)); }

#elif RPP_BARE_METAL

    static rpp::uint32 load_word(const void* addr) noexcept
    {
        return static_cast<const std::atomic<rpp::uint32>*>(addr)->load(std::memory_order_relaxed);
    }

    // no kernel wait queue here, so a waiter yields until the word changes
    void address_wait(const void* addr, rpp::uint32 expected) noexcept
    {
        while (load_word(addr) == expected)
            rpp::yield();
    }

    bool address_wait_for(const void* addr, rpp::uint32 expected, rpp::Duration timeout) noexcept
    {
        const rpp::TimePoint deadline = rpp::TimePoint::monotonic_now() + timeout;
        while (load_word(addr) == expected)
        {
            if (rpp::TimePoint::monotonic_now() >= deadline)
                return false;
            rpp::yield();
        }
        return true;
    }

    void address_wake_one(const void*) noexcept {}
    void address_wake_all(const void*) noexcept {}

#elif __linux__ && !RPP_ADDRESS_WAIT_PARKING_LOT

    #if defined(SYS_futex)
        struct futex_timeout { long tv_sec; long tv_nsec; }; // the kernel timespec of SYS_futex, also on 32-bit
        constexpr long FUTEX_SYSCALL = SYS_futex;
    #else // a 32-bit target with a 64-bit time_t only
        struct futex_timeout { long long tv_sec; long long tv_nsec; };
        constexpr long FUTEX_SYSCALL = SYS_futex_time64;
    #endif

    static long futex(const void* addr, int op, rpp::uint32 value, const futex_timeout* timeout) noexcept
    {
        return syscall(FUTEX_SYSCALL, addr, op, value, timeout, nullptr, 0);
    }

    void address_wait(const void* addr, rpp::uint32 expected) noexcept
    {
        (void)futex(addr, FUTEX_WAIT_PRIVATE, expected, nullptr);
    }

    bool address_wait_for(const void* addr, rpp::uint32 expected, rpp::Duration timeout) noexcept
    {
        if (timeout.nsec <= 0)
            return false;
        // a relative FUTEX_WAIT timeout runs on CLOCK_MONOTONIC
        futex_timeout ts { static_cast<long>(timeout.nsec / NANOS_PER_SEC), static_cast<long>(timeout.nsec % NANOS_PER_SEC) };
        return futex(addr, FUTEX_WAIT_PRIVATE, expected, &ts) == 0 || errno != ETIMEDOUT;
    }

    void address_wake_one(const void* addr) noexcept { (void)futex(addr, FUTEX_WAKE_PRIVATE, 1, nullptr); }
    void address_wake_all(const void* addr) noexcept { (void)futex(addr, FUTEX_WAKE_PRIVATE, INT_MAX, nullptr); }

#else

    namespace
    {
        // waiters on different words can share a bucket, so a wake is always a broadcast
        struct parking_bucket
        {
            pthread_mutex_t mutex;
            pthread_cond_t cond;
            int waiters;
        };

        struct parking_lot
        {
            static constexpr int BITS = 6;
            parking_bucket buckets[1 << BITS];

            parking_lot() noexcept
            {
                pthread_condattr_t attr;
                pthread_condattr_init(&attr);
            #if !__APPLE__ // Apple waits with a relative timeout instead
                pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
            #endif
                for (parking_bucket& b : buckets)
                {
                    pthread_mutex_init(&b.mutex, nullptr);
                    pthread_cond_init(&b.cond, &attr);
                    b.waiters = 0;
                }
                pthread_condattr_destroy(&attr);
            }

            parking_bucket& bucket(const void* addr) noexcept
            {
                rpp::uint64 hash = static_cast<rpp::uint64>(reinterpret_cast<uintptr_t>(addr)) * 0x9E3779B97F4A7C15ull;
                return buckets[hash >> (64 - BITS)]; // a multiplicative hash mixes best into its top bits
            }
        };

        // never freed, because a thread can still wait while the static destructors run
        parking_lot& lot() noexcept
        {
            static parking_lot* p = new parking_lot{};
            return *p;
        }

        rpp::uint32 load_word(const void* addr) noexcept
        {
            return static_cast<const std::atomic<rpp::uint32>*>(addr)->load(std::memory_order_relaxed);
        }

        // @returns false when the timeout elapsed. A null timeout waits forever
        bool park(const void* addr, rpp::uint32 expected, const rpp::Duration* timeout) noexcept
        {
            parking_bucket& b = lot().bucket(addr);
            bool woken = true;
            pthread_mutex_lock(&b.mutex);
            // the waker locks this mutex after it changed the word, so the check and the sleep are atomic for it
            if (load_word(addr) == expected)
            {
                ++b.waiters;
                if (!timeout)
                {
                    pthread_cond_wait(&b.cond, &b.mutex);
                }
                else
                {
                    rpp::int64 ns = timeout->nsec;
                #if __APPLE__
                    timespec rel { static_cast<time_t>(ns / NANOS_PER_SEC), static_cast<long>(ns % NANOS_PER_SEC) };
                    woken = pthread_cond_timedwait_relative_np(&b.cond, &b.mutex, &rel) != ETIMEDOUT;
                #else
                    timespec abs {};
                    clock_gettime(CLOCK_MONOTONIC, &abs);
                    ns += abs.tv_nsec;
                    abs.tv_sec += static_cast<time_t>(ns / NANOS_PER_SEC);
                    abs.tv_nsec = static_cast<long>(ns % NANOS_PER_SEC);
                    woken = pthread_cond_timedwait(&b.cond, &b.mutex, &abs) != ETIMEDOUT;
                #endif
                }
                --b.waiters;
            }
            pthread_mutex_unlock(&b.mutex);
            return woken;
        }

        void unpark_all(const void* addr) noexcept
        {
            parking_bucket& b = lot().bucket(addr);
            pthread_mutex_lock(&b.mutex);
            if (b.waiters > 0)
                pthread_cond_broadcast(&b.cond);
            pthread_mutex_unlock(&b.mutex);
        }
    }

    void address_wait(const void* addr, rpp::uint32 expected) noexcept
    {
        (void)park(addr, expected, nullptr);
    }

    bool address_wait_for(const void* addr, rpp::uint32 expected, rpp::Duration timeout) noexcept
    {
        return timeout.nsec > 0 && park(addr, expected, &timeout);
    }

    void address_wake_one(const void* addr) noexcept { unpark_all(addr); }
    void address_wake_all(const void* addr) noexcept { unpark_all(addr); }

#endif
}
