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
        AssertEqual(cv.wait_for(lock, ms(20)), std::cv_status::timeout);
        AssertGreater(t.elapsed_millis(), 10.0);
        AssertLess(t.elapsed_millis(), 100.0);
    }

    TestCase(wait_for_duration_notified)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;

        std::thread notifier(notify_after, std::ref(cv), 5);
        auto lock = std::unique_lock<rpp::mutex>{mtx};
        AssertEqual(cv.wait_for(lock, ms(200)), std::cv_status::no_timeout);
        notifier.join();
    }

    TestCase(wait_until_monotonic_timepoint_timeout)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;
        auto lock = std::unique_lock<rpp::mutex>{mtx};

        rpp::Timer t;
        AssertEqual(cv.wait_until(lock, rpp::TimePoint::monotonic_now() + ms(20)), std::cv_status::timeout);
        AssertGreater(t.elapsed_millis(), 10.0);
        AssertLess(t.elapsed_millis(), 100.0);
    }

    TestCase(wait_until_monotonic_timepoint_notified)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;

        std::thread notifier(notify_after, std::ref(cv), 5);
        auto lock = std::unique_lock<rpp::mutex>{mtx};
        AssertEqual(cv.wait_until(lock, rpp::TimePoint::monotonic_now() + ms(200)), std::cv_status::no_timeout);
        notifier.join();
    }

    TestCase(wait_until_past_deadline_returns_immediately)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;
        auto lock = std::unique_lock<rpp::mutex>{mtx};

        rpp::Timer t;
        AssertEqual(cv.wait_until(lock, rpp::TimePoint::monotonic_now() - ms(100)), std::cv_status::timeout);
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
            if (cv.wait_until(lock, deadline) == std::cv_status::timeout)
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

    TestCase(wait_for_zero_duration_returns_immediately)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;
        auto lock = std::unique_lock<rpp::mutex>{mtx};

        rpp::Timer t;
        AssertEqual(cv.wait_for(lock, rpp::Duration::zero()), std::cv_status::timeout);
        AssertLess(t.elapsed_millis(), 10.0);
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
                    if (cv.wait_until(lock, deadline) == std::cv_status::timeout)
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
};
