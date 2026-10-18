/**
 * The stripe pool of rpp::seqlock_atomic, Copyright (c) 2026, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "atomic.h"

namespace rpp::atomics
{
    seqlock_stripe seqlock_stripes[SEQLOCK_STRIPES];

    uint32 seqlock_lock(seqlock_stripe& stripe) noexcept
    {
        uint32 v = stripe.version.load(std::memory_order_relaxed);
        for (;;)
        {
            if (v & 1) // Another writer holds the stripe.
            {
            #if __cpp_lib_atomic_wait // A libstdc++ without threads or futexes, as on bare metal, has no wait().
                stripe.version.wait(v, std::memory_order_relaxed);
            #endif
                v = stripe.version.load(std::memory_order_relaxed);
            }
            else if (stripe.version.compare_exchange_weak(v, v | 1, std::memory_order_acquire, std::memory_order_relaxed))
            {
                return v;
            }
        }
    }

    void seqlock_write(seqlock_stripe& stripe, uint32 version, const void* owner,
                       std::atomic<uint32>* words, uint32 word0, uint32 word1) noexcept
    {
        // This fence orders the lock before the stores below, for a reader of the old words of an earlier write.
        std::atomic_thread_fence(std::memory_order_release);
        stripe.owner.store(owner, std::memory_order_relaxed);
        stripe.old[0].store(words[0].load(std::memory_order_relaxed), std::memory_order_relaxed);
        stripe.old[1].store(words[1].load(std::memory_order_relaxed), std::memory_order_relaxed);
        stripe.version.store(version | 3, std::memory_order_release); // Readers of owner take the old words now.
        // This fence orders version | 3 before the new words, for a reader of the new words.
        std::atomic_thread_fence(std::memory_order_release);
        words[0].store(word0, std::memory_order_relaxed);
        words[1].store(word1, std::memory_order_relaxed);
        seqlock_unlock(stripe, version + 4);
    }

    void seqlock_unlock(seqlock_stripe& stripe, uint32 version) noexcept
    {
        stripe.version.store(version, std::memory_order_seq_cst);
    #if __cpp_lib_atomic_wait
        stripe.version.notify_all();
    #endif
    }
} // namespace rpp::atomics
