#include <rpp/atomic.h>
#include <rpp/semaphore.h>
#include <rpp/tests.h>
#include <rpp/timepoint.h> // rpp::Duration, rpp::seconds
#include "torn_reads.h"
#include <thread>
#include <type_traits> // std::is_same_v
using namespace rpp;

// A target with lock-free 64-bit atomics keeps std::atomic, so its code stays the same.
static_assert(!std::atomic<int64>::is_always_lock_free || std::is_same_v<rpp::atomic<int64>, std::atomic<int64>>);
static_assert(std::is_same_v<rpp::atomic<int>, std::atomic<int>>);

TestImpl(test_atomic)
{
    TestInit(test_atomic)
    {
    }

    TestCase(seqlock_atomic_has_the_operations_of_std_atomic)
    {
        seqlock_atomic<int64> a { 5 };
        AssertThat(a.load(), 5);
        a.store(-1);
        AssertThat(a.exchange(0x1'0000'0002LL), -1);
        int64 expected = 7;
        AssertFalse(a.compare_exchange_strong(expected, 8));
        AssertThat(expected, 0x1'0000'0002LL);
        AssertTrue(a.compare_exchange_weak(expected, 8));
        AssertThat(a.fetch_sub(9), 8);
        AssertThat(a.fetch_add(1), -1); // The low word carries into the high word.
        AssertThat(int64(a), 0);
        AssertThat(++a, 1); // The operators let one source build on MIPS32 and where rpp::atomic is std::atomic.
        AssertThat(a++, 1);
        AssertThat(--a, 1);
        AssertThat(a--, 1);
        AssertThat(a += 0x1'0000'0000LL, 0x1'0000'0000LL);
        AssertThat(a -= 1, 0xFFFF'FFFFLL); // The high word borrows from the low word.
    }

    // The writes swap two values which differ in every bit of both words, so a torn read is neither value.
    TestCase(seqlock_atomic_reader_never_sees_a_torn_value)
    {
        seqlock_atomic<Duration> dur { Duration{ 0 } };
        const int torn = count_torn_reads([&](int j)
        {
            dur.store(Duration{ j % 2 ? -1 : 0 });
            Duration expected { -1 };
            dur.compare_exchange_strong(expected, Duration{ 0 });
            dur.exchange(Duration{ -1 });
        }, [&]
        {
            const int64 ns = dur.load().nsec;
            return ns != 0 && ns != -1;
        });
        AssertThat(torn, 0);

        seqlock_atomic<int64> sum { 0 };
        const int64 step = 0x1'0000'0001LL; // Each multiple of the step has equal words, until the low word carries.
        const int torn_sums = count_torn_reads([&](int) { sum.fetch_add(step); }, [&]
        {
            const uint64 value = uint64(sum.load());
            return (value >> 32) != (value & 0xFFFF'FFFF);
        });
        AssertThat(torn_sums, 0);
        AssertThat(sum.load(), step * TORN_WRITERS * TORN_WRITES); // No writer lost an addition.
    }

    // Objects SEQLOCK_STRIPES apart share a stripe, so a reader of one runs during the writes of the other.
    TestCase(seqlock_atomic_objects_of_one_stripe_keep_their_own_values)
    {
        static seqlock_atomic<int64> objects[atomics::SEQLOCK_STRIPES + 1];
        seqlock_atomic<int64>& a = objects[0];
        seqlock_atomic<int64>& b = objects[atomics::SEQLOCK_STRIPES];
        AssertThat(&atomics::seqlock_stripe_of(&a), &atomics::seqlock_stripe_of(&b));

        const int64 B0 = 0x1111'1111'2222'2222LL;
        const int64 B1 = 0x3333'3333'4444'4444LL;
        a.store(0);
        b.store(B0);
        const int torn = count_torn_reads([&](int j)
        {
            a.store(j % 2 ? -1 : 0);
            b.store(j % 2 ? B1 : B0);
        }, [&]
        {
            const int64 x = a.load();
            const int64 y = b.load();
            return (x != 0 && x != -1) || (y != B0 && y != B1);
        });
        AssertThat(torn, 0);
    }

    // Puts the stripe of `a` in the state of a writer of `owner` which saved `old`, and loads `a` in another thread.
    // Returns the value, or -1 when the load did not end within a second.
    static int64 load_during_write(seqlock_atomic<int64>& a, const void* owner, int64 old)
    {
        atomics::seqlock_stripe& stripe = atomics::seqlock_stripe_of(&a);
        const uint32 v = atomics::seqlock_lock(stripe);
        stripe.owner.store(owner);
        stripe.old[0].store(uint32(old));
        stripe.old[1].store(uint32(uint64(old) >> 32));
        stripe.version.store(v | 3);

        rpp::semaphore loaded;
        int64 seen = 0;
        std::thread reader([&] { seen = a.load(); loaded.notify(); });
        const bool ended = loaded.wait(rpp::seconds(1)) == rpp::semaphore::notified;
        atomics::seqlock_unlock(stripe, v);
        reader.join();
        return ended ? seen : -1;
    }

    // A writer which stops in the middle of a write delays no reader.
    TestCase(seqlock_atomic_reader_takes_the_old_value_during_a_write)
    {
        seqlock_atomic<int64> a { 1 };
        const int64 other = 0;
        AssertThat(load_during_write(a, &a, 3), 3); // A reader of the object takes the old value which the stripe holds.
        AssertThat(load_during_write(a, &other, 3), 1); // A reader of another object of the stripe takes its own words.
        AssertThat(a.load(), 1);
    }
};
