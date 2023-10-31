#include <rpp/timer.h>
#include <rpp/tests.h>
#include <thread>

TestImpl(test_timer)
{

#if APPVEYOR
    static constexpr double sigma_s = 0.02;
    static constexpr double sigma_ms = sigma_s * 2000.0;
#else
    static constexpr double sigma_s = 0.01;
    static constexpr double sigma_ms = sigma_s * 1000.0;
#endif

    TestInit(test_timer)
    {
    }

    TestCase(basic_timer_sec)
    {
        for (int i = 0; i < 5; ++i)
        {
            rpp::Timer t;
            spin_sleep_for(0.05);
            double elapsed = t.elapsed();
            print_info("timer %d 50ms spin_sleep timer result: %fs\n", i+1, elapsed);
            AssertInRange(elapsed, 0.05, 0.05 + sigma_s);
        }
    }

    TestCase(basic_timer_ms)
    {
        for (int i = 0; i < 5; ++i)
        {
            rpp::Timer t;
            spin_sleep_for(0.05);
            double elapsed_ms = t.elapsed_millis();
            print_info("timer_ms %d 50ms spin_sleep timer result: %fms\n", i+1, elapsed_ms);
            AssertInRange(elapsed_ms, 50.0, 50.0 + sigma_ms);
        }
    }

    TestCase(ensure_sleep_millis_accuracy)
    {
        print_info("ticks_per_second: %lld\n", rpp::from_sec_to_time_ticks(1.0));
        for (int i = 0; i < 20; ++i)
        {
            rpp::Timer t;
            rpp::sleep_ms(18);
            double elapsed_ms = t.elapsed_millis();
            print_info("millis %d 18ms sleep time: %gms\n", i+1, elapsed_ms);
            AssertInRange(elapsed_ms, 18.0, 30.0); // OS sleep can never be accurate enough, so the range must be very loose
        }
    }

    TestCase(ensure_sleep_micros_accuracy)
    {
        print_info("ticks_per_second: %lld\n", rpp::from_sec_to_time_ticks(1.0));
        for (int i = 0; i < 20; ++i)
        {
            rpp::Timer t;
            rpp::sleep_us(2500);
            double elapsed_us = t.elapsed_micros();
            print_info("micros %d 2500us sleep time: %gus\n", i+1, elapsed_us);
            AssertInRange(elapsed_us, 2500, 15'000); // OS sleep can never be accurate enough, so the range must be very loose
        }
        for (int i = 0; i < 20; ++i)
        {
            rpp::Timer t;
            rpp::sleep_us(500);
            double elapsed_us = t.elapsed_micros();
            print_info("micros %d 500us sleep time: %gus\n", i+1, elapsed_us);
            AssertInRange(elapsed_us, 500, 15'000); // OS sleep can never be accurate enough, so the range must be very loose
        }
    }

    TestCase(ensure_sleep_nanos_accuracy)
    {
        print_info("ticks_per_second: %lld\n", rpp::from_sec_to_time_ticks(1.0));
        for (int i = 0; i < 20; ++i)
        {
            rpp::Timer t;
            // going below 100'000ns is not accurate with clock_nanosleep
            rpp::sleep_ns(100'000);
            uint32_t elapsed_ns = t.elapsed_ns(rpp::TimePoint::now());
            print_info("nanos %d 100000ns sleep time: %uns\n", i+1, elapsed_ns);
            AssertInRange(elapsed_ns, 99'999, 5'000'000); // OS sleep can never be accurate enough, so the range must be very loose
        }
    }

    // this tests that all of the Duration::seconds() / TimePoint::elapsed_sec() math is correct
    TestCase(duration_sec_arithmetic)
    {
        rpp::TimePoint t1 = rpp::TimePoint::now();
        const int millis = 57; // wait a few millis
        spin_sleep_for(0.001 * millis);
        rpp::TimePoint t2 = rpp::TimePoint::now();

        double elapsed_sec = t1.elapsed_sec(t2);
        print_info("elapsed_sec: %fs\n", elapsed_sec);

        // now add fake +1sec to t2 and check elapsed_ns again
        rpp::TimePoint t3 = t2 + rpp::Duration::from_seconds(1.0);
        double elapsed_sec2 = t1.elapsed_sec(t3);
        print_info("elapsed_sec2: %fs\n", elapsed_sec2);
        AssertEqual(elapsed_sec2, 1.0 + elapsed_sec);

        // remove fake -1sec from t2 and check again:
        rpp::TimePoint t4 = t2 - rpp::Duration::from_millis(1000);
        double elapsed_sec3 = t1.elapsed_sec(t4);
        print_info("elapsed_sec3: %fs\n", elapsed_sec3);
        AssertEqual(elapsed_sec3, -1.0 + elapsed_sec);
    }

    // this tests that all of the Duration::millis() / TimePoint::elapsed_ms() math is correct
    TestCase(duration_millis_arithmetic)
    {
        rpp::TimePoint t1 = rpp::TimePoint::now();
        const int millis = 15; // wait a few millis
        spin_sleep_for(0.001 * millis);
        rpp::TimePoint t2 = rpp::TimePoint::now();

        int32_t elapsed_ms = t1.elapsed_ms(t2);
        print_info("elapsed_ms: %dms\n", elapsed_ms);

        // now add fake +1sec to t2 and check elapsed_ns again
        rpp::TimePoint t3 = t2 + rpp::Duration::from_millis(1000);
        int32_t elapsed_ms2 = t1.elapsed_ms(t3);
        print_info("elapsed_ms2: %dms\n", elapsed_ms2);
        AssertEqual(elapsed_ms2, 1'000 + elapsed_ms);

        // remove fake -1sec from t2 and check again:
        rpp::TimePoint t4 = t2 - rpp::Duration::from_millis(1000);
        int32_t elapsed_ms3 = t1.elapsed_ms(t4);
        print_info("elapsed_ms3: %dms\n", elapsed_ms3);
        AssertEqual(elapsed_ms3, -1'000 + elapsed_ms);
    }

    // this tests that all of the Duration::micros() / TimePoint::elapsed_us() math is correct
    TestCase(duration_micros_arithmetic)
    {
        rpp::TimePoint t1 = rpp::TimePoint::now();
        const int microseconds = 40; // wait a few micros
        spin_sleep_for(0.000'001 * microseconds);
        rpp::TimePoint t2 = rpp::TimePoint::now();

        int32_t elapsed_us = t1.elapsed_us(t2);
        print_info("elapsed_us: %dus\n", elapsed_us);

        // now add fake +1sec to t2 and check elapsed_ns again
        rpp::TimePoint t3 = t2 + rpp::Duration::from_micros(1'000'000);
        int32_t elapsed_us2 = t1.elapsed_us(t3);
        print_info("elapsed_us2: %dus\n", elapsed_us2);
        AssertEqual(elapsed_us2, 1'000'000 + elapsed_us);

        // remove fake -1sec from t2 and check again:
        rpp::TimePoint t4 = t2 - rpp::Duration::from_micros(1'000'000);
        int32_t elapsed_us3 = t1.elapsed_us(t4);
        print_info("elapsed_us3: %dus\n", elapsed_us3);
        AssertEqual(elapsed_us3, -1'000'000 + elapsed_us);
    }


    // this tests that all of the Duration::nanos() / TimePoint::elapsed_ns() math is correct
    TestCase(duration_nanos_arithmetic)
    {
        rpp::TimePoint t1 = rpp::TimePoint::now();
        const int microseconds = 15; // wait a few micros
        spin_sleep_for(0.000'001 * microseconds);
        rpp::TimePoint t2 = rpp::TimePoint::now();

        int64_t elapsed_ns = t1.elapsed_ns(t2);
        print_info("elapsed_ns: %lldns\n", elapsed_ns);

        // now add fake +1sec to t2 and check elapsed_ns again
        rpp::TimePoint t3 = t2 + rpp::Duration::from_nanos(1'000'000'000);
        int64_t elapsed_ns2 = t1.elapsed_ns(t3);
        print_info("elapsed_ns2: %lldns\n", elapsed_ns2);
        AssertEqual(elapsed_ns2, 1'000'000'000 + elapsed_ns);

        // remove fake -1sec from t2 and check again:
        rpp::TimePoint t4 = t2 - rpp::Duration::from_nanos(1'000'000'000);
        int64_t elapsed_ns3 = t1.elapsed_ns(t4);
        print_info("elapsed_ns3: %lldns\n", elapsed_ns3);
        AssertEqual(elapsed_ns3, -1'000'000'000 + elapsed_ns);
    }

    TestCase(basic_stopwatch)
    {
        rpp::StopWatch sw;
        AssertThat(sw.started(), false);
        AssertThat(sw.stopped(), false);
        AssertThat(sw.elapsed(), 0.0); // default timer must report 0.0

        sw.start();
        AssertThat(sw.started(), true);
        AssertThat(sw.stopped(), false);

        spin_sleep_for(0.1);

        sw.stop();
        AssertThat(sw.started(), true);
        AssertThat(sw.stopped(), true);
        print_info("100ms stopwatch time: %fs\n", sw.elapsed());
        AssertInRange(sw.elapsed(), 0.1, 0.1 + sigma_s);
        AssertThat(sw.elapsed(), sw.elapsed()); // ensure time is stable after stop

        sw.resume();
        AssertThat(sw.started(), true);
        AssertThat(sw.stopped(), false);

        sw.reset();
        AssertThat(sw.started(), false);
        AssertThat(sw.stopped(), false);
    }

    TestCase(scoped_perf_timer)
    {
        {
            auto spt = rpp::ScopedPerfTimer{};
            spin_sleep_for(0.05);
        }
        {
            auto spt = rpp::ScopedPerfTimer { __FUNCTION__ }; // backwards compatibility
            spin_sleep_for(0.05);
        }
        {
            auto spt = rpp::ScopedPerfTimer { "[perf]", __FUNCTION__ };
            spin_sleep_for(0.05);
        }
        {
            auto spt = rpp::ScopedPerfTimer { "[perf]", __FUNCTION__, "detail-item" };
            spin_sleep_for(0.05);
        }
    }
};
