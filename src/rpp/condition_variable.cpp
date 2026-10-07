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
    #include <climits>
#else
    #include <pthread.h>
    #include <cerrno>
    #include <cstdint>
    #include <ctime>
    #include <type_traits>
#endif

namespace rpp::cvar
{
    static FINLINE rpp::uint32 load_word(const void* addr) noexcept
    {
        return static_cast<const std::atomic_uint32_t*>(addr)->load(std::memory_order_relaxed);
    }

    // os_wait() sleeps once while the word equals `expected`, and a null timeout waits forever.
    // It can return early or without a wake, so wait() and wait_for() check the word again.
#if _MSC_VER

    // WaitOnAddress() takes whole milliseconds, so a partial millisecond rounds up
    static FINLINE DWORD wait_millis(rpp::Duration timeout) noexcept
    {
        rpp::int64 ms = timeout.nsec / NANOS_PER_MILLI + (timeout.nsec % NANOS_PER_MILLI > 0 ? 1 : 0);
        return ms >= INFINITE ? INFINITE - 1 : static_cast<DWORD>(ms);
    }

    static void os_wait(const void* addr, rpp::uint32 expected, const rpp::Duration* timeout) noexcept
    {
        if (!timeout)
        {
            WaitOnAddress(const_cast<void*>(addr), &expected, sizeof(expected), INFINITE);
            return;
        }
        // the default timer tick is ~15.6ms, so a short wait raises the timer resolution
        MMRESULT mmStatus = timeBeginPeriod(1);
        WaitOnAddress(const_cast<void*>(addr), &expected, sizeof(expected), wait_millis(*timeout));
        if (mmStatus == TIMERR_NOERROR)
            timeEndPeriod(1);
    }

    void wake_one(const void* addr) noexcept { WakeByAddressSingle(const_cast<void*>(addr)); }
    void wake_all(const void* addr) noexcept { WakeByAddressAll(const_cast<void*>(addr)); }

#elif RPP_BARE_METAL

    // without an RTOS, rpp::yield() sleeps in WFI, which misses an ISR that changed the word after the check
    static FINLINE void relax() noexcept
    {
    #if RPP_FREERTOS
        rpp::yield();
    #elif RPP_CORTEX_M_ARCH
        asm volatile("wfe"); // the notifier runs SEV, so a WFE after the notify returns at once
    #endif
    }

    // Cortex-M3 r1p1 does not set the event register on an exception return (erratum 563915), so SEV sets it
    static FINLINE void send_event() noexcept
    {
    #if !RPP_FREERTOS && RPP_CORTEX_M_ARCH
        asm volatile("sev");
    #endif
    }

    // no kernel wait queue here, so a waiter polls until the word changes
    static void os_wait(const void* addr, rpp::uint32 expected, const rpp::Duration* timeout) noexcept
    {
        const rpp::TimePoint deadline = timeout ? rpp::TimePoint::monotonic_now() + *timeout : rpp::TimePoint{};
        while (load_word(addr) == expected && (!timeout || rpp::TimePoint::monotonic_now() < deadline))
            relax();
    }

    void wake_one(const void*) noexcept { send_event(); }
    void wake_all(const void*) noexcept { send_event(); }

#elif __linux__ && !RPP_ADDRESS_WAIT_PARKING_LOT

    #if defined(SYS_futex)
        struct futex_timeout { long tv_sec; long tv_nsec; }; // the kernel timespec of SYS_futex, also on 32-bit
        constexpr long FUTEX_SYSCALL = SYS_futex;
    #else // a 32-bit target with a 64-bit time_t only
        struct futex_timeout { long long tv_sec; long long tv_nsec; };
        constexpr long FUTEX_SYSCALL = SYS_futex_time64;
    #endif

    static FINLINE long futex(const void* addr, int op, rpp::uint32 value, const futex_timeout* timeout) noexcept
    {
        return syscall(FUTEX_SYSCALL, addr, op, value, timeout, nullptr, 0);
    }

    static void os_wait(const void* addr, rpp::uint32 expected, const rpp::Duration* timeout) noexcept
    {
        if (!timeout)
        {
            (void)futex(addr, FUTEX_WAIT_PRIVATE, expected, nullptr);
            return;
        }
        // a relative FUTEX_WAIT timeout runs on CLOCK_MONOTONIC
        futex_timeout ts { static_cast<long>(timeout->nsec / NANOS_PER_SEC), static_cast<long>(timeout->nsec % NANOS_PER_SEC) };
        (void)futex(addr, FUTEX_WAIT_PRIVATE, expected, &ts);
    }

    void wake_one(const void* addr) noexcept { (void)futex(addr, FUTEX_WAKE_PRIVATE, 1, nullptr); }
    void wake_all(const void* addr) noexcept { (void)futex(addr, FUTEX_WAKE_PRIVATE, INT_MAX, nullptr); }

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

        // no destructor runs at exit, so a thread can still wait while the static destructors run
        static_assert(std::is_trivially_destructible_v<parking_lot>);
        parking_lot& lot() noexcept
        {
            static parking_lot instance;
            return instance;
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

    static void os_wait(const void* addr, rpp::uint32 expected, const rpp::Duration* timeout) noexcept
    {
        parking_bucket& b = lot().bucket(addr);
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
                pthread_cond_timedwait_relative_np(&b.cond, &b.mutex, &rel);
            #else
                timespec abs {};
                clock_gettime(CLOCK_MONOTONIC, &abs);
                ns += abs.tv_nsec;
                abs.tv_sec += static_cast<time_t>(ns / NANOS_PER_SEC);
                abs.tv_nsec = static_cast<long>(ns % NANOS_PER_SEC);
                pthread_cond_timedwait(&b.cond, &b.mutex, &abs);
            #endif
            }
            --b.waiters;
        }
        pthread_mutex_unlock(&b.mutex);
    }

    void wake_one(const void* addr) noexcept { unpark_all(addr); }
    void wake_all(const void* addr) noexcept { unpark_all(addr); }

#endif

    void wait(const void* addr, rpp::uint32 expected) noexcept
    {
        while (load_word(addr) == expected)
            os_wait(addr, expected, nullptr);
    }

    bool wait_for(const void* addr, rpp::uint32 expected, rpp::Duration timeout) noexcept
    {
        // a Windows wait can end one timer tick early, so the deadline decides the timeout
        const rpp::TimePoint deadline = rpp::TimePoint::monotonic_now() + timeout;
        while (load_word(addr) == expected)
        {
            rpp::Duration left = deadline - rpp::TimePoint::monotonic_now();
            if (left.nsec <= 0)
                return false;
            os_wait(addr, expected, &left);
        }
        return true;
    }
}

namespace rpp
{
    // the pause hint frees the core for its other hyperthread while this one spins
    static FINLINE void cpu_pause() noexcept
    {
    #if _MSC_VER
        YieldProcessor();
    #elif __x86_64__ || __i386__
        __builtin_ia32_pause();
    #elif __aarch64__ || (__arm__ && __ARM_ARCH >= 7)
        asm volatile("yield" ::: "memory");
    #endif
    }

    // a kernel sleep and wake costs more than a short critical section, so a locker first waits with loads only.
    // A CONTENDED word already has a sleeper, so a spin there only delays this sleep
    void futex_mutex::lock_slow() noexcept
    {
        static constexpr int SPIN_ROUNDS = 100;
        auto spin = [this] {
            rpp::uint32 state = word.load(std::memory_order_relaxed);
            for (int i = 0; state == LOCKED && i < SPIN_ROUNDS; ++i)
            {
                cpu_pause();
                state = word.load(std::memory_order_relaxed);
            }
            return state;
        };
        rpp::uint32 state = spin();
        if (state == UNLOCKED && try_take())
            return;
        // a woken locker cannot know if more lockers sleep, so it takes the lock as CONTENDED
        while (state == CONTENDED || word.exchange(CONTENDED, std::memory_order_acquire) != UNLOCKED)
        {
            cvar::wait(&word, CONTENDED);
            state = spin();
        }
    }
}
