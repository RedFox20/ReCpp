#include <rpp/concurrent_queue.h>
#include <rpp/future.h>
#include <rpp/timer.h>
#include <rpp/scope_guard.h>
#include <rpp/tests.h>
using namespace rpp;
using namespace std::chrono_literals;

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

    using Clock = concurrent_queue<std::string>::clock;
    using Millis = std::chrono::milliseconds;

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
        AssertThat(queue.safe_size(), 3);
        AssertThat(queue.empty(), false);

        AssertThat(queue.pop(), "item1");
        AssertThat(queue.pop(), "item2");
        AssertThat(queue.pop(), "item3");
        AssertThat(queue.size(), 0);
        AssertThat(queue.safe_size(), 0);
        AssertThat(queue.empty(), true);

        queue.push_no_notify("item4");
        AssertThat(queue.size(), 1);
        AssertThat(queue.safe_size(), 1);
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

    TestCase(rapid_growth)
    {
        constexpr int MAX_SIZE = 40'000;
        concurrent_queue<std::string> queue;
        for (int i = 0; i < MAX_SIZE; ++i)
            queue.push("item");
        AssertThat(queue.size(), MAX_SIZE);

        int numPopped = 0;
        std::string item;
        while (queue.wait_pop(item, 50ms))
            ++numPopped;
        AssertThat(numPopped, MAX_SIZE);
    }

    TestCase(rapid_growth_async)
    {
        constexpr int MAX_SIZE = 40'000;
        concurrent_queue<std::string> queue;
        cfuture<> producer = rpp::async_task([&] {
            for (int i = 0; i < MAX_SIZE; ++i)
                queue.push("item");
        });
        scope_guard([&]{ producer.get(); });

        rpp::Timer t;
        int numPopped = 0;
        std::string item;
        while (numPopped < MAX_SIZE && queue.wait_pop(item, 100ms))
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
        cfuture<> worker = rpp::async_task([&]
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
        while (!queue.empty())
            rpp::sleep_us(100);
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

        cfuture<> producer = rpp::async_task([&] {
            queue.push("item1");
            queue.push("item2");
            queue.push("item3");
            rpp::sleep_ms(5);
            queue.notify_one(); // notify consumer
        });

        cfuture<> consumer = rpp::async_task([&] {
            std::string item1, item2, item3;
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

    PopResult wait_pop_timed(concurrent_queue<std::string>& queue, Clock::duration timeout)
    {
        PopResult r;
        rpp::Timer t;
        r.success = queue.wait_pop(r.item, timeout);
        r.elapsed_ms = t.elapsed_millis();
        print_info("wait_pop_timed elapsed: %.2f ms item: %s\n", r.elapsed_ms, r.item.c_str());
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
        AssertWaitPopTimed(5ms, false, /*item*/"", /*elapsed ms:*/ 4.0, 10.0);
        AssertWaitPopTimed(0ms, false, /*item*/"", /*elapsed ms:*/ 0.0, 0.2);

        // if someone pushes an item if we have a huge timeout, 
        // we should get it immediately
        queue.push("item1");
        AssertWaitPopTimed(10s, true, /*item*/"item1", /*elapsed ms:*/ 0.0, 10.0);
        AssertWaitPopTimed(15ms, false, /*item*/"", /*elapsed ms:*/ 14.0, 17.0);
    }

    // introduce a slow producer thread so we can test our timeouts
    TestCase(wait_pop_with_timeout_slow_producer)
    {
        concurrent_queue<std::string> queue;
        rpp::cfuture<> slow_producer = rpp::async_task([&] {
            spin_sleep_for_ms(50);
            queue.push("item1");
            spin_sleep_for_ms(50);
            queue.push("item2");
            spin_sleep_for_ms(50);
            queue.push("item3");
            spin_sleep_for_ms(100);
            queue.push("item4");
        });
        scope_guard([&]{ slow_producer.get(); });

        PopResult r;
        AssertWaitPopTimed(5ms, false, /*item*/"", /*elapsed ms:*/ 4.0, 10.0);
        AssertWaitPopTimed(0ms, false, /*item*/"", /*elapsed ms:*/ 0.0, 0.5);
        AssertWaitPopTimed(15ms, false, /*item*/"", /*elapsed ms:*/ 14.0, 18.0);

        AssertWaitPopTimed(50ms, true, /*item*/"item1", /*elapsed ms:*/ 15.0, 50.0); // this should not timeout
        AssertWaitPopTimed(75ms, true, /*item*/"item2", /*elapsed ms:*/ 25.0, 55.0);
        AssertWaitPopTimed(75ms, true, /*item*/"item3", /*elapsed ms:*/ 25.0, 55.0);

        // now we enter a long wait, but we should be notified by the producer
        AssertWaitPopTimed(1000ms, true, /*item*/"item4", /*elapsed ms:*/ 0.0, 110.0);
    }

    PopResult wait_pop_until(concurrent_queue<std::string>& queue, Clock::time_point until)
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

        AssertWaitPopUntil(Clock::now()+5ms, false, /*item*/"", /*elapsed ms:*/ 2.9, 10.0);
        AssertWaitPopUntil(Clock::now()+0ms, false, /*item*/"", /*elapsed ms:*/ 0.0, 0.2);

        // if someone pushes an item if we have a huge timeout,
        // we should get it immediately
        queue.push("item1");
        AssertWaitPopUntil(Clock::now()+10s, true, /*item*/"item1", /*elapsed ms:*/ 0.0, 10.0);
        AssertWaitPopUntil(Clock::now()+15ms, false, /*item*/"", /*elapsed ms:*/ 12.9, 20.0);

        // if we have an item, but `until` is in the past, it should immediately return false
        queue.push("item2");
        AssertWaitPopUntil(Clock::now()-15ms, false, /*item*/"", /*elapsed ms:*/ 0.0, 0.2);
        // and now we can consume it
        AssertWaitPopUntil(Clock::now()+15ms, true, /*item*/"item2", /*elapsed ms:*/ 0.0, 0.2);
    }

    // ensure that `wait_pop_until` gives up if timeout is reached
    TestCase(wait_pop_until_stops_on_timeout)
    {
        concurrent_queue<std::string> queue;
        rpp::cfuture<> slow_producer = rpp::async_task([&] {
            spin_sleep_for_ms(50);
            queue.push("item1");
            spin_sleep_for_ms(50);
            queue.push("item2");
            spin_sleep_for_ms(50);
            queue.push("item3");
        });
        scope_guard([&]{ slow_producer.get(); }); // Clang doesn't have jthread yet o_O

        PopResult r;

        // we should only have time to receive item1 and item2
        auto until = Clock::now() + 125ms;
        AssertWaitPopUntil(until, true, /*item*/"item1", /*elapsed ms:*/ 20.0, 60.0);
        AssertWaitPopUntil(until, true, /*item*/"item2", /*elapsed ms*/ 20.0, 60.0);
        AssertWaitPopUntil(until, false, /*item*/"", /*elapsed ms*/ 20.0, 60.0);
    }

    // in general the pop_with_timeout is not very useful because
    // it requires some external mechanism to notify the queue
    TestCase(wait_pop_with_timeout_and_cancelcondition)
    {
        concurrent_queue<std::string> queue;
        std::atomic_bool finished = false;
        rpp::cfuture<> slow_producer = rpp::async_task([&] {
            spin_sleep_for_ms(50);
            queue.push("item1");
            spin_sleep_for_ms(50);
            queue.push("item2");
            spin_sleep_for_ms(50);
            queue.push("item3");
            spin_sleep_for_ms(50);
            finished = true;
            queue.notify(); // notify any waiting threads
        });
        scope_guard([&]{ slow_producer.get(); });

        auto cancelCondition = [&] { return (bool)finished; };
        std::string item;
        AssertFalse(queue.wait_pop(item, 15ms, cancelCondition)); // this should timeout
        AssertTrue(queue.wait_pop(item, 40ms, cancelCondition));
        AssertThat(item, "item1");
        AssertTrue(queue.wait_pop(item, 55ms, cancelCondition));
        AssertThat(item, "item2");
        AssertTrue(queue.wait_pop(item, 55ms, cancelCondition));
        AssertThat(item, "item3");
        // now wait until producer exits by setting the cancellation condition
        // this should not take longer than ~55ms
        rpp::Timer t;
        AssertFalse(queue.wait_pop(item, 1000ms, cancelCondition));
        AssertLess(t.elapsed_millis(), 55);
    }

    // ensure that wait_pop_interval actually checks the cancelCondition
    // with sufficient frequency
    TestCase(wait_pop_interval)
    {
        concurrent_queue<std::string> queue;
        rpp::cfuture<> slow_producer = rpp::async_task([&]
        {
            spin_sleep_for_ms(50);
            queue.push("item1");
            spin_sleep_for_ms(50);
            queue.push("item2");
            spin_sleep_for_ms(50);
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

        std::string item;

        // wait for 100ms with 10ms intervals, but first item will arrive within ~50ms
        std::atomic_int counter0 = 0;
        AssertTrue(wait_pop_interval(item, 100ms, 10ms, [&] { return ++counter0 >= 10; }));
        AssertThat(item, "item1");
        AssertInRange(int(counter0), 5, 9);

        // wait for 20ms with 5ms intervals, it should timeout
        std::atomic_int counter1 = 0;
        AssertFalse(wait_pop_interval(item, 20ms, 5ms, [&] { return ++counter1 >= 10; }));
        AssertInRange(int(counter1), 4, 5);

        // wait another 30ms with 2ms intervals, and it should trigger the cancelcondition
        std::atomic_int counter2 = 0;
        AssertFalse(wait_pop_interval(item, 30ms, 2ms, [&] { return ++counter2 >= 10; }));
        AssertThat(int(counter2), 10); // it should have cancelled exactly at 10 checks

        // wait until we pop the item finally
        std::atomic_int counter3 = 0;
        AssertTrue(wait_pop_interval(item, 100ms, 5ms, [&] { return ++counter3 >= 20; }));
        AssertThat(item, "item2");
        AssertLess(int(counter3), 20); // we should never have reached all the checks

        // now wait with extreme short intervals
        std::atomic_int counter4 = 0;
        AssertTrue(wait_pop_interval(item, 55ms, 1ms, [&] { return ++counter4 >= 55; }));
        AssertThat(item, "item3");
        // we should never have reached the limit of 55
        // however there is NO guarantee that the sleep will be 1ms, that's just a minimum hint
        // most likely it will sleep in 1-5ms range, so we lower the min range
        AssertInRange(int(counter4), 15, 54);
    }

    TestCase(wait_pop_cross_thread_perf)
    {
        constexpr int num_iterations = 10;
        constexpr int num_items = 100'000;
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
                    if (queue.wait_pop(item, Millis{5})) {
                        ++num_received;
                    }
                }
            });

            producer.get();
            consumer.get();
            double elapsed = t.elapsed();
            total_time += elapsed;
            print_info("wait_pop consumer elapsed: %.2f ms  queue capacity: %d\n", elapsed*1000, queue.capacity());
        }

        double avg_time = total_time / num_iterations;
        double items_per_sec = double(total_items) / total_time;
        double Mitems_per_sec = items_per_sec / 1'000'000.0;
        print_info("AVERAGE wait_pop consumer elapsed: %.2f ms  %.1f Mitems/s\n", avg_time, Mitems_per_sec);
    }

    TestCase(wait_pop_interval_cross_thread_perf)
    {
        constexpr int num_iterations = 10;
        constexpr int num_items = 100'000;
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
                    if (queue.wait_pop_interval(item, Millis{15}, Millis{5},
                                                []{ return false; })) {
                        ++num_received;
                    }
                }
            });

            producer.get();
            consumer.get();
            double elapsed = t.elapsed();
            total_time += elapsed;
            print_info("wait_pop_interval consumer elapsed: %.2f ms  queue capacity: %d\n", elapsed*1000, queue.capacity());
        }

        double avg_time = total_time / num_iterations;
        double items_per_sec = double(total_items) / total_time;
        double Mitems_per_sec = items_per_sec / 1'000'000.0;
        print_info("AVERAGE wait_pop_interval consumer elapsed: %.2f ms  %.1f Mitems/s\n", avg_time, Mitems_per_sec);
    }
};
