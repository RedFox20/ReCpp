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

    static void sleep_for(double seconds)
    {
        using clock = std::chrono::high_resolution_clock;
        static constexpr std::chrono::duration<double> MIN_SLEEP_DURATION {0};
        clock::time_point start = clock::now();
        while (std::chrono::duration<double>(clock::now() - start).count() < seconds) {
            std::this_thread::sleep_for(MIN_SLEEP_DURATION);
        }
    }

    TestCase(basic_timer)
    {
        Timer t;
        sleep_for(0.1);
        double elapsed = t.elapsed();
        print_info("100ms sleep time: %f seconds\n", elapsed);
        AssertThat(0.1 <= elapsed && elapsed <= 0.1 + sigma_s, true);
    }

    TestCase(basic_timer_ms)
    {
        Timer t;
        sleep_for(0.1);
        double elapsed = t.elapsed_ms();
        print_info("100ms sleep time: %f milliseconds\n", elapsed);
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

        sleep_for(0.1);

        sw.stop();
        AssertThat(sw.started(), true);
        AssertThat(sw.stopped(), true);
        print_info("100ms stopwatch time: %f seconds\n", sw.elapsed());
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
