#include <rpp/timer.h>
#include <rpp/tests.h>
#include <thread>
using namespace rpp;
using namespace std::literals;

TestImpl(test_timer)
{
    static constexpr double sigma_s = 0.002;
    static constexpr double sigma_ms = sigma_s * 1000.0;
    TestInit(test_timer)
    {
    }

    TestCase(basic_timer)
    {
        Timer t;
        std::this_thread::sleep_for(100ms);
        double elapsed = t.elapsed();
        println("100ms sleep time:", elapsed, "seconds");
        AssertThat(0.1 <= elapsed && elapsed <= 0.1 + sigma_s, true);
    }

    TestCase(basic_timer_ms)
    {
        Timer t;
        std::this_thread::sleep_for(100ms);
        double elapsed = t.elapsed_ms();
        println("100ms sleep time:", elapsed, "milliseconds");
        AssertThat(100.0 <= elapsed && elapsed <= 100.0 + sigma_ms, true);
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

        std::this_thread::sleep_for(100ms);

        sw.stop();
        AssertThat(sw.started(), true);
        AssertThat(sw.stopped(), true);
        println("100ms stopwatch time:", sw.elapsed(), "seconds");
        AssertThat(0.1 <= sw.elapsed() && sw.elapsed() <= 0.1 + sigma_s, true);
        AssertThat(sw.elapsed(), sw.elapsed()); // ensure time is stable after stop

        sw.resume();
        AssertThat(sw.started(), true);
        AssertThat(sw.stopped(), false);

        sw.reset();
        AssertThat(sw.started(), false);
        AssertThat(sw.stopped(), false);
    }

};
