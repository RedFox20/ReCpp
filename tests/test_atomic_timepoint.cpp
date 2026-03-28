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
};
