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
            double elapsed = t.elapsed_ms();
            print_info("timer_ms %d 50ms spin_sleep timer result: %fms\n", i+1, elapsed);
            AssertInRange(elapsed, 50.0, 50.0 + sigma_ms);
        }
    }

    TestCase(ensure_sleep_millis_accuracy)
    {
        print_info("ticks_per_second: %lld\n", rpp::from_sec_to_time_ticks(1.0));
        for (int i = 0; i < 20; ++i)
        {
            rpp::Timer t;
            rpp::sleep_ms(18);
            double elapsed = t.elapsed_ms();
            print_info("millis %d 18ms sleep time: %gms\n", i+1, elapsed);
            AssertInRange(elapsed, 18.0, 30.0); // OS sleep can never be accurate enough, so the range must be very loose
        }
    }

    TestCase(ensure_sleep_micros_accuracy)
    {
        print_info("ticks_per_second: %lld\n", rpp::from_sec_to_time_ticks(1.0));
        for (int i = 0; i < 20; ++i)
        {
            rpp::Timer t;
            rpp::sleep_us(2500);
            double elapsed_us = t.elapsed_ms() * 1000.0;
            print_info("micros %d 2500us sleep time: %gus\n", i+1, elapsed_us);
            AssertInRange(elapsed_us, 2500, 9000); // OS sleep can never be accurate enough, so the range must be very loose
        }
        for (int i = 0; i < 20; ++i)
        {
            rpp::Timer t;
            rpp::sleep_us(500);
            double elapsed_us = t.elapsed_ms() * 1000.0;
            print_info("micros %d 500us sleep time: %gus\n", i+1, elapsed_us);
            AssertInRange(elapsed_us, 500, 4000); // OS sleep can never be accurate enough, so the range must be very loose
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
            double elapsed_ns = t.elapsed_ms() * 1'000'000.0;
            print_info("nanos %d 100000ns sleep time: %gns\n", i+1, elapsed_ns);
            AssertInRange(elapsed_ns, 99999, 400000); // OS sleep can never be accurate enough, so the range must be very loose
        }
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
            auto spt = rpp::ScopedPerfTimer{ std::source_location::current() };
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
