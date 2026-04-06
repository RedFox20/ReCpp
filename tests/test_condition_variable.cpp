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

    // Verify that wait_for with rpp::Duration correctly times out
    TestCase(wait_for_duration_timeout)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;
        auto lock = std::unique_lock<rpp::mutex>{mtx};

        rpp::Timer timer;
        auto result = cv.wait_for(lock, ms(50));
        double elapsed = timer.elapsed_ms();

        AssertEqual(result, std::cv_status::timeout);
        AssertGreater(elapsed, 30.0); // should wait at least ~50ms (with some tolerance)
        AssertLess(elapsed, 200.0);   // should not wait forever
    }

    // Verify that wait_for with rpp::Duration wakes up on notify
    TestCase(wait_for_duration_notified)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;

        std::thread notifier([&] {
            rpp::sleep_ms(30);
            cv.notify_one();
        });

        auto lock = std::unique_lock<rpp::mutex>{mtx};
        rpp::Timer timer;
        auto result = cv.wait_for(lock, ms(500));
        double elapsed = timer.elapsed_ms();

        notifier.join();
        AssertEqual(result, std::cv_status::no_timeout);
        AssertLess(elapsed, 200.0); // should wake up well before timeout
    }

    // Verify that wait_until with rpp::TimePoint (monotonic) correctly times out
    TestCase(wait_until_monotonic_timepoint_timeout)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;
        auto lock = std::unique_lock<rpp::mutex>{mtx};

        auto deadline = rpp::TimePoint::monotonic_now() + ms(50);
        rpp::Timer timer;
        auto result = cv.wait_until(lock, deadline);
        double elapsed = timer.elapsed_ms();

        AssertEqual(result, std::cv_status::timeout);
        AssertGreater(elapsed, 30.0);
        AssertLess(elapsed, 200.0);
    }

    // Verify that wait_until with rpp::TimePoint wakes up on notify
    TestCase(wait_until_monotonic_timepoint_notified)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;

        std::thread notifier([&] {
            rpp::sleep_ms(30);
            cv.notify_one();
        });

        auto lock = std::unique_lock<rpp::mutex>{mtx};
        auto deadline = rpp::TimePoint::monotonic_now() + ms(500);
        rpp::Timer timer;
        auto result = cv.wait_until(lock, deadline);
        double elapsed = timer.elapsed_ms();

        notifier.join();
        AssertEqual(result, std::cv_status::no_timeout);
        AssertLess(elapsed, 200.0);
    }

    // Verify that wait_until with a past deadline returns timeout immediately
    TestCase(wait_until_past_deadline_returns_immediately)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;
        auto lock = std::unique_lock<rpp::mutex>{mtx};

        auto past = rpp::TimePoint::monotonic_now() - ms(100);
        rpp::Timer timer;
        auto result = cv.wait_until(lock, past);
        double elapsed = timer.elapsed_ms();

        AssertEqual(result, std::cv_status::timeout);
        AssertLess(elapsed, 20.0); // should return almost immediately
    }

    // Regression: repeated wait_until calls with same deadline
    // should correctly track remaining time
    TestCase(repeated_wait_until_tracks_remaining_time)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;
        int wakeup_count = 0;
        bool ready = false;

        // Notifier sends a few spurious wakeups, then the real one
        std::thread notifier([&] {
            for (int i = 0; i < 3; ++i) {
                rpp::sleep_ms(10);
                cv.notify_one(); // spurious (ready still false)
            }
            rpp::sleep_ms(10);
            {
                std::lock_guard lock{mtx};
                ready = true;
            }
            cv.notify_one();
        });

        auto lock = std::unique_lock<rpp::mutex>{mtx};
        auto deadline = rpp::TimePoint::monotonic_now() + ms(500);

        // Simulate the semaphore pattern: loop calling wait_until with same deadline
        while (!ready)
        {
            auto result = cv.wait_until(lock, deadline);
            ++wakeup_count;
            if (result == std::cv_status::timeout)
                break;
        }

        notifier.join();
        AssertTrue(ready);
        AssertGreaterOrEqual(wakeup_count, 1); // at least the real wakeup
    }

    // Regression: multiple threads waiting on the same CV
    TestCase(multiple_waiters_all_notified)
    {
        rpp::condition_variable cv;
        rpp::mutex mtx;
        std::atomic_int notified_count{0};
        bool ready = false;

        constexpr int NUM_WAITERS = 4;
        std::vector<std::thread> waiters;
        for (int i = 0; i < NUM_WAITERS; ++i)
        {
            waiters.emplace_back([&] {
                auto lock = std::unique_lock<rpp::mutex>{mtx};
                auto deadline = rpp::TimePoint::monotonic_now() + ms(500);
                while (!ready)
                {
                    if (cv.wait_until(lock, deadline) == std::cv_status::timeout)
                        return; // timed out
                }
                ++notified_count;
            });
        }

        rpp::sleep_ms(30);
        {
            std::lock_guard lock{mtx};
            ready = true;
        }
        cv.notify_all();

        for (auto& t : waiters) t.join();
        AssertEqual(notified_count.load(), NUM_WAITERS);
    }
};
