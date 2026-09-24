#pragma once
/**
 * Lock-free atomic Duration and TimePoint, Copyright (c) 2026, Jorma Rebane
 * Distributed under MIT Software License
 *
 * Provides AtomicDuration and AtomicTimePoint as thin wrappers
 * inheriting from std::atomic<Duration> and std::atomic<TimePoint>.
 * Both types are 8 bytes (int64 nsec) and lock-free on 64-bit platforms.
 */
#include "config.types.h" // rpp::int64, rpp::byte
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
    struct RPPAPI AtomicDuration : std::atomic<Duration>
    {
        using base = std::atomic<Duration>;
        using base::base;      // inherit constructors: atomic(), atomic(Duration)
        using base::operator=; // inherit: Duration operator=(Duration)

    #if !MIPS // mips is 32-bit, so this will not be atomic
        static_assert(base::is_always_lock_free,
            "std::atomic<Duration> must be lock-free (requires 64-bit platform)");
    #endif // !MIPS

        // default initialize to zero, because it's not the default for atomics
        AtomicDuration() noexcept : base{Duration::zero()}
        {
        }

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
    struct RPPAPI AtomicTimePoint : std::atomic<TimePoint>
    {
        using base = std::atomic<TimePoint>;
        using base::base;      // inherit constructors: atomic(), atomic(TimePoint)
        using base::operator=; // inherit: TimePoint operator=(TimePoint)

    #if !MIPS // mips is 32-bit, so this will not be atomic
        static_assert(base::is_always_lock_free,
            "std::atomic<TimePoint> must be lock-free (requires 64-bit platform)");
    #endif // !MIPS

        // default initialize to zero, because it's not the default for atomics
        AtomicTimePoint() noexcept : base{TimePoint::zero()}
        {
        }

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

    /**
     * @brief A customizable time source for atomically advancing simulation time
     *        in a lock free manner.
     *        Additionally supports time offsets used for time syncing with external time sources.
     */
    class RPPAPI AtomicTimeSource
    {
        // combines sync and warp offsets atomically
        std::atomic<rpp::int64> combined_offset_ns {};

        // sync offset to apply to current time
        std::atomic<rpp::int64> sync_offset_ns {};

        // time warp offset to apply to current time, can be used for time warping in simulations
        std::atomic<rpp::int64> warp_offset_ns {};

        // Realtime follows each wall clock step, and Monotonic keeps waits and timers through it
        std::atomic<rpp::ClockType> base_clock { rpp::ClockType::Realtime };

    public:

        /**
         * @returns The virtual time according to this time source,
         *           which is the base clock time plus the combined offset (sync + warp).
         */
        rpp::TimePoint time_now() const noexcept
        {
            rpp::TimePoint base_now = base_time_now();
            rpp::int64 offset_ns = combined_offset_ns.load(std::memory_order_relaxed);
            return rpp::TimePoint{ base_now.duration.nsec + offset_ns };
        }

        /**
         * @returns The virtual time with ONLY warp offset applied, ignoring sync offset.
         *          The sync offset is typically used for time syncing.
         */
        rpp::TimePoint time_unsynced() const noexcept
        {
            rpp::TimePoint base_local = base_time_now();
            rpp::int64 warp_ns = warp_offset_ns.load(std::memory_order_relaxed);
            return rpp::TimePoint{ base_local.duration.nsec + warp_ns };
        }

        /**
         * @returns The total offset (sync + warp) currently applied to the time source, 
         *          which can be added to a TimePoint to get the current time.
         */
        rpp::Duration total_offset() const noexcept
        {
            return rpp::Duration{ combined_offset_ns.load(std::memory_order_relaxed) };
        }
        
        /**
         * @returns The current warp offset applied to the time source.
         * @note Does not guarantee atomic consistency with total_offset()
         *       use for diagnostic purposes only.
         */
        rpp::Duration warp_offset() const noexcept
        {
            return rpp::Duration{ warp_offset_ns.load(std::memory_order_relaxed) };
        }

        /**
         * @returns The current sync offset applied to the time source.
         * @note Does not guarantee atomic consistency with total_offset()
         *       use for diagnostic purposes only.
         */
        rpp::Duration sync_offset() const noexcept
        {
            return rpp::Duration{ sync_offset_ns.load(std::memory_order_relaxed) };
        }

        /** @returns The time of the base clock, without the sync and warp offsets */
        rpp::TimePoint base_time_now() const noexcept { return rpp::TimePoint::now(base_clock.load(std::memory_order_relaxed)); }

        /** @returns The clock under the sync and warp offsets, Realtime by default */
        rpp::ClockType get_base_clock() const noexcept { return base_clock.load(std::memory_order_relaxed); }

        /**
         * @brief Sets the clock under the sync and warp offsets. Set it before the first read.
         *        Monotonic ignores a wall clock step, so a wait or a timer does not stall.
         */
        void set_base_clock(rpp::ClockType clock) noexcept { base_clock.store(clock, std::memory_order_relaxed); }

        /**
         * @brief Warps time forward atomically by the given delta
         */
        void warp_forward(rpp::Duration delta) noexcept
        {
            combined_offset_ns.fetch_add(delta.nsec, std::memory_order_seq_cst);
            warp_offset_ns.fetch_add(delta.nsec, std::memory_order_seq_cst);
        }

        /**
         * @brief Warps time backward atomically by the given delta
         */
        void warp_backward(rpp::Duration delta) noexcept
        {
            combined_offset_ns.fetch_sub(delta.nsec, std::memory_order_seq_cst);
            warp_offset_ns.fetch_sub(delta.nsec, std::memory_order_seq_cst);
        }

        /**
         * @brief Sets the sync offset to the given value
         */
        void set_sync_offset(rpp::Duration new_offset) noexcept
        {
            rpp::int64 old_offset = sync_offset_ns.exchange(new_offset.nsec, std::memory_order_seq_cst);
            
            // update combined offset by the difference between new and old sync offsets
            // to avoid another fetch of warp_offset_ns
            rpp::int64 offset_diff = new_offset.nsec - old_offset;
            combined_offset_ns.fetch_add(offset_diff, std::memory_order_seq_cst);
        }

        /**
         * @brief Sets the warp offset to the given value
         */
        void set_warp_offset(rpp::Duration new_offset) noexcept
        {
            rpp::int64 old_offset = warp_offset_ns.exchange(new_offset.nsec, std::memory_order_seq_cst);

            // update combined offset by the difference between new and old warp offsets
            // to avoid another fetch of sync_offset_ns
            rpp::int64 offset_diff = new_offset.nsec - old_offset;
            combined_offset_ns.fetch_add(offset_diff, std::memory_order_seq_cst);
        }
    };

    //////////////////////////////////////////////////////////////////////////////////////////

} // namespace rpp
