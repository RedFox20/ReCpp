#include <rpp/condition_variable.h>
#include <rpp/semaphore.h>
#include <rpp/timer.h>
#include <rpp/tests.h>
#include <thread>

constexpr rpp::Duration ms(int milliseconds)
{
    return rpp::Duration::from_millis(milliseconds);
}

TestImpl(test_condition_variable)
{
    TestInit(test_condition_variable)
    {
    }

    // helper: notify cv from a background thread after a short delay
    static void notify_after(rpp::condition_variable& cv, int delay_ms)
    {
        rpp::sleep_ms(delay_ms);
        cv.notify_one();
    }

    // helper: set ready=true under lock, then notify
    static void set_ready_and_notify(rpp::condition_variable& cv, rpp::mutex& mtx, bool& ready, int delay_ms)
    {
        rpp::sleep_ms(delay_ms);
        { std::lock_guard lock{mtx}; ready = true; }
        cv.notify_one();
    }

    TestCase(wait_for_duration_timeout)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;
        auto lock = std::unique_lock<rpp::mutex>{mtx};

        rpp::Timer t;
        AssertEqual(cv.wait_for(lock, ms(20)), rpp::cv_status::timeout);
        AssertGreater(t.elapsed_millis(), 10.0);
        AssertLess(t.elapsed_millis(), 100.0);
    }

    TestCase(wait_for_duration_notified)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;

        std::thread notifier(notify_after, std::ref(cv), 5);
        auto lock = std::unique_lock<rpp::mutex>{mtx};
        AssertEqual(cv.wait_for(lock, ms(200)), rpp::cv_status::no_timeout);
        notifier.join();
    }

    TestCase(wait_until_monotonic_timepoint_timeout)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;
        auto lock = std::unique_lock<rpp::mutex>{mtx};

        rpp::Timer t;
        AssertEqual(cv.wait_until(lock, rpp::TimePoint::monotonic_now() + ms(20)), rpp::cv_status::timeout);
        AssertGreater(t.elapsed_millis(), 10.0);
        AssertLess(t.elapsed_millis(), 100.0);
    }

    TestCase(wait_until_monotonic_timepoint_notified)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;

        std::thread notifier(notify_after, std::ref(cv), 5);
        auto lock = std::unique_lock<rpp::mutex>{mtx};
        AssertEqual(cv.wait_until(lock, rpp::TimePoint::monotonic_now() + ms(200)), rpp::cv_status::no_timeout);
        notifier.join();
    }

    TestCase(wait_until_past_deadline_returns_immediately)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;
        auto lock = std::unique_lock<rpp::mutex>{mtx};

        rpp::Timer t;
        AssertEqual(cv.wait_until(lock, rpp::TimePoint::monotonic_now() - ms(100)), rpp::cv_status::timeout);
        AssertLess(t.elapsed_millis(), 10.0);
    }

    // Regression: repeated wait_until calls with same deadline
    // should correctly track remaining time across spurious wakeups
    TestCase(repeated_wait_until_tracks_remaining_time)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;
        int wakeup_count = 0;
        bool ready = false;

        std::thread notifier([&] {
            for (int i = 0; i < 3; ++i) {
                rpp::sleep_ms(3);
                cv.notify_one(); // spurious (ready still false)
            }
            rpp::sleep_ms(3);
            { std::lock_guard lock{mtx}; ready = true; }
            cv.notify_one();
        });

        auto lock = std::unique_lock<rpp::mutex>{mtx};
        auto deadline = rpp::TimePoint::monotonic_now() + ms(200);
        while (!ready)
        {
            if (cv.wait_until(lock, deadline) == rpp::cv_status::timeout)
                break;
            ++wakeup_count;
        }

        notifier.join();
        AssertTrue(ready);
        AssertGreaterOrEqual(wakeup_count, 1);
    }

    TestCase(wait_for_predicate_timeout)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;
        auto lock = std::unique_lock<rpp::mutex>{mtx};

        rpp::Timer t;
        AssertFalse(cv.wait_for(lock, ms(20), [] { return false; }));
        AssertGreater(t.elapsed_millis(), 10.0);
    }

    TestCase(wait_for_predicate_notified)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;
        bool ready = false;

        std::thread notifier(set_ready_and_notify, std::ref(cv), std::ref(mtx), std::ref(ready), 5);
        auto lock = std::unique_lock<rpp::mutex>{mtx};
        AssertTrue(cv.wait_for(lock, ms(200), [&] { return ready; }));
        notifier.join();
    }

    TestCase(wait_until_predicate_timeout)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;
        auto lock = std::unique_lock<rpp::mutex>{mtx};

        rpp::Timer t;
        AssertFalse(cv.wait_until(lock, rpp::TimePoint::monotonic_now() + ms(20), [] { return false; }));
        AssertGreater(t.elapsed_millis(), 10.0);
    }

    TestCase(wait_until_predicate_notified)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;
        bool ready = false;

        std::thread notifier(set_ready_and_notify, std::ref(cv), std::ref(mtx), std::ref(ready), 5);
        auto lock = std::unique_lock<rpp::mutex>{mtx};
        AssertTrue(cv.wait_until(lock, rpp::TimePoint::monotonic_now() + ms(200), [&] { return ready; }));
        notifier.join();
    }

    // a wait needs only lock() and unlock(), so a plain mutex works as the lock
    TestCase(wait_takes_any_lock)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;
        bool ready = false;

        std::thread notifier(set_ready_and_notify, std::ref(cv), std::ref(mtx), std::ref(ready), 5);
        mtx.lock();
        AssertTrue(cv.wait_for(mtx, ms(1000), [&] { return ready; })); // a hang guard, the notifier releases it
        mtx.unlock();
        notifier.join();
    }

    // a lock type which exposes a futex_mutex still gets its own lock() and unlock() calls
    TestCase(a_wait_calls_lock_and_unlock_of_a_custom_lock)
    {
        struct counting_lock
        {
            rpp::futex_mutex& m;
            int locks = 0;
            int unlocks = 0;
            rpp::futex_mutex* mutex() const noexcept { return &m; }
            void lock() noexcept { ++locks; m.lock(); }
            void unlock() noexcept { ++unlocks; m.unlock(); }
        };
        rpp::condition_variable cv;
        rpp::futex_mutex m;
        counting_lock lock { m };
        m.lock();
        (void)cv.wait_for(lock, rpp::millis(5)); // nothing notifies, so the wait times out
        m.unlock();
        AssertThat(lock.unlocks, 1);
        AssertThat(lock.locks, 1);
    }

    TestCase(a_wait_relocks_through_the_lock_of_a_derived_mutex)
    {
        struct counting_mutex : rpp::futex_mutex
        {
            int locks = 0;
            void lock() noexcept { ++locks; futex_mutex::lock(); }
        };
        rpp::condition_variable cv;
        counting_mutex m;
        std::unique_lock lock { m };
        (void)cv.wait_for(lock, rpp::millis(5)); // nothing notifies, so the wait times out
        AssertThat(m.locks, 2);
    }

    TestCase(wait_for_zero_duration_returns_immediately)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;
        auto lock = std::unique_lock<rpp::mutex>{mtx};

        rpp::Timer t;
        AssertEqual(cv.wait_for(lock, rpp::Duration::zero()), rpp::cv_status::timeout);
        AssertLess(t.elapsed_millis(), 10.0);
    }

    // WaitOnAddress() takes whole milliseconds and can wake a timer tick early
    TestCase(a_timed_wait_never_ends_before_its_timeout)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;
        auto lock = std::unique_lock<rpp::mutex>{mtx};
        for (int i = 0; i < 3; ++i)
        {
            rpp::Timer t;
            AssertEqual(cv.wait_for(lock, rpp::micros(1900)), rpp::cv_status::timeout);
            AssertGreaterOrEqual(t.elapsed_millis(), 1.9);
        }
    }

    TestCase(wait_for_negative_duration_returns_immediately)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;
        auto lock = std::unique_lock<rpp::mutex>{mtx};
        rpp::Timer t;
        AssertEqual(cv.wait_for(lock, rpp::millis(-20)), rpp::cv_status::timeout);
        AssertLess(t.elapsed_millis(), 10.0);
    }

    // a notify with no waiter makes no RMW, so a wait which starts after it must still sleep
    TestCase(a_notify_without_waiters_does_not_wake_a_later_wait)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;
        cv.notify_one();
        cv.notify_all();
        auto lock = std::unique_lock<rpp::mutex>{mtx};
        rpp::Timer t;
        AssertEqual(cv.wait_for(lock, rpp::millis(3)), rpp::cv_status::timeout);
        AssertGreaterOrEqual(t.elapsed_millis(), 3.0);
    }

    TestCase(cvar_wait_returns_at_once_when_the_word_already_changed)
    {
        std::atomic_uint32_t word { 1 };
        rpp::cvar::wake_one(&word); // no thread sleeps on it
        rpp::cvar::wake_all(&word);
        rpp::Timer t;
        rpp::cvar::wait(&word, 0);
        AssertTrue(rpp::cvar::wait_for(&word, 0, rpp::seconds(1)));
        AssertFalse(rpp::cvar::wait_for(&word, 1, rpp::Duration::zero()));
        AssertFalse(rpp::cvar::wait_for(&word, 1, rpp::millis(-20)));
        AssertLess(t.elapsed_millis(), 10.0);
    }

    TestCase(cvar_wait_for_returns_true_when_another_thread_changes_the_word)
    {
        std::atomic_uint32_t word { 0 };
        std::thread changer([&] { rpp::sleep_ms(3); word.store(1); rpp::cvar::wake_all(&word); });
        rpp::Timer t;
        AssertTrue(rpp::cvar::wait_for(&word, 0, rpp::seconds(1)));
        AssertLess(t.elapsed_millis(), 200.0);
        changer.join();
    }

    TestCase(multiple_waiters_all_notified)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;
        std::atomic_int notified_count{0};
        bool ready = false;

        constexpr int NUM_WAITERS = 4;
        std::vector<std::thread> waiters;
        waiters.reserve(NUM_WAITERS);
        for (int i = 0; i < NUM_WAITERS; ++i)
        {
            waiters.emplace_back([&] {
                auto lock = std::unique_lock<rpp::mutex>{mtx};
                auto deadline = rpp::TimePoint::monotonic_now() + ms(200);
                while (!ready)
                    if (cv.wait_until(lock, deadline) == rpp::cv_status::timeout)
                        return;
                ++notified_count;
            });
        }

        rpp::sleep_ms(10);
        { std::lock_guard lock{mtx}; ready = true; }
        cv.notify_all();

        for (auto& t : waiters) t.join();
        AssertEqual(notified_count.load(), NUM_WAITERS);
    }

    // the first field of the class is the word its threads sleep on
    template<class T> static rpp::uint32 word_of(const T& sync)
    {
        static_assert(std::is_standard_layout_v<T>);
        return reinterpret_cast<const std::atomic_uint32_t*>(&sync)->load();
    }

    // a waiter counts itself under the lock, so this thread sees the count only after the waiter released the lock to wait
    static void lock_once_waiting(std::unique_lock<rpp::futex_mutex>& lock, const int& waiting, int count)
    {
        while (waiting < count) { lock.unlock(); rpp::yield(); lock.lock(); }
    }

    TestCase(futex_mutex_excludes_concurrent_lockers)
    {
        rpp::futex_mutex m;
        int counter = 0;
        std::vector<std::thread> threads;
        threads.reserve(4);
        m.lock(); // the lockers start blocked, so every run takes the contended path
        for (int t = 0; t < 4; ++t)
        {
            threads.emplace_back([&] {
                for (int i = 0; i < 1'000; ++i)
                {
                    std::lock_guard guard { m };
                    ++counter;
                }
            });
        }
        while (word_of(m) != 2u) // a locker marked the mutex contended, and sleeps until the unlock
            rpp::yield();
        m.unlock();
        for (std::thread& t : threads)
            t.join();
        AssertThat(counter, 4'000);
        AssertThat(word_of(m), 0u);
        AssertThat(m.try_lock(), true);
        AssertThat(m.try_lock(), false);
        m.unlock();
    }

    // a woken waiter leaves the count only once it runs, so a notify before that has nobody new to wake
    TestCase(a_notify_skips_the_wake_when_every_waiter_has_one)
    {
        for (void (rpp::condition_variable::*notify)() : { &rpp::condition_variable::notify_one,
                                                           &rpp::condition_variable::notify_all })
        {
            rpp::condition_variable cv;
            rpp::futex_mutex m;
            int waiting = 0;
            std::thread waiter([&] {
                std::unique_lock lock { m };
                ++waiting;
                (void)cv.wait_for(lock, rpp::seconds(1)); // a hang guard, the notify below releases it
            });
            std::unique_lock lock { m };
            lock_once_waiting(lock, waiting, 1);
            rpp::sleep_ms(5); // lets the waiter fall asleep, so it leaves the wait only after a wake from the kernel
            const rpp::uint32 seq = word_of(cv);
            for (int i = 0; i < 100; ++i)
                (cv.*notify)();
            AssertThat(word_of(cv) - seq, 1u); // the waiter cannot wait again while this thread holds the lock
            lock.unlock();
            waiter.join();
        }
    }

    TestCase(each_notify_one_wakes_its_own_waiter)
    {
        rpp::condition_variable cv;
        rpp::futex_mutex m;
        int waiting = 0;
        auto wait_1s = [&] {
            std::unique_lock lock { m };
            ++waiting;
            (void)cv.wait_for(lock, rpp::seconds(1)); // a hang guard, a notify below releases it
        };
        std::thread a { wait_1s };
        std::thread b { wait_1s };
        std::unique_lock lock { m };
        lock_once_waiting(lock, waiting, 2);
        rpp::sleep_ms(5); // lets both waiters fall asleep, so each one needs its own wake
        rpp::Timer t;
        cv.notify_one();
        cv.notify_one();
        lock.unlock();
        a.join();
        b.join();
        AssertLess(t.elapsed_millis(), 500.0); // a waiter which missed its wake returns only at the hang guard
    }

    // a notify between the count and a later seq read would count the waiter, and leave seq as the waiter reads it
    TestCase(a_notify_while_a_waiter_registers_still_wakes_it)
    {
        rpp::condition_variable cv;
        rpp::futex_mutex m;
        std::atomic_bool stop { false };
        std::thread notifier([&] { while (!stop) cv.notify_one(); });
        int timeouts = 0;
        for (rpp::Timer t; t.elapsed_millis() < 20.0 && timeouts == 0; )
        {
            std::unique_lock lock { m };
            if (cv.wait_for(lock, rpp::seconds(1)) == rpp::cv_status::timeout) // a hang guard, the notifier never stops
                ++timeouts;
        }
        stop = true;
        notifier.join();
        AssertThat(timeouts, 0);
    }

    // the relock marks the word contended, so the next unlock wakes a locker which still sleeps
    TestCase(a_wait_relocks_a_futex_mutex_as_contended)
    {
        for (bool timed : { false, true })
        {
            rpp::condition_variable cv;
            rpp::futex_mutex m;
            int waiting = 0;
            rpp::uint32 word_after_wait = 0;
            std::thread waiter([&] {
                std::unique_lock lock { m };
                ++waiting;
                if (timed)
                    (void)cv.wait_for(lock, rpp::seconds(1)); // a hang guard, the notify below releases it
                else
                    cv.wait(lock);
                word_after_wait = word_of(m);
            });
            std::unique_lock lock { m };
            lock_once_waiting(lock, waiting, 1);
            lock.unlock();
            cv.notify_one(); // after the unlock, so the waiter relocks a free mutex
            waiter.join();
            AssertThat(word_after_wait, 2u);
        }
    }
};
