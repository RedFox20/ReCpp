#include <rpp/semaphore.h>
#include <rpp/coroutines.h>
#include <rpp/future.h>
#include <rpp/timer.h>
#include <rpp/tests.h>
#include <thread>
#include <deque>

constexpr rpp::Duration millis(int milliseconds)
{
    return rpp::Duration::from_millis(milliseconds);
}

TestImpl(test_semaphore)
{
    TestInit(test_semaphore)
    {
    }

    TestCase(can_notify_and_wait)
    {
        rpp::semaphore sem;

        // notify
        sem.notify();
        AssertEqual(1, sem.count());

        // wait
        AssertEqual(rpp::semaphore::notified, sem.wait(millis(100)));
        AssertEqual(0, sem.count());
    }

    TestCase(can_notify_multiple_times)
    {
        rpp::semaphore sem;

        // notify
        sem.notify();
        sem.notify();
        sem.notify();
        AssertEqual(3, sem.count());

        // wait
        AssertEqual(rpp::semaphore::notified, sem.wait(millis(100)));
        AssertEqual(2, sem.count());
        AssertEqual(rpp::semaphore::notified, sem.wait(millis(100)));
        AssertEqual(1, sem.count());
        AssertEqual(rpp::semaphore::notified, sem.wait(millis(100)));
        AssertEqual(0, sem.count());
    }

    TestCase(will_not_block_if_notified_before_wait)
    {
        rpp::semaphore sem;

        for (int i = 0; i < 100; ++i)
        {
            sem.notify();

            rpp::Timer timer;
            AssertEqual(rpp::semaphore::notified, sem.wait(millis(100)));
            AssertLess(timer.elapsed_ms(), 20);
            AssertEqual(0, sem.count());
        }
    }

    TestCase(will_not_block_if_notified_multiple_times_before_wait)
    {
        rpp::semaphore sem;

        for (int i = 0; i < 20; ++i)
        {
            sem.notify();
            sem.notify();
            sem.notify();

            rpp::Timer timer;
            AssertEqual(rpp::semaphore::notified, sem.wait(millis(100)));
            AssertLess(timer.elapsed_ms(), 20);
            AssertEqual(2, sem.count());
            timer.start();
            AssertEqual(rpp::semaphore::notified, sem.wait(millis(100)));
            AssertLess(timer.elapsed_ms(), 20);
            AssertEqual(1, sem.count());
            timer.start();
            AssertEqual(rpp::semaphore::notified, sem.wait(millis(100)));
            AssertLess(timer.elapsed_ms(), 20);
            AssertEqual(0, sem.count());
        }
    }

    TestCase(notify_once_will_not_increment_semaphore_count_more_than_once)
    {
        rpp::semaphore sem;

        for (int i = 0; i < 10; ++i)
        {
            sem.notify_once();
            AssertEqual(1, sem.count());
        }
    }

    TestCase(can_notify_worker_thread)
    {
        rpp::semaphore sem;
        std::atomic_bool working = true;
        std::atomic_int num_notified = 0;

        std::thread worker([&]
        {
            while (working)
            {
                rpp::sleep_ms(1); // do some work
                if (sem.wait(millis(50)) == rpp::semaphore::timeout)
                    AssertFailed("semaphore was not notified");
                else if (working)
                    ++num_notified;
            }
        });

        int num_notifies_sent = 10;
        for (int i = 0; i < num_notifies_sent; ++i)
        {
            sem.notify();
            rpp::sleep_ms(2);
        }

        working = false;
        sem.notify(); // notify finished
        worker.join();

        AssertEqual(num_notified, num_notifies_sent);
    }

    // this is a much more intensive test
    TestCase(can_notify_worker_thread_sub_millisecond)
    {
        rpp::semaphore sem;
        std::atomic_bool working = true;
        std::atomic_int num_notified = 0;

        std::thread worker([&]
        {
            while (working)
            {
                spin_sleep_for_us(100); // do some work
                if (sem.wait(millis(50)) == rpp::semaphore::timeout)
                    AssertFailed("semaphore was not notified");
                else if (working)
                    ++num_notified;
            }
        });

        int num_notifies_sent = 500;
        for (int i = 0; i < num_notifies_sent; ++i)
        {
            sem.notify();
            spin_sleep_for_us(100);
        }

        spin_sleep_for_us(5000);
        working = false;
        sem.notify(); // notify finished
        worker.join();

        AssertEqual(num_notified, num_notifies_sent);
    }

    // this is a much more intensive test
    TestCase(can_transfer_data_between_two_threads)
    {
        std::vector<std::string> producer_data; // for later comparison
        std::deque<std::string> producer_queue;
        std::vector<std::string> consumer_data;
        rpp::mutex producer_mutex;
        rpp::semaphore sem;

        std::atomic_bool working = true;
        std::thread producer([&] {
            const int max_data = 10'000;
            for (int i = 0; i < max_data; ++i) {
                { std::lock_guard lock { producer_mutex };
                    producer_data.push_back("data_" + std::to_string(i));
                    producer_queue.push_back(producer_data.back());
                }
                sem.notify();
            }
        });

        std::thread consumer([&] {
            constexpr auto timeout = millis(5000); // use a huge timeout to make bugs obvious
            while (working) {
                if (sem.wait(timeout) == rpp::semaphore::notified) {
                    if (!working) break; // stopped
                    std::lock_guard lock { producer_mutex };
                    consumer_data.emplace_back(std::move(producer_queue.front()));
                    producer_queue.pop_front();
                } else AssertFailed("semaphore was not notified");
            }
        });

        producer.join();

        // wait for consumer to finish receiving all of the data
        for (int i = 0; i < 40; ++i) { // with a max wait limit
            { std::lock_guard lock { producer_mutex };
              if (producer_queue.empty()) break;
            }
            rpp::sleep_ms(1);
        }

        working = false;
        sem.notify(); // notify finished
        consumer.join();

        AssertEqual(consumer_data.size(), producer_data.size());
        AssertEqual(consumer_data, producer_data);
    }

    // Regression: wait with timeout must correctly time out
    TestCase(wait_timeout_returns_after_expected_duration)
    {
        rpp::semaphore sem;
        rpp::Timer t;
        AssertEqual(sem.wait(millis(20)), rpp::semaphore::timeout);
        AssertGreater(t.elapsed_millis(), 10.0);
        AssertLess(t.elapsed_millis(), 100.0);
    }

    // Regression: notify just before timeout should still return notified
    TestCase(notify_near_timeout_returns_notified)
    {
        rpp::semaphore sem;
        std::thread notifier([&] { rpp::sleep_ms(10); sem.notify(); });

        rpp::Timer t;
        AssertEqual(sem.wait(millis(100)), rpp::semaphore::notified);
        AssertLess(t.elapsed_millis(), 50.0);
        notifier.join();
    }

    // Regression: notify_once then wait should consume the stale value,
    // then a second wait should block until notified
    TestCase(stale_notify_once_consumed_then_blocks)
    {
        rpp::semaphore sem;
        sem.notify_once();

        rpp::Timer t;
        AssertEqual(sem.wait(millis(50)), rpp::semaphore::notified);
        AssertLess(t.elapsed_millis(), 10.0);
        AssertEqual(0, sem.count());

        t.start();
        AssertEqual(sem.wait(millis(20)), rpp::semaphore::timeout);
        AssertGreater(t.elapsed_millis(), 10.0);
    }

    // Regression: concurrent notify during semaphore wait must not be lost
    TestCase(concurrent_notify_during_wait_not_lost)
    {
        for (int i = 0; i < 100; ++i)
        {
            rpp::semaphore sem;
            std::thread notifier([&] { rpp::sleep_us(500); sem.notify(); });
            AssertEqual(sem.wait(millis(100)), rpp::semaphore::notified);
            notifier.join();
        }
    }

    // Regression: multiple rapid notify_once + wait cycles
    TestCase(rapid_notify_once_wait_cycles)
    {
        rpp::semaphore sem;
        std::atomic_bool running{true};
        std::atomic_int received{0};

        std::thread producer([&] {
            while (running) { sem.notify_once(); rpp::sleep_us(100); }
        });

        for (int i = 0; i < 50; ++i)
            if (sem.wait(millis(50)) == rpp::semaphore::notified)
                ++received;

        running = false;
        producer.join();
        AssertEqual(received.load(), 50);
    }

    TestCase(semaphore_flag_wait_timeout)
    {
        rpp::semaphore_flag flag;
        rpp::Timer t;
        AssertEqual(flag.wait(millis(20)), rpp::semaphore::timeout);
        AssertGreater(t.elapsed_millis(), 10.0);
        AssertLess(t.elapsed_millis(), 100.0);
    }

    TestCase(semaphore_flag_wait_notified)
    {
        rpp::semaphore_flag flag;
        std::thread notifier([&] { rpp::sleep_ms(5); flag.notify(); });

        AssertEqual(flag.wait(millis(200)), rpp::semaphore::notified);
        notifier.join();
    }

    TestCase(wait_zero_timeout_returns_immediately)
    {
        rpp::semaphore sem;
        rpp::Timer t;
        AssertEqual(sem.wait(millis(0)), rpp::semaphore::timeout);
        AssertEqual(sem.wait_no_unset(millis(0)), rpp::semaphore::timeout);
        AssertLess(t.elapsed_millis(), 10.0);
    }

    TestCase(wait_very_short_timeout_no_deadlock)
    {
        rpp::semaphore sem;
        rpp::Timer t;
        AssertEqual(sem.wait(millis(1)), rpp::semaphore::timeout);
        AssertLess(t.elapsed_millis(), 50.0);
    }

    // Regression: multiple rapid wait timeouts must not accumulate errors
    TestCase(rapid_wait_timeouts_no_accumulation)
    {
        rpp::semaphore sem;
        rpp::Timer t;
        for (int i = 0; i < 10; ++i)
            AssertEqual(sem.wait(millis(2)), rpp::semaphore::timeout);
        AssertGreater(t.elapsed_millis(), 10.0);
        AssertLess(t.elapsed_millis(), 200.0);
    }

    TestCase(semaphore_flag_concurrent_notify)
    {
        for (int i = 0; i < 20; ++i)
        {
            rpp::semaphore_flag f;
            std::thread notifier([&f] { rpp::sleep_us(500); f.notify(); });
            AssertEqual(f.wait(millis(100)), rpp::semaphore::notified);
            notifier.join();
        }
    }

#if RPP_HAS_COROUTINES
    // NOLINTBEGIN(cppcoreguidelines-avoid-capturing-lambda-coroutines)
    // NOTE: TestCaseCoro cannot be used here because the standalone awaiters
    // resume on a background thread, which conflicts with the default test
    // runner's synchronous pump loop.

    // ─── co_await semaphore: signal received ────────────────────
    TestCase(co_await_semaphore_notified)
    {
        rpp::semaphore sem;
        auto coro = [&]() -> rpp::cfuture<void>
        {
            auto result = co_await sem.await(millis(500));
            AssertEqual(result, rpp::semaphore::notified);
        };

        std::thread producer([&]() { rpp::sleep_ms(20); sem.notify(); });
        auto future = coro();
        future.get();
        producer.join();
    }

    // ─── co_await semaphore: timeout ────────────────────────────
    TestCase(co_await_semaphore_timeout)
    {
        rpp::semaphore sem;
        auto coro = [&]() -> rpp::cfuture<void>
        {
            auto result = co_await sem.await(millis(20));
            AssertEqual(result, rpp::semaphore::timeout);
        };
        coro().get();
    }

    // ─── co_await semaphore: already signaled (fast path) ───────
    TestCase(co_await_semaphore_already_signaled)
    {
        rpp::semaphore sem;
        sem.notify(); // pre-signal

        auto coro = [&]() -> rpp::cfuture<void>
        {
            auto result = co_await sem.await(millis(100));
            AssertEqual(result, rpp::semaphore::notified);
        };
        coro().get();
        // try_wait in await_ready should have consumed the signal
        AssertEqual(sem.count(), 0);
    }

    // NOLINTEND(cppcoreguidelines-avoid-capturing-lambda-coroutines)
#endif // RPP_HAS_COROUTINES
};
