#include <rpp/timer.h>
#include <rpp/tests.h>
#include <thread>
#include <chrono>
using namespace rpp;
using namespace std::literals;

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
            Timer t;
            spin_sleep_for(0.05);
            double elapsed = t.elapsed();
            print_info("iteration %d 50ms spin_sleep timer result: %f seconds\n", i+1, elapsed);
            AssertGreaterOrEqual(elapsed, 0.05);
            AssertLessOrEqual(elapsed, 0.05 + sigma_s);
        }
    }

    TestCase(basic_timer_ms)
    {
        for (int i = 0; i < 5; ++i)
        {
            Timer t;
            spin_sleep_for(0.05);
            double elapsed = t.elapsed_ms();
            print_info("iteration %d 50ms spin_sleep timer result: %f milliseconds\n", i+1, elapsed);
            AssertGreaterOrEqual(elapsed, 50.0);
            AssertLessOrEqual(elapsed, 50.0 + sigma_ms);
        }
    }

    TestCase(ensure_sleep_ms_accuracy)
    {
        print_info("ticks_per_second: %lld\n", from_sec_to_time_ticks(1.0));
        print_info("hw concurrency: %d\n", std::thread::hardware_concurrency());
        for (int i = 0; i < 20; ++i)
        {
            Timer t;
            rpp::sleep_ms(18);
            double elapsed = t.elapsed_ms();
            print_info("iteration %d 18ms sleep time: %f milliseconds\n", i+1, elapsed);
            AssertGreaterOrEqual(elapsed, 18.0);
            AssertLessOrEqual(elapsed, 18.2);
        }
    }

    TestCase(ensure_sleep_us_accuracy)
    {
        print_info("ticks_per_second: %lld\n", from_sec_to_time_ticks(1.0));
        for (int i = 0; i < 20; ++i)
        {
            Timer t;
            rpp::sleep_us(1500);
            double elapsed = t.elapsed_ms();
            print_info("iteration %d 1500us sleep time: %f milliseconds\n", i+1, elapsed);
            AssertGreaterOrEqual(elapsed, 1.5);
            AssertLessOrEqual(elapsed, 1.7);
        }
    }

    TestCase(basic_stopwatch)
    {
        StopWatch sw;
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
        print_info("100ms stopwatch time: %f seconds\n", sw.elapsed());
        AssertGreaterOrEqual(sw.elapsed(), 0.1);
        AssertLessOrEqual(sw.elapsed(), 0.1 + sigma_s);
        AssertThat(sw.elapsed(), sw.elapsed()); // ensure time is stable after stop

        sw.resume();
        AssertThat(sw.started(), true);
        AssertThat(sw.stopped(), false);

        sw.reset();
        AssertThat(sw.started(), false);
        AssertThat(sw.stopped(), false);
    }

};
