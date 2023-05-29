#include <rpp/timer.h>
#include <rpp/tests.h>
#include <thread>

TestImpl(test_timer)
{
    static constexpr double sigma_s = 0.01;
    static constexpr double sigma_ms = sigma_s * 1000.0;
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
            print_info("iteration %d 50ms spin_sleep timer result: %fs\n", i+1, elapsed);
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
            print_info("iteration %d 50ms spin_sleep timer result: %fms\n", i+1, elapsed);
            AssertInRange(elapsed, 50.0, 50.0 + sigma_ms);
        }
    }

    TestCase(ensure_sleep_ms_accuracy)
    {
        print_info("ticks_per_second: %lld\n", rpp::from_sec_to_time_ticks(1.0));
        for (int i = 0; i < 20; ++i)
        {
            rpp::Timer t;
            rpp::sleep_ms(18);
            double elapsed = t.elapsed_ms();
            print_info("iteration %d 18ms sleep time: %fms\n", i+1, elapsed);
            AssertInRange(elapsed, 18.0, 30.0); // OS sleep can never be accurate enough, so the range must be very loose
        }
    }

    TestCase(ensure_sleep_us_accuracy)
    {
        print_info("ticks_per_second: %lld\n", rpp::from_sec_to_time_ticks(1.0));
        for (int i = 0; i < 20; ++i)
        {
            rpp::Timer t;
            rpp::sleep_us(2500);
            double elapsed = t.elapsed_ms();
            print_info("iteration %d 2500us sleep time: %fms\n", i+1, elapsed);
            AssertInRange(elapsed, 2.5, 4.0); // OS sleep can never be accurate enough, so the range must be very loose
        }
        for (int i = 0; i < 20; ++i)
        {
            rpp::Timer t;
            rpp::sleep_us(500);
            double elapsed = t.elapsed_ms();
            print_info("iteration %d 500us sleep time: %fms\n", i+1, elapsed);
            AssertInRange(elapsed, 0.5, 2.0); // OS sleep can never be accurate enough, so the range must be very loose
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

};
