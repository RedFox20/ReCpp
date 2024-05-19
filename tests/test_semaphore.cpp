#include <rpp/semaphore.h>
#include <rpp/timer.h>
#include <rpp/tests.h>
#include <thread>
using namespace rpp;
using millis = std::chrono::milliseconds;

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
        AssertEqual(rpp::semaphore::notified, sem.wait(millis{100}));
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
        AssertEqual(rpp::semaphore::notified, sem.wait(millis{100}));
        AssertEqual(2, sem.count());
        AssertEqual(rpp::semaphore::notified, sem.wait(millis{100}));
        AssertEqual(1, sem.count());
        AssertEqual(rpp::semaphore::notified, sem.wait(millis{100}));
        AssertEqual(0, sem.count());
    }

    TestCase(will_not_block_if_notified_before_wait)
    {
        rpp::semaphore sem;

        for (int i = 0; i < 100; ++i)
        {
            sem.notify();

            rpp::Timer timer;
            AssertEqual(rpp::semaphore::notified, sem.wait(millis{100}));
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
            AssertEqual(rpp::semaphore::notified, sem.wait(millis{100}));
            AssertLess(timer.elapsed_ms(), 20);
            AssertEqual(2, sem.count());
            timer.start();
            AssertEqual(rpp::semaphore::notified, sem.wait(millis{100}));
            AssertLess(timer.elapsed_ms(), 20);
            AssertEqual(1, sem.count());
            timer.start();
            AssertEqual(rpp::semaphore::notified, sem.wait(millis{100}));
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
                rpp::sleep_ms(3); // do some work
                if (sem.wait(millis{100}) == rpp::semaphore::timeout)
                    AssertFailed("semaphore was not notified");
                else if (working)
                    ++num_notified;
            }
        });

        int num_notifies_sent = 25;
        for (int i = 0; i < num_notifies_sent; ++i)
        {
            sem.notify();
            rpp::sleep_ms(5);
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
                if (sem.wait(millis{100}) == rpp::semaphore::timeout)
                    AssertFailed("semaphore was not notified");
                else if (working)
                    ++num_notified;
            }
        });

        int num_notifies_sent = 1000;
        for (int i = 0; i < num_notifies_sent; ++i)
        {
            sem.notify();
            spin_sleep_for_us(100);
        }

        spin_sleep_for_us(15000);
        working = false;
        sem.notify(); // notify finished
        worker.join();

        AssertEqual(num_notified, num_notifies_sent);
    }
};
