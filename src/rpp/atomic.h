#pragma once
/**
 * rpp::atomic, std::atomic with lock-free readers on every target, Copyright (c) 2026, Jorma Rebane
 * Distributed under MIT Software License
 *
 * rpp::atomic<T> is std::atomic<T> where that is always lock-free. An 8-byte T on a target
 * without lock-free 64-bit atomics, such as MIPS32, gets rpp::seqlock_atomic<T> in the same 8 bytes.
 */
#include "config.h" // RPPAPI, rpp::uint32
#include <atomic>
#include <bit> // std::bit_cast
#include <cstdint> // uintptr_t
#include <type_traits> // std::conditional_t, std::is_integral_v

namespace rpp
{
    /// The shared stripe pool and the seqlock steps of seqlock_atomic, which rpp::atomic uses without lock-free 64-bit atomics.
    namespace atomics
    {
        /**
         * @brief One stripe of the pool which each seqlock_atomic hashes its address into.
         *        version & 1: a writer holds the stripe. version & 2: the writer changes the words of owner,
         *        and old holds them as they were before the write.
         */
        struct seqlock_stripe
        {
            std::atomic<uint32> version;
            std::atomic<uint32> old[2];
            std::atomic<const void*> owner;
        };

        inline constexpr int SEQLOCK_STRIPES = 64;
        extern RPPAPI seqlock_stripe seqlock_stripes[SEQLOCK_STRIPES];

        /// @returns The stripe of the object at p.
        inline seqlock_stripe& seqlock_stripe_of(const void* p) noexcept
        {
            return seqlock_stripes[(reinterpret_cast<uintptr_t>(p) >> 3) % SEQLOCK_STRIPES];
        }

        /// @brief Waits until no writer holds the stripe, then takes it. @returns The version to give back.
        RPPAPI uint32 seqlock_lock(seqlock_stripe& stripe) noexcept;

        /// @brief Writes the two words of owner and gives the stripe back. Readers of owner take the old words meanwhile.
        RPPAPI void seqlock_write(seqlock_stripe& stripe, uint32 version, const void* owner,
                                  std::atomic<uint32>* words, uint32 word0, uint32 word1) noexcept;

        /// @brief Gives the stripe back at this version and wakes the writers which wait for it.
        RPPAPI void seqlock_unlock(seqlock_stripe& stripe, uint32 version) noexcept;
    } // namespace atomics

    /**
     * @brief The std::atomic interface for an 8-byte T in 8 bytes, built on 32-bit atomics. For an integral T it has
     *        fetch_add(), fetch_sub(), ++, --, += and -=, but no bitwise operations.
     *        A reader takes no lock and never waits for a writer: during a write, it takes the old value from the
     *        writer's stripe. Writers to the objects of one stripe wait for each other.
     *        Each operation is at least as strong as the memory order it asks for.
     */
    template<class T>
    class seqlock_atomic
    {
        static_assert(sizeof(T) == 8 && std::is_trivially_copyable_v<T>, "seqlock_atomic holds an 8-byte value");

        alignas(8) std::atomic<uint32> words[2]; // The low and the high half, aligned as std::atomic<int64>.

    public:
        static constexpr bool is_always_lock_free = false;

        constexpr seqlock_atomic() noexcept : seqlock_atomic{ T{} } {}
        constexpr seqlock_atomic(T value) noexcept
            : words{ uint32(std::bit_cast<uint64>(value)), uint32(std::bit_cast<uint64>(value) >> 32) } {}
        seqlock_atomic(const seqlock_atomic&) = delete;
        seqlock_atomic& operator=(const seqlock_atomic&) = delete;

        bool is_lock_free() const noexcept { return false; }

        T load(std::memory_order /*order*/ = std::memory_order_seq_cst) const noexcept
        {
            const atomics::seqlock_stripe& stripe = atomics::seqlock_stripe_of(this);
            for (;;)
            {
                const uint32 v = stripe.version.load(std::memory_order_seq_cst);
                const bool written = (v & 2) && stripe.owner.load(std::memory_order_relaxed) == this;
                const std::atomic<uint32>* src = written ? stripe.old : words;
                const uint64 low = src[0].load(std::memory_order_relaxed);
                const uint64 high = src[1].load(std::memory_order_relaxed);
                // This fence pairs with the release fences of seqlock_write(), so a torn read sees a new version.
                std::atomic_thread_fence(std::memory_order_acquire);
                if (stripe.version.load(std::memory_order_relaxed) == v) return std::bit_cast<T>(high << 32 | low);
            }
        }
        operator T() const noexcept { return load(); }

        void store(T value, std::memory_order /*order*/ = std::memory_order_seq_cst) noexcept
        {
            atomics::seqlock_stripe& stripe = atomics::seqlock_stripe_of(this);
            write(stripe, atomics::seqlock_lock(stripe), value);
        }
        T operator=(T value) noexcept { store(value); return value; }

        T exchange(T value, std::memory_order /*order*/ = std::memory_order_seq_cst) noexcept
        {
            atomics::seqlock_stripe& stripe = atomics::seqlock_stripe_of(this);
            const uint32 v = atomics::seqlock_lock(stripe);
            const T old = read();
            write(stripe, v, value);
            return old;
        }

        bool compare_exchange_strong(T& expected, T desired, std::memory_order /*success*/ = std::memory_order_seq_cst,
                                     std::memory_order /*failure*/ = std::memory_order_seq_cst) noexcept
        {
            atomics::seqlock_stripe& stripe = atomics::seqlock_stripe_of(this);
            const uint32 v = atomics::seqlock_lock(stripe);
            const T current = read();
            if (std::bit_cast<uint64>(current) == std::bit_cast<uint64>(expected))
            {
                write(stripe, v, desired);
                return true;
            }
            atomics::seqlock_unlock(stripe, v);
            expected = current;
            return false;
        }
        bool compare_exchange_weak(T& expected, T desired, std::memory_order success = std::memory_order_seq_cst,
                                   std::memory_order failure = std::memory_order_seq_cst) noexcept
        {
            return compare_exchange_strong(expected, desired, success, failure);
        }

        T fetch_add(T delta, std::memory_order /*order*/ = std::memory_order_seq_cst) noexcept requires std::is_integral_v<T>
        {
            atomics::seqlock_stripe& stripe = atomics::seqlock_stripe_of(this);
            const uint32 v = atomics::seqlock_lock(stripe);
            const T old = read();
            write(stripe, v, T(uint64(old) + uint64(delta))); // The unsigned sum wraps, as std::atomic does.
            return old;
        }
        T fetch_sub(T delta, std::memory_order order = std::memory_order_seq_cst) noexcept requires std::is_integral_v<T>
        {
            return fetch_add(T(0 - uint64(delta)), order);
        }
        // These return what the operators of std::atomic return, so one source compiles on every target.
        T operator++() noexcept requires std::is_integral_v<T> { return T(uint64(fetch_add(1)) + 1); }
        T operator++(int) noexcept requires std::is_integral_v<T> { return fetch_add(1); }
        T operator--() noexcept requires std::is_integral_v<T> { return T(uint64(fetch_sub(1)) - 1); }
        T operator--(int) noexcept requires std::is_integral_v<T> { return fetch_sub(1); }
        T operator+=(T delta) noexcept requires std::is_integral_v<T> { return T(uint64(fetch_add(delta)) + uint64(delta)); }
        T operator-=(T delta) noexcept requires std::is_integral_v<T> { return T(uint64(fetch_sub(delta)) - uint64(delta)); }

    private:
        /// @returns The value. The caller holds the stripe, so no writer changes it.
        T read() const noexcept
        {
            const uint64 low = words[0].load(std::memory_order_relaxed);
            return std::bit_cast<T>(uint64(words[1].load(std::memory_order_relaxed)) << 32 | low);
        }

        void write(atomics::seqlock_stripe& stripe, uint32 version, T value) noexcept
        {
            const uint64 bits = std::bit_cast<uint64>(value);
            atomics::seqlock_write(stripe, version, this, words, uint32(bits), uint32(bits >> 32));
        }
    };

    static_assert(sizeof(seqlock_atomic<int64>) == 8 && alignof(seqlock_atomic<int64>) == alignof(std::atomic<int64>),
                  "seqlock_atomic keeps the layout of std::atomic<int64>");

    /// std::atomic<T> where it is always lock-free, else seqlock_atomic<T> for an 8-byte T.
    template<class T>
    using atomic = std::conditional_t<std::atomic<T>::is_always_lock_free || sizeof(T) != 8, std::atomic<T>, seqlock_atomic<T>>;

    static_assert(sizeof(atomic<int64>) == 8, "rpp::atomic<int64> takes the 8 bytes of an int64 on every target");
} // namespace rpp
