#include <rpp/atomic_timepoint.h>
#include <rpp/tests.h>
#include <thread>
#include <vector>
using namespace rpp;

TestImpl(test_atomic_timepoint)
{
    TestInit(test_atomic_timepoint)
    {
    }

    ///////////////////// AtomicDuration /////////////////////

    TestCase(duration_default_init)
    {
        AtomicDuration dur;
        AssertThat(dur.load(), Duration::zero());
    }

    TestCase(duration_construct_from_duration)
    {
        AtomicDuration dur = Duration::from_millis(100);
        AssertThat(dur.load(), Duration::from_millis(100));
    }

    TestCase(duration_store_and_load)
    {
        AtomicDuration dur;
        dur.store(Duration::from_seconds(1));
        AssertThat(dur.load(), Duration::from_seconds(1));
    }

    TestCase(duration_assign_operator)
    {
        AtomicDuration dur;
        dur = Duration::from_millis(50);
        AssertThat(dur.load(), Duration::from_millis(50));
    }

    TestCase(duration_exchange)
    {
        AtomicDuration dur = Duration::from_millis(100);
        Duration old = dur.exchange(Duration::from_millis(200));
        AssertThat(old, Duration::from_millis(100));
        AssertThat(dur.load(), Duration::from_millis(200));
    }

    TestCase(duration_compare_exchange_success)
    {
        AtomicDuration dur = Duration::from_millis(100);
        Duration expected = Duration::from_millis(100);
        bool ok = dur.compare_exchange_strong(expected, Duration::from_millis(200));
        AssertThat(ok, true);
        AssertThat(dur.load(), Duration::from_millis(200));
    }

    TestCase(duration_compare_exchange_failure)
    {
        AtomicDuration dur = Duration::from_millis(100);
        Duration expected = Duration::from_millis(999);
        bool ok = dur.compare_exchange_strong(expected, Duration::from_millis(200));
        AssertThat(ok, false);
        // expected is updated to the actual value
        AssertThat(expected, Duration::from_millis(100));
        // value unchanged
        AssertThat(dur.load(), Duration::from_millis(100));
    }

    TestCase(duration_operator_plus_eq)
    {
        AtomicDuration dur = Duration::from_millis(100);
        Duration result = dur += Duration::from_millis(50);
        AssertThat(result, Duration::from_millis(150));
        AssertThat(dur.load(), Duration::from_millis(150));
    }

    TestCase(duration_operator_minus_eq)
    {
        AtomicDuration dur = Duration::from_millis(100);
        Duration result = dur -= Duration::from_millis(30);
        AssertThat(result, Duration::from_millis(70));
        AssertThat(dur.load(), Duration::from_millis(70));
    }

    TestCase(duration_fetch_add)
    {
        AtomicDuration dur = Duration::from_millis(100);
        Duration old = dur.fetch_add(Duration::from_millis(50));
        // fetch_add returns the old value
        AssertThat(old, Duration::from_millis(100));
        AssertThat(dur.load(), Duration::from_millis(150));
    }

    TestCase(duration_fetch_sub)
    {
        AtomicDuration dur = Duration::from_millis(100);
        Duration old = dur.fetch_sub(Duration::from_millis(30));
        // fetch_sub returns the old value
        AssertThat(old, Duration::from_millis(100));
        AssertThat(dur.load(), Duration::from_millis(70));
    }

    TestCase(duration_concurrent_fetch_add)
    {
        AtomicDuration dur;
        constexpr int num_threads = 8;
        constexpr int increments_per_thread = 10000;
        Duration increment = Duration::from_micros(1);

        std::vector<std::thread> threads;
        threads.reserve(num_threads);
        for (int i = 0; i < num_threads; ++i)
        {
            threads.emplace_back([&]()
            {
                for (int j = 0; j < increments_per_thread; ++j)
                    dur += increment;
            });
        }
        for (auto& t : threads) t.join();

        Duration expected = Duration::from_micros(num_threads * increments_per_thread);
        AssertThat(dur.load(), expected);
    }

    ///////////////////// AtomicTimePoint /////////////////////

    TestCase(timepoint_default_init)
    {
        AtomicTimePoint tp;
        AssertThat(tp.load(), TimePoint::zero());
    }

    TestCase(timepoint_construct_from_timepoint)
    {
        TimePoint now = TimePoint::now();
        AtomicTimePoint tp = now;
        AssertThat(tp.load(), now);
        AtomicTimePoint tp2 { now };
        AssertThat(tp2.load(), now);
    }

    TestCase(timepoint_store_and_load)
    {
        AtomicTimePoint tp;
        TimePoint now = TimePoint::now();
        tp.store(now);
        AssertThat(tp.load(), now);
    }

    TestCase(timepoint_exchange)
    {
        TimePoint t1 = TimePoint{int64(1000)};
        TimePoint t2 = TimePoint{int64(2000)};
        AtomicTimePoint tp = t1;
        TimePoint old = tp.exchange(t2);
        AssertThat(old, t1);
        AssertThat(tp.load(), t2);
    }

    TestCase(timepoint_compare_exchange_success)
    {
        TimePoint t1 = TimePoint{int64(1000)};
        TimePoint t2 = TimePoint{int64(2000)};
        AtomicTimePoint tp = t1;
        TimePoint expected = t1;
        bool ok = tp.compare_exchange_strong(expected, t2);
        AssertThat(ok, true);
        AssertThat(tp.load(), t2);
    }

    TestCase(timepoint_compare_exchange_failure)
    {
        TimePoint t1 = TimePoint{int64(1000)};
        TimePoint t2 = TimePoint{int64(2000)};
        AtomicTimePoint tp = t1;
        TimePoint expected = t2; // wrong expectation
        bool ok = tp.compare_exchange_strong(expected, TimePoint{int64(3000)});
        AssertThat(ok, false);
        AssertThat(expected, t1); // updated to actual
        AssertThat(tp.load(), t1); // unchanged
    }

    TestCase(timepoint_operator_plus_eq)
    {
        AtomicTimePoint tp = TimePoint{int64(1000)};
        TimePoint result = tp += Duration::from_nanos(500);
        AssertThat(result, TimePoint{int64(1500)});
        AssertThat(tp.load(), TimePoint{int64(1500)});
    }

    TestCase(timepoint_operator_minus_eq)
    {
        AtomicTimePoint tp = TimePoint{int64(1000)};
        TimePoint result = tp -= Duration::from_nanos(300);
        AssertThat(result, TimePoint{int64(700)});
        AssertThat(tp.load(), TimePoint{int64(700)});
    }

    TestCase(timepoint_concurrent_add)
    {
        AtomicTimePoint tp = TimePoint{int64(0)};
        constexpr int num_threads = 8;
        constexpr int increments_per_thread = 10000;
        Duration increment = Duration::from_micros(1);

        std::vector<std::thread> threads;
        threads.reserve(num_threads);
        for (int i = 0; i < num_threads; ++i)
        {
            threads.emplace_back([&]()
            {
                for (int j = 0; j < increments_per_thread; ++j)
                    tp += increment;
            });
        }
        for (auto& t : threads) t.join();

        TimePoint expected = TimePoint{Duration::from_micros(num_threads * increments_per_thread)};
        AssertThat(tp.load(), expected);
    }

    ///////////////////// AtomicTimeSource /////////////////////

    TestCase(timesource_default_zero)
    {
        AtomicTimeSource ts;
        AssertThat(ts.total_offset(), Duration::zero());
        AssertThat(ts.sync_offset(), Duration::zero());
        AssertThat(ts.warp_offset(), Duration::zero());

        // time_now() should be approximately system time
        TimePoint before = TimePoint::system_now();
        TimePoint ts_now = ts.time_now();
        TimePoint after = TimePoint::system_now();
        AssertGreaterOrEqual(ts_now, before);
        AssertLessOrEqual(ts_now, after);
    }

    TestCase(timesource_warp_forward)
    {
        AtomicTimeSource ts;
        ts.warp_forward(Duration::from_seconds(1));

        AssertThat(ts.warp_offset(), Duration::from_seconds(1));
        AssertThat(ts.sync_offset(), Duration::zero());
        AssertThat(ts.total_offset(), Duration::from_seconds(1));

        // time_now() should be ~1s ahead of system time
        TimePoint sys_now = TimePoint::system_now();
        TimePoint ts_now = ts.time_now();
        Duration diff = ts_now - sys_now;
        AssertGreater(diff.nsec, Duration::from_millis(900).nsec);
        AssertLess(diff.nsec, Duration::from_millis(1100).nsec);
    }

    TestCase(timesource_warp_backward)
    {
        AtomicTimeSource ts;
        ts.warp_forward(Duration::from_seconds(2));
        ts.warp_backward(Duration::from_seconds(1));

        AssertThat(ts.warp_offset(), Duration::from_seconds(1));
        AssertThat(ts.total_offset(), Duration::from_seconds(1));
    }

    TestCase(timesource_set_sync_offset)
    {
        AtomicTimeSource ts;
        ts.set_sync_offset(Duration::from_millis(500));

        AssertThat(ts.sync_offset(), Duration::from_millis(500));
        AssertThat(ts.warp_offset(), Duration::zero());
        AssertThat(ts.total_offset(), Duration::from_millis(500));
    }

    TestCase(timesource_sync_and_warp_combined)
    {
        AtomicTimeSource ts;
        ts.set_sync_offset(Duration::from_seconds(1));
        ts.warp_forward(Duration::from_seconds(2));

        AssertThat(ts.sync_offset(), Duration::from_seconds(1));
        AssertThat(ts.warp_offset(), Duration::from_seconds(2));
        AssertThat(ts.total_offset(), Duration::from_seconds(3));
    }

    TestCase(timesource_set_sync_offset_overwrite)
    {
        AtomicTimeSource ts;
        ts.set_sync_offset(Duration::from_seconds(1));
        ts.set_sync_offset(Duration::from_millis(300));

        // should be 300ms, not 1.3s
        AssertThat(ts.sync_offset(), Duration::from_millis(300));
        AssertThat(ts.total_offset(), Duration::from_millis(300));
    }

    TestCase(timesource_set_sync_offset_overwrite_with_warp)
    {
        AtomicTimeSource ts;
        ts.warp_forward(Duration::from_seconds(2));
        ts.set_sync_offset(Duration::from_seconds(1));
        ts.set_sync_offset(Duration::from_millis(300));

        AssertThat(ts.sync_offset(), Duration::from_millis(300));
        AssertThat(ts.warp_offset(), Duration::from_seconds(2));
        AssertThat(ts.total_offset(), Duration::from_millis(2300));
    }

    TestCase(timesource_set_sync_offset_to_zero)
    {
        AtomicTimeSource ts;
        ts.set_sync_offset(Duration::from_seconds(1));
        ts.set_sync_offset(Duration::zero());

        AssertThat(ts.sync_offset(), Duration::zero());
        AssertThat(ts.total_offset(), Duration::zero());
    }

    TestCase(timesource_warp_negative_total)
    {
        AtomicTimeSource ts;
        ts.warp_backward(Duration::from_seconds(1));

        AssertThat(ts.warp_offset().nsec, -Duration::from_seconds(1).nsec);
        AssertThat(ts.total_offset().nsec, -Duration::from_seconds(1).nsec);

        // time_now() should be ~1s in the past relative to system time
        TimePoint ts_now = ts.time_now();
        TimePoint sys_now = TimePoint::system_now();
        Duration diff = sys_now - ts_now;
        AssertGreater(diff.nsec, Duration::from_millis(900).nsec);
        AssertLess(diff.nsec, Duration::from_millis(1100).nsec);
    }

    TestCase(timesource_set_sync_negative)
    {
        AtomicTimeSource ts;
        ts.set_sync_offset(Duration::from_millis(-500));

        AssertThat(ts.sync_offset().nsec, Duration::from_millis(-500).nsec);
        AssertThat(ts.total_offset().nsec, Duration::from_millis(-500).nsec);
    }

    TestCase(timesource_time_now_monotonic_baseline)
    {
        AtomicTimeSource ts;
        TimePoint t1 = ts.time_now();
        TimePoint t2 = ts.time_now();
        AssertGreaterOrEqual(t2, t1);
    }

    TestCase(timesource_time_now_reflects_warp)
    {
        AtomicTimeSource ts;
        TimePoint before = ts.time_now();
        ts.warp_forward(Duration::from_seconds(1));
        TimePoint after = ts.time_now();

        Duration diff = after - before;
        AssertGreater(diff.nsec, Duration::from_millis(900).nsec);
        AssertLess(diff.nsec, Duration::from_millis(1100).nsec);
    }

    TestCase(timesource_concurrent_warp_forward)
    {
        AtomicTimeSource ts;
        constexpr int num_threads = 3;
        constexpr int ops_per_thread = 1000;
        Duration increment = Duration::from_micros(1);

        std::vector<std::thread> threads;
        threads.reserve(num_threads);
        for (int i = 0; i < num_threads; ++i)
        {
            threads.emplace_back([&]()
            {
                for (int j = 0; j < ops_per_thread; ++j)
                    ts.warp_forward(increment);
            });
        }
        for (auto& t : threads) t.join();

        Duration expected = Duration::from_micros(num_threads * ops_per_thread);
        AssertThat(ts.warp_offset(), expected);
        AssertThat(ts.total_offset(), expected);
        AssertThat(ts.sync_offset(), Duration::zero());
    }

    TestCase(timesource_concurrent_set_sync_offset)
    {
        AtomicTimeSource ts;
        constexpr int num_threads = 8;
        constexpr int ops_per_thread = 10000;

        std::vector<std::thread> threads;
        threads.reserve(num_threads);
        for (int i = 0; i < num_threads; ++i)
        {
            threads.emplace_back([&]()
            {
                for (int j = 0; j < ops_per_thread; ++j)
                    ts.set_sync_offset(Duration::from_micros(j));
            });
        }
        for (auto& t : threads) t.join();

        // no warp applied, so total must equal sync
        AssertThat(ts.total_offset(), ts.sync_offset());
        AssertThat(ts.warp_offset(), Duration::zero());
    }

    TestCase(timesource_concurrent_warp_and_sync)
    {
        AtomicTimeSource ts;
        constexpr int num_threads = 4;
        constexpr int ops_per_thread = 10000;
        Duration warp_increment = Duration::from_micros(1);

        std::vector<std::thread> threads;
        threads.reserve(num_threads * 2);

        // half the threads warp forward
        for (int i = 0; i < num_threads; ++i)
        {
            threads.emplace_back([&]()
            {
                for (int j = 0; j < ops_per_thread; ++j)
                    ts.warp_forward(warp_increment);
            });
        }
        // other half set sync offset
        for (int i = 0; i < num_threads; ++i)
        {
            threads.emplace_back([&]()
            {
                for (int j = 0; j < ops_per_thread; ++j)
                    ts.set_sync_offset(Duration::from_micros(j));
            });
        }
        for (auto& t : threads) t.join();

        // after all threads complete, the invariant must hold
        Duration expected_warp = Duration::from_micros(num_threads * ops_per_thread);
        AssertThat(ts.warp_offset(), expected_warp);
        AssertThat(ts.total_offset(), Duration{ts.sync_offset().nsec + ts.warp_offset().nsec});
    }

    TestCase(timesource_concurrent_read_while_warp)
    {
        AtomicTimeSource ts;
        constexpr int num_writers = 4;
        constexpr int num_readers = 4;
        constexpr int ops_per_thread = 10000;
        Duration increment = Duration::from_micros(1);
        std::atomic<bool> done {false};

        std::vector<std::thread> threads;
        threads.reserve(num_writers + num_readers);

        // writers warp forward
        for (int i = 0; i < num_writers; ++i)
        {
            threads.emplace_back([&]()
            {
                for (int j = 0; j < ops_per_thread; ++j)
                    ts.warp_forward(increment);
            });
        }
        // readers call time_now() continuously
        for (int i = 0; i < num_readers; ++i)
        {
            threads.emplace_back([&]()
            {
                while (!done.load(std::memory_order_relaxed))
                {
                    // just exercise the read path under contention
                    TimePoint t = ts.time_now();
                    (void)t;
                }
            });
        }
        // wait for writers to finish
        for (int i = 0; i < num_writers; ++i)
            threads[i].join();
        done.store(true, std::memory_order_relaxed);
        for (int i = num_writers; i < (int)threads.size(); ++i)
            threads[i].join();

        Duration expected_warp = Duration::from_micros(num_writers * ops_per_thread);
        AssertThat(ts.warp_offset(), expected_warp);
    }
};
