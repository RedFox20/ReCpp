#include <rpp/timer.h>
#include <rpp/tests.h>
#include <thread>

namespace rpp {
    string_buffer& operator<<(string_buffer& sb, const Duration& d) noexcept
    {
        return sb << d.nsec << "ns";
    }
}

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
        for (int i = 0; i < 20; ++i)
        {
            rpp::Timer t;
            rpp::sleep_ms(18);
            double elapsed_ms = t.elapsed_millis();
            print_info("millis %d 18ms sleep time: %gms\n", i+1, elapsed_ms);
            AssertInRange(elapsed_ms, 17.0, 30.0); // OS sleep can never be accurate enough, so the range must be very loose
        }
    }

    TestCase(ensure_sleep_micros_accuracy)
    {
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
        for (int i = 0; i < 20; ++i)
        {
            rpp::Timer t;
            // going below 100'000ns is not accurate with clock_nanosleep
            rpp::sleep_ns(100'000);
            int64_t elapsed_ns = t.elapsed_ns(rpp::TimePoint::now());
            print_info("nanos %d 100000ns sleep time: %lldns\n", i+1, elapsed_ns);
            AssertInRange(elapsed_ns, 99'999, 5'000'000); // OS sleep can never be accurate enough, so the range must be very loose
        }
    }

    TestCase(duration_conversion)
    {
        rpp::Duration d0 = rpp::Duration::from_nanos(1'000'000'000);
        AssertEqual(d0.sec(), 1.0);
        AssertEqual(d0.millis(), 1'000);
        AssertEqual(d0.micros(), 1'000'000);
        AssertEqual(d0.nanos(), 1'000'000'000);
        AssertEqual(d0, rpp::Duration::from_millis(1000));

        rpp::Duration d1 = rpp::Duration::from_seconds(1.0);
        AssertEqual(d1.sec(), 1.0);
        AssertEqual(d1.millis(), 1'000);
        AssertEqual(d1.micros(), 1'000'000);
        AssertEqual(d1.nanos(), 1'000'000'000);
        AssertEqual(d1, rpp::Duration::from_millis(1000));

        rpp::Duration d2 = rpp::Duration::from_millis(500);
        AssertEqual(d2.sec(), 0.5);
        AssertEqual(d2.millis(), 500);
        AssertEqual(d2.micros(), 500'000);
        AssertEqual(d2.nanos(), 500'000'000);
        AssertEqual(d2, rpp::Duration::from_millis(500));

        rpp::Duration d3 = rpp::Duration::from_micros(100'000);
        AssertEqual(d3.sec(), 0.1);
        AssertEqual(d3.millis(), 100);
        AssertEqual(d3.micros(), 100'000);
        AssertEqual(d3.nanos(), 100'000'000);
        AssertEqual(d3, rpp::Duration::from_millis(100));

        rpp::Duration d4 = rpp::Duration::from_nanos(2'000'000);
        AssertEqual(d4.sec(), 0.002);
        AssertEqual(d4.millis(), 2);
        AssertEqual(d4.micros(), 2'000);
        AssertEqual(d4.nanos(), 2'000'000);
        AssertEqual(d4, rpp::Duration::from_millis(2));
    }

    TestCase(duration_conversion_negative)
    {
        rpp::Duration d0 = rpp::Duration::from_nanos(-1'000'000'000);
        AssertEqual(d0.sec(), -1.0);
        AssertEqual(d0.millis(), -1'000);
        AssertEqual(d0.micros(), -1'000'000);
        AssertEqual(d0.nanos(), -1'000'000'000);
        AssertEqual(d0, rpp::Duration::from_millis(-1000));

        rpp::Duration d1 = rpp::Duration::from_seconds(-1.0);
        AssertEqual(d1.sec(), -1.0);
        AssertEqual(d1.millis(), -1'000);
        AssertEqual(d1.micros(), -1'000'000);
        AssertEqual(d1.nanos(), -1'000'000'000);
        AssertEqual(d1, rpp::Duration::from_millis(-1000));

        rpp::Duration d2 = rpp::Duration::from_millis(-500);
        AssertEqual(d2.sec(), -0.5);
        AssertEqual(d2.millis(), -500);
        AssertEqual(d2.micros(), -500'000);
        AssertEqual(d2.nanos(), -500'000'000);
        AssertEqual(d2, rpp::Duration::from_millis(-500));

        rpp::Duration d3 = rpp::Duration::from_micros(-100'000);
        AssertEqual(d3.sec(), -0.1);
        AssertEqual(d3.millis(), -100);
        AssertEqual(d3.micros(), -100'000);
        AssertEqual(d3.nanos(), -100'000'000);
        AssertEqual(d3, rpp::Duration::from_millis(-100));

        rpp::Duration d4 = rpp::Duration::from_nanos(-2'000'000);
        AssertEqual(d4.sec(), -0.002);
        AssertEqual(d4.millis(), -2);
        AssertEqual(d4.micros(), -2'000);
        AssertEqual(d4.nanos(), -2'000'000);
        AssertEqual(d4, rpp::Duration::from_millis(-2));
    }

    TestCase(duration_arithmetic)
    {
        rpp::Duration d1 = rpp::Duration::from_millis(500);
        rpp::Duration d2 = rpp::Duration::from_micros(-250'000);
        rpp::Duration d3 = d1 + d2; // 250ms
        AssertEqual(d3.sec(), 0.25);
        AssertEqual(d3.millis(), 250);
        AssertEqual(d3.micros(), 250'000);
        AssertEqual(d3.nanos(), 250'000'000);
        AssertEqual(d3, rpp::Duration::from_millis(250));

        rpp::Duration d4 = d2 - d1; // -750ms
        AssertEqual(d4.sec(), -0.75);
        AssertEqual(d4.millis(), -750);
        AssertEqual(d4.micros(), -750'000);
        AssertEqual(d4.nanos(), -750'000'000);
        AssertEqual(d4, rpp::Duration::from_millis(-750));

        rpp::Duration d5 = d1 + d1; // 1s
        AssertEqual(d5.sec(), 1.0);
        AssertEqual(d5.millis(), 1'000);
        AssertEqual(d5.micros(), 1'000'000);
        AssertEqual(d5.nanos(), 1'000'000'000);
        AssertEqual(d5, rpp::Duration::from_millis(1000));
    }

    // this tests that all of the Duration::sec() / TimePoint::elapsed_sec() math is correct
    TestCase(duration_sec_arithmetic)
    {
        rpp::TimePoint t1 = rpp::TimePoint::now();
        const int millis = 57; // wait a few millis
        spin_sleep_for(0.001 * millis);
        rpp::TimePoint t2 = rpp::TimePoint::now();

        double elapsed_sec = t1.elapsed_sec(t2);
        print_info("elapsed_sec: %fs\n", elapsed_sec);
        rpp::Duration one_sec = rpp::Duration::from_seconds(1.0);
        print_info("duration::from_seconds(1.0): %fs\n", one_sec.sec());
        AssertEqual(one_sec.sec(), 1.0);

        // now add fake +1sec to t2 and check elapsed_ns again
        rpp::TimePoint t3 = t2 + one_sec;
        double elapsed_sec2 = t1.elapsed_sec(t3);
        print_info("elapsed_sec2: %fs\n", elapsed_sec2);
        AssertEqual(elapsed_sec2, 1.0 + elapsed_sec);

        // remove fake -1sec from t2 and check again:
        rpp::TimePoint t4 = t2 - one_sec;
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

        rpp::Duration elapsed1 = t1.elapsed(t2);
        print_info("elapsed: %lldns\n", elapsed1.nanos());
        rpp::Duration one_sec = rpp::Duration::from_millis(1000);
        print_info("duration::from_millis(1000): %dms\n", one_sec.millis());
        AssertEqual(one_sec.millis(), 1'000);

        // now add fake +1sec to t2 and check elapsed_ns again
        rpp::TimePoint t3 = t2 + one_sec;
        rpp::Duration elapsed2 = t1.elapsed(t3);
        print_info("elapsed2: %lldns\n", elapsed2.nanos());
        AssertEqual(elapsed2, one_sec + elapsed1);

        // remove fake -1sec from t2 and check again:
        rpp::TimePoint t4 = t2 - one_sec;
        rpp::Duration elapsed3 = t1.elapsed(t4);
        print_info("elapsed3: %lldns\n", elapsed3.nanos());
        AssertEqual(elapsed3, -one_sec + elapsed1);
    }

    // this tests that all of the Duration::micros() / TimePoint::elapsed_us() math is correct
    TestCase(duration_micros_arithmetic)
    {
        rpp::TimePoint t1 = rpp::TimePoint::now();
        const int microseconds = 40; // wait a few micros
        spin_sleep_for(0.000'001 * microseconds);
        rpp::TimePoint t2 = rpp::TimePoint::now();

        rpp::Duration elapsed1 = t1.elapsed(t2);
        print_info("elapsed: %lldns\n", elapsed1.nanos());
        rpp::Duration one_sec = rpp::Duration::from_micros(1'000'000);
        print_info("duration::from_micros(1'000'000): %dus\n", one_sec.micros());
        AssertEqual(one_sec.micros(), 1'000'000);

        // now add fake +1sec to t2 and check elapsed_ns again
        rpp::TimePoint t3 = t2 + one_sec;
        rpp::Duration elapsed2 = t1.elapsed(t3);
        print_info("elapsed2: %lldns\n", elapsed2.nanos());
        AssertEqual(elapsed2, one_sec + elapsed1);

        // remove fake -1sec from t2 and check again:
        rpp::TimePoint t4 = t2 - one_sec;
        rpp::Duration elapsed3 = t1.elapsed(t4);
        print_info("elapsed3: %lldns\n", elapsed3.nanos());
        AssertEqual(elapsed3, -one_sec + elapsed1);
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
        rpp::Duration one_sec = rpp::Duration::from_nanos(1'000'000'000);
        print_info("duration::from_nanos(1'000'000'000): %lldus\n", one_sec.nanos());
        AssertEqual(one_sec.nanos(), 1'000'000'000LL);

        // now add fake +1sec to t2 and check elapsed_ns again
        rpp::TimePoint t3 = t2 + one_sec;
        int64_t elapsed_ns2 = t1.elapsed_ns(t3);
        print_info("elapsed_ns2: %lldns\n", elapsed_ns2);
        AssertEqual(elapsed_ns2, 1'000'000'000 + elapsed_ns);

        // remove fake -1sec from t2 and check again:
        rpp::TimePoint t4 = t2 - one_sec;
        int64_t elapsed_ns3 = t1.elapsed_ns(t4);
        print_info("elapsed_ns3: %lldns\n", elapsed_ns3);
        AssertEqual(elapsed_ns3, -1'000'000'000 + elapsed_ns);
    }

    TestCase(duration_overflow)
    {
        // take 2147483647 (int max) seconds and convert to nanos
        rpp::Duration d0 = rpp::Duration::from_seconds(2147483647);
        print_info("d0: %lldns\n", d0.nanos());
        print_info("d0.seconds: %lld\n", d0.seconds());
        print_info("d0.millis: %lld\n", d0.millis());
        print_info("d0.micros: %lld\n", d0.micros());
        print_info("d0.nanos: %lld\n", d0.nanos());
        print_info("d0.days: %lld\n", d0.days());
        print_info("d0.hours: %lld\n", d0.hours());
        print_info("d0.minutes: %lld\n", d0.minutes());

        rpp::Duration d1 = rpp::Duration{1708732302913202308LL};
        print_info("d1.seconds: %lld\n", d1.seconds());
        print_info("d1.millis: %lld\n", d1.millis());
        print_info("d1.micros: %lld\n", d1.micros());
        print_info("d1.nanos: %lld\n", d1.nanos());
        print_info("d1.days: %lld\n", d1.days());
        print_info("d1.hours: %lld\n", d1.hours());
        print_info("d1.minutes: %lld\n", d1.minutes());
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

    TestCase(duration_to_string)
    {
        auto d1 = rpp::Duration(18, 56, 10, 523'000'000);
        auto d2 = rpp::Duration(19, 57, 11, 523'001'000);
        AssertEqual(rpp::Duration{}.to_string(6), "00:00:00.000000");
        AssertEqual(d1.to_string(6), "18:56:10.523000");
        AssertEqual(d2.to_string(6), "19:57:11.523001");
        AssertEqual((d2 - d1).to_string(6),  "01:01:01.000001");
        AssertEqual((d1 - d2).to_string(6), "-01:01:01.000001");
    }

    TestCase(timepoint_to_string)
    {
        auto t1 = rpp::TimePoint(2021, 1, 1, 12, 34, 56, 789'010'000LL);
        auto t2 = rpp::TimePoint(2021, 1, 1, 12, 34, 56, 789'021'000LL);
        AssertEqual(t1.to_string(6), "2021-01-01 12:34:56.789010");
        AssertEqual(t2.to_string(6), "2021-01-01 12:34:56.789021");
        AssertEqual((t2 - t1).to_string(6),  "00:00:00.000011");
        AssertEqual((t1 - t2).to_string(6), "-00:00:00.000011");

        auto t3 = rpp::TimePoint(2024, 3, 4, 9, 8, 7, 123'456'789LL);
        AssertEqual(t3.to_string(3), "2024-03-04 09:08:07.123");
        AssertEqual(t3.to_string(6), "2024-03-04 09:08:07.123456");
        AssertEqual(t3.to_string(9), "2024-03-04 09:08:07.123456789");
    }
};
