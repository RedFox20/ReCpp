#pragma once
/**
 * Lock-free atomic Duration and TimePoint, Copyright (c) 2026, Jorma Rebane
 * Distributed under MIT Software License
 *
 * Provides AtomicDuration and AtomicTimePoint as thin wrappers
 * inheriting from std::atomic<Duration> and std::atomic<TimePoint>.
 * Both types are 8 bytes (int64 nsec) and lock-free on 64-bit platforms.
 */
#include "timepoint.h"
#include <atomic>

namespace rpp
{
    //////////////////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Lock-free atomic Duration for thread-safe duration arithmetic.
     *
     * Inherits load/store/exchange/compare_exchange from std::atomic<Duration>.
     * Adds atomic += and -= operators via CAS loops, reusing Duration's own arithmetic.
     */
    struct AtomicDuration : std::atomic<Duration>
    {
        using base = std::atomic<Duration>;
        using base::base;      // inherit constructors: atomic(), atomic(Duration)
        using base::operator=; // inherit: Duration operator=(Duration)

        static_assert(base::is_always_lock_free,
            "std::atomic<Duration> must be lock-free (requires 64-bit platform)");

        /** @brief Atomically adds a Duration. @returns The new value after addition. */
        Duration operator+=(Duration d) noexcept
        {
            Duration old = load(std::memory_order_relaxed);
            while (!compare_exchange_weak(old, old + d,
                        std::memory_order_seq_cst,
                        std::memory_order_relaxed))
            {}
            return old + d;
        }

        /** @brief Atomically subtracts a Duration. @returns The new value after subtraction. */
        Duration operator-=(Duration d) noexcept
        {
            Duration old = load(std::memory_order_relaxed);
            while (!compare_exchange_weak(old, old - d,
                        std::memory_order_seq_cst,
                        std::memory_order_relaxed))
            {}
            return old - d;
        }

        /** @brief Atomically adds a Duration. @returns The OLD value before addition. */
        Duration fetch_add(Duration d, std::memory_order order = std::memory_order_seq_cst) noexcept
        {
            Duration old = load(std::memory_order_relaxed);
            while (!compare_exchange_weak(old, old + d, order, std::memory_order_relaxed))
            {}
            return old;
        }

        /** @brief Atomically subtracts a Duration. @returns The OLD value before subtraction. */
        Duration fetch_sub(Duration d, std::memory_order order = std::memory_order_seq_cst) noexcept
        {
            Duration old = load(std::memory_order_relaxed);
            while (!compare_exchange_weak(old, old - d, order, std::memory_order_relaxed))
            {}
            return old;
        }
    };

    //////////////////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Lock-free atomic TimePoint for thread-safe timepoint arithmetic.
     *
     * Inherits load/store/exchange/compare_exchange from std::atomic<TimePoint>.
     * Adds atomic += and -= operators (with Duration operand) via CAS loops.
     */
    struct AtomicTimePoint : std::atomic<TimePoint>
    {
        using base = std::atomic<TimePoint>;
        using base::base;      // inherit constructors: atomic(), atomic(TimePoint)
        using base::operator=; // inherit: TimePoint operator=(TimePoint)

        static_assert(base::is_always_lock_free,
            "std::atomic<TimePoint> must be lock-free (requires 64-bit platform)");

        /** @brief Atomically adds a Duration. @returns The new TimePoint after addition. */
        TimePoint operator+=(Duration d) noexcept
        {
            TimePoint old = load(std::memory_order_relaxed);
            while (!compare_exchange_weak(old, old + d,
                        std::memory_order_seq_cst,
                        std::memory_order_relaxed))
            {}
            return old + d;
        }

        /** @brief Atomically subtracts a Duration. @returns The new TimePoint after subtraction. */
        TimePoint operator-=(Duration d) noexcept
        {
            TimePoint old = load(std::memory_order_relaxed);
            while (!compare_exchange_weak(old, old - d,
                        std::memory_order_seq_cst,
                        std::memory_order_relaxed))
            {}
            return old - d;
        }
    };

    //////////////////////////////////////////////////////////////////////////////////////////

} // namespace rpp
