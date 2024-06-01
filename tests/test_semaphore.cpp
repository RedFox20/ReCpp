#include <rpp/semaphore.h>
#include <rpp/timer.h>
#include <rpp/tests.h>
#include <thread>
#include <deque>

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
            constexpr auto timeout = millis{5000}; // use a huge timeout to make bugs obvious
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
        // we need to a tiny amount of time for consumer to finish receiving all of the data
        rpp::sleep_ms(15);

        working = false;
        sem.notify(); // notify finished
        consumer.join();

        AssertEqual(consumer_data.size(), producer_data.size());
        AssertEqual(consumer_data, producer_data);
    }
};
