/**
 * Checks that the C++20 modules are usable the way a consumer uses them.
 * Only built when the toolchain carries modules, see BUILD_WITH_MODULES in CMakeLists.txt.
 */
#include <rpp/tests.h>

#if RPP_BUILD_WITH_MODULES

#include <rpp/debugging.macros.h> // LogInfo, LogWarning, Assert, ThrowErr
#include <stdexcept>              // ThrowErr throws std::runtime_error
#include <string>

import rpp.strview;   // includes come first, the import goes last
import rpp.debugging;
import rpp.obfuscated_string;
import rpp.scopeguard;
import rpp.bitutils;
import rpp.traits;
import rpp.endian;
import rpp.predicates;
import rpp.sort;
import rpp.proc_utils;
import rpp.math;
import rpp.timepoint;
import rpp.delegate;
import rpp.atomic_timepoint;
import rpp.collections;
import rpp.stack_trace;
import rpp.threads;
import rpp.timer;
import rpp.vec;

TestImpl(test_modules)
{
    TestInit(test_modules)
    {
    }

    static void log_handler(void* /*user*/, LogSeverity /*severity*/,
                            const char* /*msg*/, int /*len*/) noexcept {}

    // The macros expand to _LogInfo, _LogWarning, _FmtString and rpp::__wrap, so this
    // fails to build if rpp.debugging stops exporting any of them.
    TestCase(debugging_macros_work_beside_the_module)
    {
        LogSeverity previous = GetLogSeverityFilter();
        SetLogSeverityFilter(LogSeverityWarn);
        AssertThat(GetLogSeverityFilter(), LogSeverityWarn);

        LogInfo("filtered out at Warn: %d", 42);
        LogInfoFL("file.cpp", 1, "func", "custom source %d", 1);

        std::string wrapped = "passed through rpp::__wrap";
        LogWarning("%s", wrapped); // no .c_str(), the macro wraps it

        // no Assert() here: <rpp/tests.h> defines its own one-argument Assert and wins
        AssertExpr(GetLogSeverityFilter() == LogSeverityWarn);
        DbgAssert(true, "never fires %d", 0);

        rpp::add_log_handler(nullptr, &log_handler);
        rpp::remove_log_handler(nullptr, &log_handler);

        SetLogSeverityFilter(previous);
    }

    TestCase(throw_err_macro_reaches_the_module)
    {
        bool caught = false;
        try { ThrowErr("thrown %d", 3); }
        catch (const std::exception& e) { caught = true; AssertThat(rpp::strview{e.what()}, "thrown 3"); }
        AssertThat(caught, true);
    }

    // rpp::strview from the module and from <rpp/strview.h> name one type, so a value
    // built here crosses into code that only included the header.
    TestCase(strview_module_and_header_name_one_type)
    {
        using namespace rpp::literals; // the module exports the literal operator too
        rpp::strview sv = "hello world"_sv;
        AssertThat(sv.length(), 11);
        AssertThat(rpp::concat(sv, "!"), "hello world!");
        AssertThat(sv.next(' '), "hello"); // next() consumes the token from sv
        AssertThat(sv, "world");
    }

    // this file includes no obfuscated_string.h, so the module alone carries the surface
    TestCase(obfuscated_string_module_carries_the_whole_surface)
    {
        using namespace rpp::literals;
        constexpr auto email = rpp::make_obfuscated("super@secret.com");
        AssertNotEqual(std::string{email.obfuscated()}, std::string{"super@secret.com"});
        AssertThat(email.to_string(), std::string{"super@secret.com"});
        constexpr auto literal = "super@secret.com"_obfuscated;
        AssertThat(literal.to_string(), std::string{"super@secret.com"});
    }

    // this file includes no scope_guard.h, so the module alone carries the surface
    TestCase(scopeguard_module_carries_the_whole_surface)
    {
        static_assert(sizeof(rpp::scope_finalizer<void(*)()>) > 0, "the module must export scope_finalizer");
        int calls = 0;
        {
            auto guard = rpp::make_scope_guard([&]{ ++calls; });
            AssertThat(calls, 0);
        }
        AssertThat(calls, 1); // the guard ran when it left the scope
    }

    // No case below includes a header, so the L1 module alone carries the surface.
    // <rpp/tests.h> masks type_traits, source_loc and future_types, see l1_module_only.cpp

    TestCase(bitutils_module_carries_the_whole_surface)
    {
        rpp::bit_array bits{ 16 };
        bits.set(3);
        AssertThat(bits.isSet(3), true);
        AssertThat(bits.isSet(4), false);
    }

    TestCase(traits_module_carries_the_whole_surface)
    {
        using Fn = int(*)(double);
        static_assert(std::is_same_v<rpp::function_traits<Fn>::ret_type, int>);
        static_assert(std::is_same_v<rpp::first_arg_type<Fn>, double>);
        AssertThat(sizeof(rpp::function_traits<Fn>), sizeof(rpp::function_traits<Fn>));
    }

    TestCase(endian_module_carries_the_whole_surface)
    {
        uint8_t buf[8] = {};
        rpp::writeBEU32(buf, 0x01020304u);
        AssertThat(rpp::readBEU32(buf), 0x01020304u);
        rpp::writeLEU16(buf, uint16_t(0xBEEF));
        AssertThat(rpp::readLEU16(buf), uint16_t(0xBEEF));
    }

    TestCase(predicates_module_carries_the_whole_surface)
    {
        auto yes = []{ return true; };
        static_assert(rpp::IsCallable<decltype(yes)>);
        static_assert(rpp::IsPredicate<decltype(yes)>);
        AssertThat(yes(), true);
    }

    TestCase(sort_module_carries_the_whole_surface)
    {
        int values[5] = { 4, 2, 5, 1, 3 };
        rpp::insertion_sort(values, 5, [](int a, int b) { return a < b; });
        AssertThat(values[0], 1);
        AssertThat(values[4], 5);
        static_assert(std::is_same_v<rpp::container_element_t<std::string>, char>);
    }

    TestCase(proc_utils_module_carries_the_whole_surface)
    {
        rpp::proc_mem_info mem = rpp::proc_current_mem_used();
        AssertGreater(mem.virtual_size, 0u);
        rpp::cpu_usage_info cpu = rpp::proc_total_cpu_usage();
        AssertGreaterOrEqual(cpu.cpu_time_us, 0);
    }

    TestCase(math_module_carries_the_whole_surface)
    {
        AssertThat(rpp::clamp(5, 0, 3), 3);
        AssertThat(rpp::lerp(0.5, 30.0, 60.0), 45.0);
        AssertThat(rpp::lerpInverse(45.0, 30.0, 60.0), 0.5);
        AssertThat(rpp::nearlyZero(0.0001), true);
        AssertThat(rpp::almostEqual(1.0, 1.0001), true);
        AssertGreater(rpp::PI, 3.14);
        AssertThat(rpp::degf(rpp::radf(90.0f)), 90.0f);
    }

    TestCase(timepoint_module_carries_the_whole_surface)
    {
        using namespace rpp::duration_literals;
        rpp::Duration d = rpp::millis(250);
        AssertThat(d.millis(), 250);
        AssertThat((1_s).seconds(), 1);
        AssertThat(rpp::NANOS_PER_SEC, 1'000'000'000LL);
        rpp::TimePoint start = rpp::TimePoint::now();
        rpp::sleep_ms(1);
        AssertGreaterOrEqual((rpp::TimePoint::now() - start).nsec, 0);
    }

    TestCase(delegate_module_carries_the_whole_surface)
    {
        int calls = 0;
        rpp::delegate<void(int)> d = [&](int n) { calls += n; };
        d(2);
        AssertThat(calls, 2);

        rpp::multicast_delegate<int> m;
        m += [&](int n) { calls += n; };
        m(3);
        AssertThat(calls, 5);
    }

    // L2. <rpp/tests.h> masks sprint, so masked_module_only.cpp covers that one.

    TestCase(atomic_timepoint_module_carries_the_whole_surface)
    {
        rpp::AtomicDuration d;
        d.store(rpp::millis(40));
        AssertThat(d.load().millis(), 40);
        rpp::AtomicTimePoint t;
        t.store(rpp::TimePoint::now());
        AssertThat(t.load().is_valid(), true);
    }

    TestCase(collections_module_carries_the_whole_surface)
    {
        std::vector<int> v { 4, 1, 3 };
        AssertThat(rpp::contains(v, 3), true);
        AssertThat(rpp::index_of(v, 1), 1);
        AssertThat(rpp::sum_all(v), 8);
        AssertThat(rpp::any_of(v, [](int n) { return n > 3; }), true);
        rpp::element_range<int> r = rpp::range(v);
        AssertThat(int(r.size()), 3);
    }

    TestCase(stack_trace_module_carries_the_whole_surface)
    {
        static_assert(rpp::CALLSTACK_MAX_DEPTH == 256u);
        std::vector<uint64_t> frames = rpp::get_callstack(8);
        AssertGreater(frames.size(), 0u);
        AssertGreater(rpp::format_trace("probe", frames.data(), frames.size()).size(), 0u);
    }

    TestCase(threads_module_carries_the_whole_surface)
    {
        AssertGreater(rpp::num_physical_cores(), 0);
        AssertGreater(rpp::get_thread_id(), 0u);
        rpp::set_this_thread_name("module_probe");
        AssertThat(rpp::get_this_thread_name(), std::string{"module_probe"});
        rpp::yield();
    }

    TestCase(timer_module_carries_the_whole_surface)
    {
        rpp::Timer t;
        rpp::sleep_ms(1);
        AssertGreater(t.elapsed_millis(), 0.0);
        rpp::StopWatch sw;
        sw.start();
        sw.stop();
        AssertGreaterOrEqual(sw.elapsed(), 0.0);
    }

    TestCase(vec_module_carries_the_whole_surface)
    {
        rpp::Vector3 a { 1.0f, 2.0f, 3.0f };
        rpp::Vector3 b = rpp::Vector3::One();
        rpp::Vector3 sum = a + b;
        AssertThat(sum.x, 2.0f);
        AssertThat(sum.z, 4.0f);
        rpp::Vector2 p { 3.0f, 4.0f };
        AssertThat(p.length(), 5.0f);
    }
};

#endif // RPP_BUILD_WITH_MODULES
