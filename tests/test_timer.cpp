#include <rpp/timer.h>
#include <rpp/debugging.h>
#include <rpp/proc_utils.h>
#include <rpp/tests.h>
#include <ctime> // gmtime_r
#include <thread>
#include <string>
#include <vector>
#if __linux__ // Android also defines __linux__.
    #include <rpp/file_io.h> // rpp::file
    #include <rpp/threads.h> // rpp::yield
    #include <atomic> // std::atomic_bool
    #include <csignal> // sigaction
    #include <cstdio> // sscanf
    #include <ctime> // CLOCK_MONOTONIC, TIMER_ABSTIME
    #include <string> // std::to_string
    #include <pthread.h> // pthread_kill
    #include <unistd.h> // gettid
#endif

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

    // a spin which starts and ends inside one scheduler tick reads 0, so a CPU clock
    // case retries. A read which still fails after every spin means a broken clock.
    static constexpr int MAX_CPU_CLOCK_SPINS = 3;

    TestInit(test_timer)
    {
    }

    TestCase(basic_timer_sec)
    {
        double elapsed_s = best_of_3([]
        {
            rpp::Timer t;
            spin_sleep_for_ms(10);
            return t.elapsed();
        });
        print_info("timer 10ms spin_sleep best of 3: %fs\n", elapsed_s);
        AssertInRange(elapsed_s, 0.010, 0.010 + sigma_s);
    }

    TestCase(basic_timer_ms)
    {
        double elapsed_ms = best_of_3([]
        {
            rpp::Timer t;
            spin_sleep_for_ms(10);
            return t.elapsed_millis();
        });
        print_info("timer_ms 10ms spin_sleep best of 3: %fms\n", elapsed_ms);
        AssertInRange(elapsed_ms, 10.0, 10.0 + sigma_ms);
    }

    // best_of_3 can regress and the three timing callers still pass on an unloaded host.
    // This pins the call count and the minimum on a fixed sequence.
    TestCase(best_of_3_takes_three_samples_and_returns_the_smallest)
    {
        int calls = 0;
        const double spike[] = { 40.0, 10.0, 25.0 }; // the smallest sits in the middle
        AssertEqual(best_of_3([&]{ return spike[calls++]; }), 10.0);
        AssertEqual(calls, 3); // a regression to one call would return the first sample

        calls = 0;
        const double rising[] = { 5.0, 6.0, 7.0 }; // the smallest sits first
        AssertEqual(best_of_3([&]{ return rising[calls++]; }), 5.0);
        AssertEqual(calls, 3);
    }

    TestCase(ensure_sleep_millis_accuracy)
    {
        if (has_coarse_timer()) return; // the coarse tick overshoots these bounds
        for (int i = 0; i < 3; ++i)
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
        if (has_coarse_timer()) return; // the coarse tick overshoots these bounds
        for (int i = 0; i < 3; ++i)
        {
            rpp::Timer t;
            rpp::sleep_us(2500);
            double elapsed_us = t.elapsed_micros();
            print_info("micros %d 2500us sleep time: %gus\n", i+1, elapsed_us);
            AssertInRange(elapsed_us, 2500, 20'000); // OS sleep can never be accurate enough, so the range must be very loose
        }
        for (int i = 0; i < 3; ++i)
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
        if (has_coarse_timer()) return; // the coarse tick overshoots these bounds
        for (int i = 0; i < 5; ++i)
        {
            rpp::Timer t;
            // going below 100'000ns is not accurate with clock_nanosleep
            rpp::sleep_ns(100'000);
            rpp::int64 elapsed_ns = t.elapsed_ns(rpp::TimePoint::monotonic_now());
            print_info("nanos %d 100000ns sleep time: %lldns\n", i+1, elapsed_ns);
            AssertInRange(elapsed_ns, 99'999, 7'100'200); // OS sleep can never be accurate enough, so the range must be very loose
        }
    }

#if __linux__
    // A thread which blocks in a system call shows its number and arguments in this file, else the word "running".
    static bool read_blocked_syscall(const std::string& path, unsigned long long& clock, unsigned long long& flags)
    {
        char text[256];
        rpp::file f { path, rpp::file::READONLY };
        const int len = f ? f.read(text, sizeof(text) - 1) : 0;
        text[len > 0 ? len : 0] = '\0';
        long long nr = -1;
        return sscanf(text, "%lld %llx %llx", &nr, &clock, &flags) == 3 && nr >= 0;
    }

    // A wall clock step moves no CLOCK_MONOTONIC deadline, and a signal must not end the sleep early.
    TestCase(sleep_waits_for_a_monotonic_deadline_through_a_signal)
    {
        static std::atomic_bool handled;
        handled = false;
        struct sigaction on_signal {};
        struct sigaction old_signal {};
        on_signal.sa_handler = [](int) { handled = true; };
        sigaction(SIGUSR1, &on_signal, &old_signal);

        const rpp::Duration sleep = rpp::millis(10);
        std::atomic<pid_t> sleeper_tid { 0 };
        std::atomic_bool done { false };
        rpp::Duration slept;
        std::thread sleeper { [&] {
            sleeper_tid = gettid();
            const rpp::TimePoint start = rpp::TimePoint::monotonic_now();
            rpp::sleep_ns(sleep.nsec);
            slept = rpp::TimePoint::monotonic_now() - start;
            done = true;
        }};
        while (sleeper_tid == 0) rpp::yield();
        const std::string path = "/proc/self/task/" + std::to_string(sleeper_tid) + "/syscall";
        unsigned long long clock = ~0ull;
        unsigned long long flags = ~0ull;
        bool blocked = false;
        while (!blocked && !done) blocked = read_blocked_syscall(path, clock, flags); // The end of the sleep ends this wait.
        pthread_kill(sleeper.native_handle(), SIGUSR1);
        sleeper.join();
        sigaction(SIGUSR1, &old_signal, nullptr);

        AssertTrue(blocked);
        AssertEqual(clock, static_cast<unsigned long long>(CLOCK_MONOTONIC));
        AssertEqual(flags, static_cast<unsigned long long>(TIMER_ABSTIME));
        AssertTrue(handled);
        AssertGreaterOrEqual(slept.nsec, sleep.nsec);
    }
#endif

    // every unit view of d names the same span. sec() compares exactly, because both sides divide exact integers
    static void check_units(rpp::Duration d, rpp::int64 ms, RPP_SOURCE_LOC)
    {
        AssertThatLoc(loc, d.sec(), ms / 1000.0);
        AssertThatLoc(loc, d.millis(), ms);
        AssertThatLoc(loc, d.micros(), ms * 1'000);
        AssertThatLoc(loc, d.nanos(), ms * 1'000'000);
        AssertThatLoc(loc, d, rpp::millis(ms));
    }

    TestCase(duration_conversion)
    {
        check_units(rpp::Duration::from_nanos(1'000'000'000), 1'000);
        check_units(rpp::Duration::from_seconds(1.0), 1'000);
        check_units(rpp::Duration::from_millis(500), 500);
        check_units(rpp::Duration::from_micros(100'000), 100);
        check_units(rpp::Duration::from_nanos(2'000'000), 2);
    }

    TestCase(duration_conversion_negative)
    {
        check_units(rpp::Duration::from_nanos(-1'000'000'000), -1'000);
        check_units(rpp::Duration::from_seconds(-1.0), -1'000);
        check_units(rpp::Duration::from_millis(-500), -500);
        check_units(rpp::Duration::from_micros(-100'000), -100);
        check_units(rpp::Duration::from_nanos(-2'000'000), -2);
    }

    TestCase(duration_arithmetic)
    {
        rpp::Duration d1 = rpp::Duration::from_millis(500);
        rpp::Duration d2 = rpp::Duration::from_micros(-250'000);
        check_units(d1 + d2, 250);
        check_units(d2 - d1, -750);
        check_units(d1 + d1, 1'000);
    }

    // moving t2 by one second must move each elapsed reading by exactly one second
    // `one` is one second in the unit elapsed() returns, so a scale error in elapsed() shows
    template<class T, class Elapsed>
    static void check_one_sec_shift(rpp::Duration one_sec, T one, Elapsed elapsed, RPP_SOURCE_LOC)
    {
        rpp::TimePoint t1 = rpp::TimePoint::monotonic_now();
        spin_sleep_for_us(15);
        rpp::TimePoint t2 = rpp::TimePoint::monotonic_now();
        AssertThatLoc(loc, (t2 + one_sec) - t2, one_sec);
        AssertThatLoc(loc, t2 - (t2 - one_sec), one_sec);
        auto e = elapsed(t1, t2);
        AssertThatLoc(loc, elapsed(t1, t2 + one_sec), one + e);
        AssertThatLoc(loc, elapsed(t1, t2 - one_sec), e - one);
    }

    TestCase(duration_sec_arithmetic)
    {
        AssertEqual(rpp::Duration::from_seconds(1.0).sec(), 1.0);
        check_one_sec_shift(rpp::Duration::from_seconds(1.0), 1.0, [](auto t1, auto t2) { return t1.elapsed_sec(t2); });
    }

    TestCase(duration_millis_arithmetic)
    {
        check_one_sec_shift(rpp::Duration::from_millis(1000), rpp::seconds(1), [](auto t1, auto t2) { return t1.elapsed(t2); });
    }

    TestCase(duration_micros_arithmetic)
    {
        check_one_sec_shift(rpp::Duration::from_micros(1'000'000), rpp::seconds(1),
                            [](auto t1, auto t2) { return t1.elapsed(t2); });
    }

    TestCase(duration_nanos_arithmetic)
    {
        check_one_sec_shift(rpp::Duration::from_nanos(1'000'000'000), 1'000'000'000LL,
                            [](auto t1, auto t2) { return t1.elapsed_ns(t2); });
    }

    TestCase(duration_overflow)
    {
        // int max seconds still fits in int64 nanos
        rpp::Duration d0 = rpp::seconds(2147483647);
        AssertThat(d0.seconds(), 2147483647LL);
        AssertThat(d0.millis(), 2147483647'000LL);
        AssertThat(d0.micros(), 2147483647'000'000LL);
        AssertThat(d0.nanos(), 2147483647'000'000'000LL);
        AssertThat(d0.days(), 24855LL);
        AssertThat(d0.hours(), 596523LL);
        AssertThat(d0.minutes(), 35791394LL);

        rpp::Duration d1 = rpp::Duration{1708732302913202308LL};
        AssertThat(d1.seconds(), 1708732302LL);
        AssertThat(d1.millis(), 1708732302913LL);
        AssertThat(d1.micros(), 1708732302913202LL);
        AssertThat(d1.days(), 19776LL);
        AssertThat(d1.hours(), 474647LL);
        AssertThat(d1.minutes(), 28478871LL);
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

        spin_sleep_for_ms(20);

        sw.stop();
        AssertThat(sw.started(), true);
        AssertThat(sw.stopped(), true);
        print_info("20ms stopwatch time: %fs\n", sw.elapsed());
        AssertInRange(sw.elapsed_millis(), 20.0, 20.0 + sigma_ms);
        AssertThat(sw.elapsed(), sw.elapsed()); // ensure time is stable after stop

        sw.resume();
        AssertThat(sw.started(), true);
        AssertThat(sw.stopped(), false);

        sw.clear();
        AssertThat(sw.started(), false);
        AssertThat(sw.stopped(), false);

        sw.start();
        AssertThat(sw.started(), true);
        AssertThat(sw.stopped(), false);
        auto prev_begin = sw.begin;

        spin_sleep_for_ms(1);
        sw.restart();
        AssertThat(sw.started(), true);
        AssertThat(sw.stopped(), false);
        AssertNotEqual(sw.begin, prev_begin);
    }

#if !RPP_ANDROID // Android sends the perf line to logcat, not to the log handlers
    static void capture_log_line(void* context, LogSeverity, const char* message, int len) noexcept
    {
        static_cast<std::vector<std::string>*>(context)->emplace_back(message, len);
    }

    TestCase(scoped_perf_timer)
    {
        std::vector<std::string> lines;
        rpp::add_log_handler(&lines, &capture_log_line);
        { rpp::ScopedPerfTimer spt{}; }
        { rpp::ScopedPerfTimer spt{ "[perf]", __FUNCTION__, "detail-item" }; }
        { rpp::ScopedPerfTimer spt{ "[perf]", __FUNCTION__, 1'000'000ul }; } // under the threshold, so no line
        rpp::remove_log_handler(&lines, &capture_log_line);

        AssertThat(lines.size(), 2u);
        // MSVC names the function with its class, so the match starts at the function name
        AssertThat(rpp::strview{lines[0]}.contains("test_scoped_perf_timer elapsed: "), true);
        AssertThat(rpp::strview{lines[1]}.contains("test_scoped_perf_timer detail-item elapsed: "), true);
        AssertThat(rpp::strview{lines[1]}.contains("[perf] "), true);
    }
#endif

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

    TestCase(duration_to_stopwatch_string)
    {
        auto d1 = rpp::Duration(19, 57, 11, 523'425'526);
        auto d2 = -rpp::Duration(0, 10000, 1, 523'001'000);
        AssertEqual(rpp::Duration{}.to_stopwatch_string(3), "[0s 000ms]");
        AssertEqual(d1.to_stopwatch_string(3), "[1197m 11s 523ms]");
        AssertEqual(d1.to_stopwatch_string(6), "[1197m 11s 523ms 425us]");
        AssertEqual(d1.to_stopwatch_string(9), "[1197m 11s 523ms 425us 526ns]");
        AssertEqual(d2.to_stopwatch_string(3), "[-10000m 1s 523ms]");
        AssertEqual(d2.to_stopwatch_string(6), "[-10000m 1s 523ms 001us]");
        AssertEqual(d2.to_stopwatch_string(9), "[-10000m 1s 523ms 001us 000ns]");
        auto minDuration = rpp::Duration::min();
        AssertEqual(minDuration.to_stopwatch_string(9), "[-153722867m 16s 854ms 775us 807ns]");
        AssertLessOrEqual(minDuration.to_stopwatch_string(9).length(), 35u);
        auto maxDuration = rpp::Duration::max();
        AssertEqual(maxDuration.to_stopwatch_string(9), "[153722867m 16s 854ms 775us 807ns]");
        AssertLessOrEqual(maxDuration.to_stopwatch_string(9).length(), 35u);
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
        AssertEqual(t3.to_string(0), "2024-03-04 09:08:07");
        AssertEqual(t3.to_string(1), "2024-03-04 09:08:07.1");
        AssertEqual(t3.to_string(2), "2024-03-04 09:08:07.12");
        AssertEqual(t3.to_string(3), "2024-03-04 09:08:07.123");
        AssertEqual(t3.to_string(4), "2024-03-04 09:08:07.1234");
        AssertEqual(t3.to_string(5), "2024-03-04 09:08:07.12345");
        AssertEqual(t3.to_string(6), "2024-03-04 09:08:07.123456");
        AssertEqual(t3.to_string(7), "2024-03-04 09:08:07.1234567");
        AssertEqual(t3.to_string(8), "2024-03-04 09:08:07.12345678");
        AssertEqual(t3.to_string(9), "2024-03-04 09:08:07.123456789");
    }

    TestCase(timepoint_to_calendar)
    {
        rpp::CalendarTime cal = rpp::TimePoint(2024, 3, 4, 9, 8, 7, 123'456'789LL).to_calendar();
        AssertTrue(cal.is_valid());
        AssertEqual(cal.year, 2024);
        AssertEqual(cal.month, 3);
        AssertEqual(cal.day, 4);
        AssertEqual(cal.hour, 9);
        AssertEqual(cal.minute, 8);
        AssertEqual(cal.second, 7);
        AssertEqual(cal.nanos, 123'456'789LL);

        // a pre-epoch timepoint is negative, so a truncated division would report the next second
        rpp::CalendarTime moon = rpp::TimePoint(1969, 7, 20, 20, 17, 40, 500'000'000LL).to_calendar();
        AssertTrue(moon.is_valid());
        AssertEqual(moon.year, 1969);
        AssertEqual(moon.month, 7);
        AssertEqual(moon.day, 20);
        AssertEqual(moon.hour, 20);
        AssertEqual(moon.minute, 17);
        AssertEqual(moon.second, 40);
        AssertEqual(moon.nanos, 500'000'000LL);
    }

    TestCase(timepoint_to_calendar_past_a_narrow_time_t)
    {
        // a narrow time_t cannot hold this year, and the calendar still splits it
        constexpr rpp::int64 YEAR_2200_SECONDS = 7'272'378'123LL;
        rpp::TimePoint far { rpp::seconds(YEAR_2200_SECONDS) };
        AssertThat(far.to_calendar(), (rpp::CalendarTime{ 2200, 6, 15, 1, 2, 3, 0 }));
    }

#if !_MSC_VER
    // the integer calendar must agree with gmtime_r() across leap days, centuries and the epoch
    TestCase(timepoint_to_calendar_matches_gmtime)
    {
        for (rpp::int64 secs = -9'000'000'000LL; secs < 9'000'000'000LL; secs += 9'876'541LL)
        {
            const time_t t = time_t(secs);
            std::tm tm {};
            if (rpp::int64(t) != secs || !gmtime_r(&t, &tm))
                continue; // a narrow time_t has no reference for this date
            rpp::CalendarTime expected { tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, 0 };
            rpp::CalendarTime cal = rpp::TimePoint{ rpp::seconds(secs) }.to_calendar();
            if (cal != expected)
                AssertFailed("%lld s gives %d-%d-%d %d:%d:%d, gmtime_r gives %d-%d-%d %d:%d:%d", secs,
                             cal.year, cal.month, cal.day, cal.hour, cal.minute, cal.second, expected.year,
                             expected.month, expected.day, expected.hour, expected.minute, expected.second);
        }
    }
#endif

    TestCase(timepoint_to_calendar_matches_to_string)
    {
        rpp::TimePoint now = rpp::TimePoint::now();
        rpp::CalendarTime cal = now.to_calendar();
        AssertTrue(bool(cal));
        rpp::TimePoint rebuilt { cal.year, cal.month, cal.day, cal.hour, cal.minute, cal.second, cal.nanos };
        AssertEqual(rebuilt.to_string(9), now.to_string(9));
    }

    TestCase(timepoint_handles_timezone_offset)
    {
        auto t1 = rpp::TimePoint(2021, 1, 1, 12, 34, 56, 789'010'000LL);
        auto local = t1.utc_to_local();
        print_info("time:  %s\n", t1.to_string().c_str());
        print_info("local: %s\n", local.to_string().c_str());

        rpp::int64 tz_offset = rpp::TimePoint::timezone_offset_seconds();
        print_info("tz_offset: %lld sec\n", tz_offset);
        auto expected = t1 + rpp::Duration::from_seconds(tz_offset);
        print_info("expected: %s\n", expected.to_string().c_str());

        AssertEqual(local.to_string(), expected.to_string());
    }

    TestCase(timer_with_clock_type)
    {
        rpp::Timer t { rpp::ClockType::Monotonic };
        AssertThat(t.clock, rpp::ClockType::Monotonic);
        AssertThat(t.is_started(), true);
        spin_sleep_for_us(10'000, /*full_spin*/true);
        double elapsed_ms = t.elapsed_millis();
        print_info("Timer(Monotonic) 10ms elapsed: %.3fms\n", elapsed_ms);
        AssertInRange(elapsed_ms, 9.0, 15.0);

        // test next() resets with correct clock
        double next_ms = t.next_millis();
        print_info("Timer(Monotonic) next: %.3fms\n", next_ms);
        AssertGreater(next_ms, 0.0);
    }

    TestCase(timer_with_clock_type_nostart)
    {
        rpp::Timer t { rpp::ClockType::Monotonic, rpp::Timer::NoStart };
        AssertThat(t.clock, rpp::ClockType::Monotonic);
        AssertThat(t.is_started(), false);
        t.start();
        AssertThat(t.is_started(), true);
        spin_sleep_for_us(10'000, /*full_spin*/true);
        double elapsed_ms = t.elapsed_millis();
        print_info("Timer(Monotonic, NoStart) 10ms elapsed: %.3fms\n", elapsed_ms);
        AssertInRange(elapsed_ms, 9.0, 15.0);
    }

    TestCase(clock_type_realtime)
    {
        // Realtime should return a value close to default now()
        rpp::TimePoint t1 = rpp::TimePoint::now();
        rpp::TimePoint t2 = rpp::TimePoint::now(rpp::ClockType::Realtime);
        rpp::int64 diff_us = (t2 - t1).abs().micros();
        print_info("Realtime vs now() diff: %lldus\n", diff_us);
        AssertLess(diff_us, 1'000); // should be within 1000us
    }

    TestCase(clock_type_realtime_coarse)
    {
        rpp::TimePoint t1 = rpp::TimePoint::now(rpp::ClockType::RealtimeCoarse);
        AssertThat(t1.is_valid(), true);
        rpp::TimePoint t2 = rpp::TimePoint::now(rpp::ClockType::Realtime);
        rpp::int64 diff_us = (t2 - t1).abs().micros();
        print_info("RealtimeCoarse vs Realtime diff: %lldus\n", diff_us);
        AssertLess(diff_us, 100'000);
    }

    /// @returns how far the clock moved across a full spin of `us`
    static rpp::Duration spin_on(rpp::ClockType clock, int us, RPP_SOURCE_LOC)
    {
        rpp::TimePoint t1 = rpp::TimePoint::now(clock);
        AssertThatLoc(loc, t1.is_valid(), true);
        spin_sleep_for_us(us, /*full_spin*/true);
        return rpp::TimePoint::now(clock) - t1;
    }

    TestCase(clock_type_monotonic)
    {
        AssertInRange(spin_on(rpp::ClockType::Monotonic, 10'000).micros(), 9'000, 18'000);
    }

    TestCase(clock_type_monotonic_raw)
    {
        AssertInRange(spin_on(rpp::ClockType::MonotonicRaw, 10'000).micros(), 9'000, 18'000);
    }

    // Windows GetTickCount64 ticks every ~15.6ms, and a virtualized runner can tick slower still.
    // A 20ms spin once measured 0ms, so the coarse clocks spin across several ticks.
    TestCase(clock_type_monotonic_coarse)
    {
        AssertInRange(spin_on(rpp::ClockType::MonotonicCoarse, 80'000).millis(), 10, 200);
    }

    // no case reads Boottime before this one, so both threads take the first read of its offset
    TestCase(two_threads_read_a_new_clock_offset_without_a_race)
    {
        rpp::TimePoint a;
        rpp::TimePoint b;
        std::thread first{[&] { a = rpp::TimePoint::now(rpp::ClockType::Boottime); }};
        std::thread second{[&] { b = rpp::TimePoint::now(rpp::ClockType::Boottime); }};
        first.join();
        second.join();
        AssertThat(a.is_valid() && b.is_valid(), true);
    }

    TestCase(clock_type_boottime)
    {
        // Boottime shares GetTickCount64 on Windows, so it needs the same tick headroom
        AssertInRange(spin_on(rpp::ClockType::Boottime, 80'000).millis(), 10, 200);
    }

    // GetProcessTimes and GetThreadTimes report whole scheduler ticks, so one spin can start and end
    // inside a single tick and read 0. Spin again until a tick lands.
    static rpp::int64 cpu_spin_us(rpp::ClockType clock, RPP_SOURCE_LOC)
    {
        rpp::TimePoint t1 = rpp::TimePoint::now(clock);
        AssertThatLoc(loc, t1.is_valid(), true);
        rpp::int64 elapsed_us = 0;
        for (int spins = 0; spins < MAX_CPU_CLOCK_SPINS && elapsed_us == 0; ++spins)
        {
            spin_sleep_for_us(20'000, /*full_spin*/true);
            elapsed_us = (rpp::TimePoint::now(clock) - t1).micros();
        }
        return elapsed_us;
    }

    TestCase(clock_type_process_cpu)
    {
        AssertGreater(cpu_spin_us(rpp::ClockType::ProcessCPU), 0);
    }

    TestCase(clock_type_thread_cpu)
    {
        AssertGreater(cpu_spin_us(rpp::ClockType::ThreadCPU), 0);
    }

    TestCase(clock_type_tai)
    {
        // TAI may fail on systems without TAI configured, but should not crash
        rpp::TimePoint t1 = rpp::TimePoint::now(rpp::ClockType::TAI);
        print_info("TAI value: %lldns\n", t1.duration.nanos());
        // TAI falls back to Realtime on non-Linux, so just verify it returns something
        AssertThat(t1.is_valid(), true);
    }

    TestCase(monotonic_epoch_synchronized)
    {
        // Monotonic clocks are synced to the realtime epoch ON FIRST USE (so they print as wall-clock
        // time). The offset is captured once and intentionally NOT refreshed - that immunity to wall-clock
        // steps is the whole point of measuring time monotonically. So the synced value can drift from
        // realtime by however much the OS stepped the wall clock since first use (NTP / suspend / VM-host
        // sync); on CI/qemu that is seconds. We therefore assert (1) the epoch was applied at all and
        // (2) tight RATE synchronization over a short window - not an absolute sub-second offset.
        rpp::TimePoint real = rpp::TimePoint::now();
        rpp::TimePoint mono = rpp::TimePoint::now(rpp::ClockType::Monotonic);
        rpp::TimePoint boot = rpp::TimePoint::now(rpp::ClockType::Boottime);
        print_info("Monotonic vs Realtime diff: %lldms\n", (mono - real).abs().millis());
        print_info("Boottime  vs Realtime diff: %lldms\n", (boot - real).abs().millis());
        // epoch applied: a non-synced clock reads raw time-since-boot, off from the wall clock by the
        // host uptime (minutes to days); a synced clock is within the wall-clock step budget (seconds).
        AssertLess((mono - real).abs().seconds(), 60);
        AssertLess((boot - real).abs().seconds(), 60);

        // rate sync: synced-monotonic and realtime must advance together over a short window (wall-clock
        // steps are negligible across a few ms), which is the precise synchronization timestamps rely on.
        rpp::TimePoint r1 = rpp::TimePoint::now();
        rpp::TimePoint m1 = rpp::TimePoint::now(rpp::ClockType::Monotonic);
        spin_sleep_for_us(20'000, /*full_spin*/true);
        rpp::TimePoint r2 = rpp::TimePoint::now();
        rpp::TimePoint m2 = rpp::TimePoint::now(rpp::ClockType::Monotonic);
        rpp::int64 skew_us = ((m2 - m1) - (r2 - r1)).abs().micros();
        print_info("Monotonic vs Realtime rate skew over 20ms: %lldus\n", skew_us);
        AssertLess(skew_us, 2'000);
    }

#if !__EMSCRIPTEN__ // getrusage() is a stub which returns constant times
    TestCase(proc_cpu_times)
    {
        rpp::cpu_usage_info t1 = rpp::proc_total_cpu_usage();
        int spin_us = 50'000;
        spin_sleep_for_us(spin_us, /*full_spin*/true); // spin wait for accurate time counts
        rpp::cpu_usage_info t2 = rpp::proc_total_cpu_usage();

        print_info("#1 cpu all:%.1fms usr:%.1fms sys:%.1fms\n", t1.cpu_time_ms(), t1.user_time_ms(), t1.kernel_time_ms());
        print_info("#2 cpu all:%.1fms usr:%.1fms sys:%.1fms\n", t2.cpu_time_ms(), t2.user_time_ms(), t2.kernel_time_ms());
        rpp::int64 kernel_delta = t2.kernel_time_us - t1.kernel_time_us;
        rpp::int64 user_delta = t2.user_time_us - t1.user_time_us;
        rpp::int64 cpu_delta = t2.cpu_time_us - t1.cpu_time_us;
        print_info("# diff all:%.1fms usr:%.1fms sys:%.1fms\n",
                   double(cpu_delta)/1000.0, double(user_delta)/1000.0, double(kernel_delta)/1000.0);

        // kernel_time_us can be 0 on Linux VMs where spin_sleep uses RDTSC (pure userspace)
        // also, since we use spin_sleep_for_us() with full_spin=true, we may get a lot of kernel=0 readings
    #if _MSC_VER
        AssertGreater(t1.kernel_time_us, 0u);
        AssertGreater(t2.kernel_time_us, 0u);
    #endif
        AssertGreater(t1.user_time_us, 0u);
        AssertGreater(t1.cpu_time_us, 0u);
        AssertGreater(t2.user_time_us, 0u);
        // only compare CPU times, since we're uncertain about USER/KERNEL distribution
        AssertGreaterOrEqual(t2.cpu_time_us, t1.cpu_time_us);

        // TODO: on some Linux VM-s this test fails because the kernel time is 0,
        //       probably because spin_sleep_for_us clock uses CPU RDTSC instruction instead of syscall
        // since calculations are done using system clock, most time should be spent in kernel
        // AssertGreater(kernel_delta, user_delta);

        #if _MSC_VER
            // TODO: Windows CPU usage timing accuracy is 15.6ms, so we need to relax the timings
            AssertGreaterOrEqual(cpu_delta, 0);
            AssertLessOrEqual(cpu_delta, spin_us + 64'000);
        #else
            AssertGreaterOrEqual(cpu_delta, spin_us - 5'000);
            AssertLessOrEqual(cpu_delta, spin_us + 5'000);
        #endif
    }
#endif // !__EMSCRIPTEN__
};
