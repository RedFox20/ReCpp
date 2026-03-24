#include <rpp/concurrent_queue.h>
#include <rpp/future.h>
#include <rpp/timer.h>
#include <rpp/scope_guard.h>
#include <rpp/tests.h>
#include <barrier>
using namespace rpp;

TestImpl(test_concurrent_queue)
{
    static constexpr double MS = 1.0 / 1000.0;

#if APPVEYOR
    static constexpr double sigma_s = 0.02;
    static constexpr double sigma_ms = sigma_s * 2000.0;
#else
    static constexpr double sigma_s = 0.01;
    static constexpr double sigma_ms = sigma_s * 1000.0;
#endif

    static TimePoint Now() { return TimePoint::now(); }
    static constexpr Duration Millis(int millis) { return Duration::from_millis(millis); }

    TestInit(test_concurrent_queue)
    {
    }

    TestCase(push_and_pop)
    {
        concurrent_queue<std::string> queue;
        queue.push("item1");
        queue.push("item2");
        std::string item3 = "item3";
        queue.push(item3); // copy
        AssertThat(queue.size(), 3);
        AssertThat(queue.empty(), false);

        AssertThat(queue.pop(), "item1");
        AssertThat(queue.pop(), "item2");
        AssertThat(queue.pop(), "item3");
        AssertThat(queue.size(), 0);
        AssertThat(queue.empty(), true);

        queue.push_no_notify("item4");
        AssertThat(queue.size(), 1);
        AssertThat(queue.empty(), false);
        AssertThat(queue.pop(), "item4");
    }

    TestCase(clear)
    {
        concurrent_queue<std::string> queue;
        queue.push("item1");
        queue.push("item2");
        queue.push("item3");
        AssertThat(queue.size(), 3);
        AssertThat(queue.empty(), false);
        queue.clear();
        AssertThat(queue.size(), 0);
        AssertThat(queue.empty(), true);
    }

    TestCase(atomic_copy)
    {
        concurrent_queue<std::string> queue;
        queue.push("item1");
        queue.push("item2");
        queue.push("item3");
        std::vector<std::string> items = queue.atomic_copy();
        AssertThat(items.size(), 3);
        AssertThat(items.at(0), "item1");
        AssertThat(items.at(1), "item2");
        AssertThat(items.at(2), "item3");
    }

    TestCase(iterate_locked)
    {
        concurrent_queue<std::string> queue;
        queue.push("item1");
        queue.push("item2");
        queue.push("item3");

        std::vector<std::string> items;
        for (const std::string& item : queue.iterator())
            items.push_back(item);
        AssertThat(items.size(), 3);
        AssertThat(items.at(0), "item1");
        AssertThat(items.at(1), "item2");
        AssertThat(items.at(2), "item3");
    }

    TestCase(iterate_external_lock)
    {
        concurrent_queue<std::string> queue;
        queue.push("item1");
        queue.push("item2");
        queue.push("item3");

        std::vector<std::string> items;
        {
            std::unique_lock lock = queue.spin_lock();
            for (const std::string& item : queue.iterator(lock))
                items.push_back(item);
        }
        AssertThat(items.size(), 3);
        AssertThat(items.at(0), "item1");
        AssertThat(items.at(1), "item2");
        AssertThat(items.at(2), "item3");
    }

    TestCase(can_erase_every_second_item)
    {
        concurrent_queue<int> queue;
        std::vector<int> expected;

        for (int i = 0; i < 1000; ++i)
        {
            queue.push(i);
            if (i % 2 != 0) expected.push_back(i);
        }

        {
            auto iterator = queue.iterator();
            for (auto* it = iterator.begin(); it != iterator.end(); )
            {
                if (*it % 2 == 0)
                    it = iterator.erase(it);
                else
                    ++it;
            }
        }

        std::vector<int> items;
        for (int item : queue.iterator())
            items.push_back(item);

        AssertThat(items.size(), 500);
        AssertEqual(items, expected);
    }

    TestCase(rapid_growth)
    {
        constexpr int MAX_SIZE = 40'000;
        concurrent_queue<std::string> queue;
        for (int i = 0; i < MAX_SIZE; ++i)
            queue.push("item");
        AssertThat(queue.size(), MAX_SIZE);

        int numPopped = 0;
        std::string item;
        while (queue.wait_pop(item, Millis(50)))
            ++numPopped;
        AssertThat(numPopped, MAX_SIZE);
    }

    TestCase(rapid_growth_async)
    {
        constexpr int MAX_SIZE = 40'000;
        concurrent_queue<std::string> queue;
        cfuture<> producer = rpp::async_task([&queue] {
            for (int i = 0; i < MAX_SIZE; ++i)
                queue.push("item");
        });
        scope_guard([&]{ producer.get(); });

        rpp::Timer t;
        int numPopped = 0;
        std::string item;
        while (numPopped < MAX_SIZE && queue.wait_pop(item, Millis(100)))
        {
            ++numPopped;
        }

        double elapsed = t.elapsed();
        double megaitems_per_sec = numPopped / (elapsed * 1'000'000.0);
        print_info("rapid_growth_async elapsed: %.2f ms %.0f Mitem/s\n", elapsed*1000, megaitems_per_sec);
        AssertThat(numPopped, MAX_SIZE);
    }

    // try_pop() is excellent for polling scenarios
    // if you don't want to wait for an item, but just check
    // if any work could be done, otherwise just continue
    TestCase(try_pop)
    {
        concurrent_queue<std::string> queue;
        std::string item;
        AssertThat(queue.try_pop(item), false);
        AssertThat(item, "");

        queue.push("item1");
        AssertThat(queue.try_pop(item), true);
        AssertThat(item, "item1");
        AssertThat(queue.try_pop(item), false);
        AssertThat(item, "item1");

        queue.push("item2");
        queue.push("item3");
        AssertThat(queue.try_pop(item), true);
        AssertThat(item, "item2");
        AssertThat(queue.try_pop(item), true);
        AssertThat(item, "item3");
        AssertThat(queue.try_pop(item), false);
        AssertThat(item, "item3");
    }

    TestCase(atomic_flush)
    {
        concurrent_queue<std::string> queue;
        queue.push("item1");
        queue.push("item2");
        queue.push("item3");

        // count the number of tasks that were atomically processed
        std::atomic_int numProcessed = 0;
        cfuture<> worker = rpp::async_task([&queue, &numProcessed]
        {
            std::string item;
            while (queue.pop_atomic_start(item))
            {
                rpp::sleep_ms(1); // simulate work
                ++numProcessed;
                queue.pop_atomic_end();
            }
        });
        scope_guard([&]{ worker.get(); });

        // flush
        rpp::Timer t;
        while (!queue.empty())
        {
            rpp::sleep_us(100);
            if (t.elapsed_ms() > 100) {
                AssertFailed("queue could not empty itself! size=%zu", queue.size());
                break;
            }
        }
        AssertThat(numProcessed, 3);
    }

    // wait_pop() is best used for producer/consumer scenarios
    // for long-lived service threads that don't have any cancellation mechanism
    TestCase(wait_pop_producer_consumer)
    {
        concurrent_queue<std::string> queue;

        cfuture<> producer = rpp::async_task([&] {
            queue.push("item1");
            queue.push("item2");
            queue.push("item3");
        });

        cfuture<> consumer = rpp::async_task([&] {
            std::string item1 = *queue.wait_pop();
            AssertThat(item1, "item1");
            std::string item2 = *queue.wait_pop();
            AssertThat(item2, "item2");
            std::string item3 = *queue.wait_pop();
            AssertThat(item3, "item3");
        });

        producer.get();
        consumer.get();
    }

    // wait_pop() is best used for producer/consumer scenarios
    // for long-lived service threads that don't have any cancellation mechanism
    // other than basic notify()
    TestCase(wait_pop_2_producer_consumer)
    {
        concurrent_queue<std::string> queue;

        cfuture<> producer = rpp::async_task([&queue] {
            queue.push("item1");
            queue.push("item2");
            queue.push("item3");
            rpp::sleep_ms(5);
            queue.notify_one(); // notify consumer
        });

        cfuture<> consumer = rpp::async_task([&queue] {
            std::string item1, item2, item3; // NOLINT(readability-isolate-declaration)
            AssertTrue(queue.wait_pop(item1));
            AssertThat(item1, "item1");
            AssertTrue(queue.wait_pop(item2));
            AssertThat(item2, "item2");
            AssertTrue(queue.wait_pop(item3));
            AssertThat(item3, "item3");

            // enter infinite wait, but we should be notified by the producer
            std::string item4;
            AssertFalse(queue.wait_pop(item4));
            AssertThat(item4, "");
        });

        producer.get();
        consumer.get();
    }

    struct PopResult
    {
        std::string item;
        double elapsed_ms;
        bool success = false;
        explicit operator bool() const noexcept { return success; }
    };

    static PopResult wait_pop_timed(concurrent_queue<std::string>& queue, Duration timeout)
    {
        PopResult r;
        rpp::Timer t;
        r.success = queue.wait_pop(r.item, timeout);
        r.elapsed_ms = t.elapsed_millis();
        print_info("wait_pop_timed(%dms) elapsed: %.2f ms item: %s\n",
                   (int)timeout.millis(), r.elapsed_ms, r.item.c_str());
        return r;
    }

    #define AssertWaitPopTimed(timeout, expectSuccess, expectItem, minElapsedMs, maxElapsedMs) \
        AssertThat(bool(r = wait_pop_timed(queue, timeout)), expectSuccess); \
        AssertThat(r.item, expectItem); \
        AssertInRange(r.elapsed_ms, minElapsedMs, maxElapsedMs);

    // wait infinitely until an item is pushed
    TestCase(wait_pop_with_timeout)
    {
        concurrent_queue<std::string> queue;

        PopResult r;
        AssertWaitPopTimed(Millis(5), false, /*item*/"", /*elapsed ms:*/ 4.0, 10.0);
        AssertWaitPopTimed(Millis(0), false, /*item*/"", /*elapsed ms:*/ 0.0, 0.2);

        // if someone pushes an item if we have a huge timeout, 
        // we should get it immediately
        queue.push("item1");
        AssertWaitPopTimed(Millis(10'000), true, /*item*/"item1", /*elapsed ms:*/ 0.0, 10.0);
        AssertWaitPopTimed(Millis(15), false, /*item*/"", /*elapsed ms:*/ 14.0, 20.0);
    }

    // introduce a slow producer thread so we can test our timeouts
    TestCase(wait_pop_with_timeout_slow_producer)
    {
        concurrent_queue<std::string> queue;
        rpp::cfuture<> slow_producer = rpp::async_task([&] {
            spin_sleep_for_ms(25);
            queue.push("item1");
            spin_sleep_for_ms(25);
            queue.push("item2");
            spin_sleep_for_ms(25);
            queue.push("item3");
            spin_sleep_for_ms(50);
            queue.push("item4");
        });
        scope_guard([&]{ slow_producer.get(); });

        PopResult r;
        AssertWaitPopTimed(Millis(5), false, /*item*/"", /*elapsed ms:*/ 4.0, 11.0);
        AssertWaitPopTimed(Millis(0), false, /*item*/"", /*elapsed ms:*/ 0.0, 0.5);
        AssertWaitPopTimed(Millis(15), false, /*item*/"", /*elapsed ms:*/ 14.0, 18.0);

        AssertWaitPopTimed(Millis(30), true, /*item*/"item1", /*elapsed ms:*/ 0.0, 30.0); // this should not timeout
        AssertWaitPopTimed(Millis(35), true, /*item*/"item2", /*elapsed ms:*/ 10.0, 30.0);
        AssertWaitPopTimed(Millis(35), true, /*item*/"item3", /*elapsed ms:*/ 10.0, 30.0);

        // now we enter a long wait, but we should be notified by the producer
        AssertWaitPopTimed(Millis(1000), true, /*item*/"item4", /*elapsed ms:*/ 0.0, 60.0);
    }

    static PopResult wait_pop_until(concurrent_queue<std::string>& queue, TimePoint until)
    {
        PopResult r;
        rpp::Timer t;
        r.success = queue.wait_pop_until(r.item, until);
        r.elapsed_ms = t.elapsed_millis();
        print_info("wait_pop_until elapsed: %.2f ms item: %s\n", r.elapsed_ms, r.item.c_str());
        return r;
    }

    #define AssertWaitPopUntil(until, expectSuccess, expectItem, minElapsedMs, maxElapsedMs) \
        AssertThat(bool(r = wait_pop_until(queue, until)), expectSuccess); \
        AssertThat(r.item, expectItem); \
        AssertInRange(r.elapsed_ms, minElapsedMs, maxElapsedMs);

    // wait until an absolute time limit
    TestCase(wait_pop_until)
    {
        concurrent_queue<std::string> queue;

        PopResult r;

        AssertWaitPopUntil(Now()+Millis(5), false, /*item*/"", /*elapsed ms:*/ 2.9, 10.0);
        AssertWaitPopUntil(Now()+Millis(0), false, /*item*/"", /*elapsed ms:*/ 0.0, 0.2);

        // if someone pushes an item if we have a huge timeout,
        // we should get it immediately
        queue.push("item1");
        AssertWaitPopUntil(Now()+Millis(10'000), true, /*item*/"item1", /*elapsed ms:*/ 0.0, 10.0);
        AssertWaitPopUntil(Now()+Millis(15), false, /*item*/"", /*elapsed ms:*/ 12.9, 20.0);

        // if we have an item, but `until` is in the past, it should immediately return false
        queue.push("item2");
        AssertWaitPopUntil(Now()-Millis(15), false, /*item*/"", /*elapsed ms:*/ 0.0, 0.2);
        // and now we can consume it
        AssertWaitPopUntil(Now()+Millis(15), true, /*item*/"item2", /*elapsed ms:*/ 0.0, 0.2);
    }

    // ensure that `wait_pop_until` gives up if timeout is reached
    TestCase(wait_pop_until_stops_on_timeout)
    {
        concurrent_queue<std::string> queue;
        rpp::cfuture<> slow_producer = rpp::async_task([&] {
            spin_sleep_for_ms(25);
            queue.push("item1");
            spin_sleep_for_ms(25);
            queue.push("item2");
            spin_sleep_for_ms(25);
            queue.push("item3");
        });
        scope_guard([&]{ slow_producer.get(); }); // Clang doesn't have jthread yet o_O

        PopResult r;

        // we should only have time to receive item1 and item2
        auto until = Now() + Millis(65);
        AssertWaitPopUntil(until, true, /*item*/"item1", /*elapsed ms:*/ 10.0, 35.0);
        AssertWaitPopUntil(until, true, /*item*/"item2", /*elapsed ms*/ 10.0, 35.0);
        AssertWaitPopUntil(until, false, /*item*/"", /*elapsed ms*/ 10.0, 35.0);
    }

    // in general the pop_with_timeout is not very useful because
    // it requires some external mechanism to notify the queue
    TestCase(wait_pop_with_timeout_and_cancelcondition)
    {
        concurrent_queue<std::string> queue;
        std::atomic_bool finished = false;
        rpp::cfuture<> slow_producer = rpp::async_task([&] {
            spin_sleep_for_ms(25);
            queue.push("item1");
            spin_sleep_for_ms(25);
            queue.push("item2");
            spin_sleep_for_ms(25);
            queue.push("item3");
            spin_sleep_for_ms(25);
            finished = true;
            queue.notify(); // notify any waiting threads
        });
        scope_guard([&]{ slow_producer.get(); });

        auto cancelCondition = [&] { return (bool)finished; };
        std::string item;
        AssertFalse(queue.wait_pop(item, Millis(15), cancelCondition)); // this should timeout
        AssertTrue(queue.wait_pop(item, Millis(15), cancelCondition));
        AssertThat(item, "item1");
        AssertTrue(queue.wait_pop(item, Millis(35), cancelCondition));
        AssertThat(item, "item2");
        AssertTrue(queue.wait_pop(item, Millis(35), cancelCondition));
        AssertThat(item, "item3");
        // now wait until producer exits by setting the cancellation condition
        // this should not take longer than ~35ms
        rpp::Timer t;
        AssertFalse(queue.wait_pop(item, Millis(1000), cancelCondition));
        AssertLess(t.elapsed_millis(), 35);
    }

    // ensure that wait_pop_interval actually checks the cancelCondition
    // with sufficient frequency
    TestCase(wait_pop_interval)
    {
        concurrent_queue<std::string> queue;

        // this barrier helps us to synchronize the producer and consumer
        // in order to minimize the timing variability of different systems
        std::barrier produce(2);

        rpp::cfuture<> slow_producer = rpp::async_task([&]
        {
            produce.arrive_and_wait(); // sync with consumer before producing item1
            spin_sleep_for_ms(25);
            print_info("Producer is pushing: item1\n");
            queue.push("item1");

            produce.arrive_and_wait(); // sync with consumer before producing item2
            spin_sleep_for_ms(25);
            print_info("Producer is pushing: item2\n");
            queue.push("item2");

            produce.arrive_and_wait(); // sync with consumer before producing item3
            spin_sleep_for_ms(25);
            print_info("Producer is pushing: item3\n");
            queue.push("item3");
        });
        scope_guard([&]{ slow_producer.get(); });

        auto wait_pop_interval = [&](std::string& item, auto timeout, auto interval, auto cancel)
        {
            rpp::Timer t;
            bool result = queue.wait_pop_interval(item, timeout, interval, cancel);
            double elapsed_ms = t.elapsed_millis();
            print_info("wait_pop_interval elapsed: %.2f ms item: %s\n", elapsed_ms, item.c_str());
            return result;
        };


        produce.arrive_and_wait(); // produce ITEM1
        {
            // item1 will arrive in 25ms
            // wait with large 10ms intervals, item should definitely arrive and we should check interval at least twice
            std::atomic_int counter0 = 0;
            std::string item1;
            AssertTrue(wait_pop_interval(item1, Millis(40), Millis(10), [&] { return ++counter0 >= 10; }));
            AssertThat(item1, "item1");
            print_info("counter0: %d (expect item to arrive within 20-40ms, 2-4 intervals)\n", int(counter0));
            AssertInRange(int(counter0), 2, 4); // cancellation tolerance is VERY loose here

            // item2 will not arrive during this wait(10ms), time out in just 2 intervals
            std::atomic_int counter1 = 0;
            AssertFalse(wait_pop_interval(item1, Millis(10), Millis(5), [&] { return ++counter1 >= 100/*should never hit this*/; }));
            AssertThat(item1, "item1");
            print_info("counter1: %d (expect early return in 2-3 intervals due to timeout 10ms)\n", int(counter1));
            AssertInRange(int(counter1), 2, 3); // this tests the total timeout(10ms) and not the interval

            // item2 will not arrive during this wait(15ms)
            std::atomic_int counter2 = 0;
            AssertFalse(wait_pop_interval(item1, Millis(15), Millis(2), [&] { return ++counter2 >= 2; }));
            AssertThat(item1, "item1");
            print_info("counter2: %d (expect return due to counter >= 2 before total timeout)\n", int(counter2));
            AssertThat(int(counter2), 2); // this test should timeout due to counter >= 2 before the total timeout is reached
        }

        produce.arrive_and_wait(); // produce ITEM2
        {
            // item2 will arrive in 25ms
            // this ensures we run enough intervals and actually get the item
            std::atomic_int counter3 = 0;
            std::string item2;
            AssertTrue(wait_pop_interval(item2, Millis(30), Millis(5), [&] { return ++counter3 >= 10; }));
            AssertThat(item2, "item2");
            print_info("counter3: %d (item should arrive in less than 7 intervals)\n", int(counter3));
            AssertLess(int(counter3), 7); // we should never have reached all the checks
        }

        produce.arrive_and_wait(); // produce ITEM3
        {
            // item3 will arrive in 25ms
            // wait with extreme short intervals to test that we can rapidly check the cancellation condition
            // this also protects us against bugs where interval 1ms is ignored and we end up waiting full 35ms
            std::atomic_int counter4 = 0;
            std::string item3;
            AssertTrue(wait_pop_interval(item3, Millis(35), Millis(1), [&] { return ++counter4 >= 35; }));
            AssertThat(item3, "item3");
            print_info("counter4: %d (expected return before reaching 35)\n", int(counter4));
            // we should never have reached the limit of 35
            // however there is NO guarantee that the sleep will be 1ms, that's just a minimum hint
            // most likely it will sleep in 1-5ms range, so we lower the min range
            AssertInRange(int(counter4), 5, 34);
        }
    }

    TestCase(wait_pop_cross_thread_perf)
    {
        constexpr int num_iterations = 10;
        constexpr int num_items = 10'000;
        constexpr int total_items = num_iterations * num_items;
        double total_time = 0.0;
        for (int i = 0; i < num_iterations; ++i)
        {
            concurrent_queue<std::string> queue;
            rpp::Timer t;

            rpp::cfuture<> producer = rpp::async_task([&] {
                for (int i = 0; i < num_items; ++i) {
                    queue.push("item");
                    if (i % 1000 == 0) // yield every 1000 items
                        std::this_thread::yield();
                }
            });
            rpp::cfuture<> consumer = rpp::async_task([&] {
                int num_received = 0;
                std::string item;
                // std::deque<std::string> items;
                while (num_received < num_items) {
                    // if (queue.try_pop_all(items)) {
                    //     num_received += items.size();
                    //     rpp::sleep_us(100);
                    // }

                    // if (queue.try_pop(item))
                    //     ++num_received;
                    // item = *queue.wait_pop();
                    // ++num_received;
                    if (queue.wait_pop(item, Millis(5))) {
                        ++num_received;
                    }
                }
            });

            producer.get();
            consumer.get();
            double elapsed = t.elapsed();
            total_time += elapsed;
            print_info("wait_pop consumer elapsed: %.2f ms  queue capacity: %zu\n", elapsed*1000, queue.capacity());
        }

        double avg_time = total_time / num_iterations;
        double items_per_sec = double(total_items) / total_time;
        double Mitems_per_sec = items_per_sec / 1'000'000.0;
        print_info("AVERAGE wait_pop consumer elapsed: %.2f ms  %.1f Mitems/s\n", avg_time, Mitems_per_sec);
    }

    TestCase(wait_pop_interval_cross_thread_perf)
    {
        constexpr int num_iterations = 10;
        constexpr int num_items = 10'000;
        constexpr int total_items = num_iterations * num_items;
        double total_time = 0.0;
        for (int i = 0; i < num_iterations; ++i)
        {
            concurrent_queue<std::string> queue;
            rpp::Timer t;

            rpp::cfuture<> producer = rpp::async_task([&] {
                for (int i = 0; i < num_items; ++i) {
                    queue.push("item");
                    if (i % 1000 == 0) // yield every 1000 items
                        std::this_thread::yield();
                }
            });
            rpp::cfuture<> consumer = rpp::async_task([&] {
                int num_received = 0;
                std::string item;
                while (num_received < num_items) {
                    if (queue.wait_pop_interval(item, Millis(15), Millis(5),
                                                []{ return false; })) {
                        ++num_received;
                    }
                }
            });

            producer.get();
            consumer.get();
            double elapsed = t.elapsed();
            total_time += elapsed;
            print_info("wait_pop_interval consumer elapsed: %.2f ms  queue capacity: %zu\n", elapsed*1000, queue.capacity());
        }

        double avg_time = total_time / num_iterations;
        double items_per_sec = double(total_items) / total_time;
        double Mitems_per_sec = items_per_sec / 1'000'000.0;
        print_info("AVERAGE wait_pop_interval consumer elapsed: %.2f ms  %.1f Mitems/s\n", avg_time, Mitems_per_sec);
    }
};
