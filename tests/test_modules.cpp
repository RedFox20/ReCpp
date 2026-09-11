/**
 * Checks that the C++20 modules are usable the way a consumer uses them.
 * Only built when the toolchain carries modules, see BUILD_WITH_MODULES in CMakeLists.txt.
 */
#include <rpp/tests.h>

#if RPP_BUILD_WITH_MODULES

#include <rpp/debugging.macros.h> // LogInfo, LogWarning, Assert, ThrowErr
#include <stdexcept>              // ThrowErr throws std::runtime_error
#include <string>
#include <vector>
#include <mutex>                  // std::unique_lock, which rpp::spin_lock returns
#include <memory>                 // std::make_shared, which rpp::atomic_shared_ptr takes
#include <type_traits>            // std::is_same_v, which pins an exported signature
#include <atomic>                 // std::atomic_bool, which rpp::atomic_test_and_set takes
#include <future>                 // std::promise, which rpp::cpromise aliases

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
import rpp.sprint;
import rpp.task;
import rpp.vec;
import rpp.load_balancer;
import rpp.memory_pool;
import rpp.mutex;
import rpp.paths;
import rpp.tests;
import rpp.atomic_shared_ptr;
import rpp.close_sync;
import rpp.condition_variable;
import rpp.file_io;
import rpp.sockets;
import rpp.binary_stream;
import rpp.concurrent_queue;
import rpp.semaphore;
import rpp.binary_serializer;
import rpp.thread_pool;
import rpp.future;
import rpp.event_loop;

// test_modules_identity.cpp takes this address through the module and includes no rpp header
const void* module_pi_addr() noexcept;

// serializable<T> is a CRTP base which calls introspect() once, so the probe needs a real type
struct module_point : rpp::serializable<module_point>
{
    int x = 0;
    float y = 0.0f;
    void introspect() { bind_name("x", x); bind_name("y", y); }
};

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

    // a namespace-scope constant needs `inline`, or the module and the header get one copy each
    TestCase(math_constant_is_one_entity)
    {
        const void* header = &rpp::PI;
        AssertThat(module_pi_addr() == header, true);
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
        using namespace rpp; // duration_literals is inline, so this alone reaches the operator
        rpp::Duration d = rpp::millis(250);
        AssertThat(d.millis(), 250);
        AssertThat((1_s).seconds(), 1);
        AssertThat((250_ms).millis(), 250);
        AssertThat(rpp::NANOS_PER_SEC, 1'000'000'000LL);
        // Monotonic never steps back, and TimePoint::now() reads the adjustable realtime clock
        rpp::TimePoint start = rpp::TimePoint::now(rpp::ClockType::Monotonic);
        rpp::sleep_ms(1);
        AssertGreaterOrEqual((rpp::TimePoint::now(rpp::ClockType::Monotonic) - start).nsec, 0);
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

        // collections.h declares rpp::sort, not sort.h, so rpp.collections carries it
        rpp::sort(v);
        AssertThat(v[0], 1);
        AssertThat(v[2], 4);
        rpp::sort(v, [](int a, int b) { return a > b; });
        AssertThat(v[0], 4);
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
        // the module hides the six host-only names on bare metal, so the test follows it
#if !RPP_BARE_METAL
        AssertGreater(rpp::num_physical_cores(), 0);
        AssertGreater(rpp::get_thread_id(), 0u);
        rpp::set_this_thread_name("module_probe");
        AssertThat(rpp::get_this_thread_name(), std::string{"module_probe"});
#endif
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

    TestCase(sprint_module_carries_the_whole_surface)
    {
        AssertThat(rpp::sprint(1, "and", 2.5), "1 and 2.5"); // sprint separates the arguments
        AssertThat(rpp::to_string('x'), "x");
        AssertThat(rpp::to_hex_string(rpp::strview{"AB"}), "4142");

        rpp::string_buffer sb;
        sb << "n=" << 42 << " v=" << std::vector<int>{ 1, 2 };
        AssertThat(sb.view(), "n=42 v={ 1, 2 }");
        AssertThat(rpp::format("%d-%s", 7, "x"), "7-x");
    }

    TestCase(load_balancer_module_carries_the_whole_surface)
    {
        rpp::load_balancer balancer { 1000 };
        AssertThat(balancer.get_max_bytes_per_sec(), 1000u);
        AssertGreater(balancer.avg_nanos_between_bytes(), 0u);
        AssertThat(balancer.can_send(), true); // nothing sent yet, so the budget is open
    }

    TestCase(memory_pool_module_carries_the_whole_surface)
    {
        rpp::linear_static_pool pool { 4096 };
        // the mixin stays off the module surface, so this proves an inherited member still reaches an importer
        int* value = pool.construct<int>(7);
        AssertThat(*value, 7);

        int* array = pool.allocate_array<int>(4);
        array[3] = 11;
        AssertThat(array[3], 11);
    }

    TestCase(mutex_module_carries_the_whole_surface)
    {
        rpp::synchronized<std::string> guarded { "value" };
        AssertThat(*guarded, "value");
        *guarded = "changed";
        AssertThat(*guarded, "changed");

        rpp::mutex m;
        AssertThat(m.try_lock(), true);
        m.unlock();
        std::unique_lock<rpp::mutex> held = rpp::spin_lock(m);
        AssertThat(held.owns_lock(), true);
    }

    TestCase(paths_module_carries_the_whole_surface)
    {
        AssertThat(rpp::path_combine("dir", "file.txt"), "dir/file.txt");
        AssertThat(rpp::file_ext("dir/file.txt"), "txt");
        AssertThat(rpp::file_name("dir/file.txt"), "file");
        AssertThat(rpp::folder_name("dir/file.txt"), "dir");
        AssertThat(rpp::file_exists("this_file_does_not_exist.txt"), false);
        AssertNotEqual(rpp::working_dir(), std::string{});
    }

    TestCase(tests_module_carries_the_whole_surface)
    {
        // this suite runs through rpp::test, so the module names the type the suite already is
        AssertThat(rpp::Compare::eq(name, name), true);
        AssertThat(rpp::Compare::lt(1, 2), true);
        AssertThat(int(rpp::TestVerbosity::Summary), 1);

        rpp::test_info info { rpp::strview{"probe"}, nullptr };
        AssertThat(info.test_enabled, true);
        AssertThat(info.auto_run, true);
        rpp::test_factory factory = info.factory;
        AssertThat(factory == nullptr, true);
    }

    TestCase(atomic_shared_ptr_module_carries_the_whole_surface)
    {
        rpp::atomic_shared_ptr<int> p { std::make_shared<int>(7) };
        AssertThat(*p.load(), 7);
        // the standard library picks the answer, so this pins the signature and not the value
        static_assert(std::is_same_v<decltype(p.is_lock_free()), bool>);

        std::shared_ptr<int> old = p.exchange(std::make_shared<int>(9));
        AssertThat(*old, 7);
        AssertThat(*p.load(), 9);

        rpp::atomic_weak_ptr<int> w { p.load() };
        AssertThat(w.load().expired(), false);
    }

    TestCase(close_sync_module_carries_the_whole_surface)
    {
        rpp::close_sync sync;
        AssertThat(sync.is_alive(), true);
        AssertThat(sync.is_closing(), false);
        AssertThat(sync.is_dead_or_closing(), false);

        rpp::readonly_lock shared = sync.try_readonly_lock();
        AssertThat(shared.owns_lock(), true);
    }

    TestCase(condition_variable_module_carries_the_whole_surface)
    {
        // the deadline already passed, so the helper reports no time left
        rpp::TimePoint past = rpp::TimePoint::monotonic_now() - rpp::seconds_f(1.0);
        AssertThat(rpp::_cv_remaining_duration(past).nsec, 0LL);

        rpp::condition_variable cv;
        rpp::mutex m;
        std::unique_lock<rpp::mutex> lock { m };
        AssertThat(cv.wait_for(lock, rpp::millis(1)) == std::cv_status::timeout, true);
    }

    TestCase(file_io_module_carries_the_whole_surface)
    {
        std::string path = rpp::path_combine(rpp::temp_dir(), "rpp_module_probe.txt");
        AssertThat(rpp::file::write_new(path, "abc", 3), 3);

        rpp::file f { path, rpp::file::READONLY };
        AssertThat(f.good(), true);
        AssertThat(f.size(), 3);

        rpp::load_buffer buf = rpp::file::read_all(path);
        AssertThat(rpp::strview(buf.str, buf.len), "abc");
        f.close();
        rpp::delete_file(path);
    }

    TestCase(sockets_module_carries_the_whole_surface)
    {
        rpp::ipaddress addr { rpp::AF_IPv4, "127.0.0.1", 1337 };
        AssertThat(addr.port(), 1337);
        AssertThat(addr.is_valid(), true);
        AssertThat(addr.str(), "127.0.0.1:1337");

        // the platform value differs per system, so the round trip is what the module owes
        AssertThat(rpp::to_addrfamily(rpp::addrfamily_int(rpp::AF_IPv4)) == rpp::AF_IPv4, true);
        rpp::socket s = rpp::make_udp_randomport();
        AssertThat(s.good(), true);
        AssertGreater(s.port(), 0);
    }

    TestCase(binary_stream_module_carries_the_whole_surface)
    {
        rpp::binary_buffer buf;
        buf << rpp::int32{7} << rpp::strview{"seven"};
        AssertThat(buf.size(), 4 + int(sizeof(rpp::binary_buffer::strlen_t)) + 5);

        AssertThat(buf.read_int32(), 7);
        AssertThat(buf.read_string(), "seven");
        AssertThat(buf.available(), 0);
    }

    TestCase(concurrent_queue_module_carries_the_whole_surface)
    {
        rpp::concurrent_queue<int> queue;
        AssertThat(queue.empty(), true);

        queue.push(11);
        queue.push(22);
        AssertThat(queue.size(), 2);

        int item = 0;
        AssertThat(queue.try_pop(item), true);
        AssertThat(item, 11);

        queue.clear();
        AssertThat(queue.empty(), true);
    }

    TestCase(semaphore_module_carries_the_whole_surface)
    {
        rpp::semaphore sem { 1 };
        AssertThat(sem.count(), 1);
        AssertThat(sem.try_wait(), true);
        AssertThat(sem.try_wait(), false);

        sem.notify();
        AssertThat(sem.count(), 1);

        rpp::semaphore_flag flag;
        AssertThat(flag.is_set(), false);

        // a weak CAS fails spuriously only where it matches, so the mismatch is the stable case
        std::atomic_bool idle { false };
        AssertThat(rpp::atomic_test_and_set(idle), false);
        AssertThat(idle.load(), false);

        std::atomic_bool running { true };
        bool acquired = false;
        for (int i = 0; i < 100 && !acquired; ++i)
            acquired = rpp::atomic_test_and_set(running);
        AssertThat(acquired, true);
        AssertThat(running.load(), false);
    }

    TestCase(binary_serializer_module_carries_the_whole_surface)
    {
        module_point written;
        written.x = 7;
        written.y = 2.5f;

        const std::vector<rpp::member_serialize<module_point>>& members = module_point::members;
        AssertThat(int(members.size()), 2);
        AssertThat(members[0].name, "x");

        rpp::binary_buffer buf;
        buf << written;
        module_point read;
        buf >> read;
        AssertThat(read.x, 7);
        AssertThat(read.y, 2.5f);

        rpp::string_buffer sb;
        written.serialize(sb);
        AssertThat(sb.view().starts_with("x;7;"), true);

        rpp::strview line = sb.view();
        module_point parsed;
        line >> parsed;
        AssertThat(parsed.x, 7);
        AssertThat(parsed.y, 2.5f);
    }

    TestCase(thread_pool_module_carries_the_whole_surface)
    {
        static_assert(std::is_same_v<rpp::task_delegate<void()>, rpp::delegate<void()>>);
        static_assert(std::is_same_v<rpp::duration_t<float>, rpp::fseconds_t>);

        // a local pool keeps the probe off whatever parallelism another suite left on the global
        rpp::thread_pool local { 2 };
        AssertThat(local.max_parallelism(), 2);
        AssertThat(local.active_tasks(), 0);

        std::atomic_int counted { 0 };
        rpp::parallel_for(0, 8, 1, [&](int start, int end) { counted += end - start; });
        AssertThat(counted.load(), 8);

        std::vector<int> items { 1, 2, 3 };
        std::atomic_int summed { 0 };
        rpp::parallel_foreach(items, [&](int item) { summed += item; });
        AssertThat(summed.load(), 6);

        rpp::pool_task_handle handle = rpp::parallel_task([&] { counted += 1; });
        rpp::wait_result waited = handle.wait(rpp::seconds(1));
        AssertThat(waited == rpp::wait_result::finished, true);
        AssertThat(counted.load(), 9);

        rpp::parallel_task_detached([&] { summed += 1; });
        rpp::thread_pool& shared = rpp::thread_pool::global();
        AssertThat(shared.wait_until_idle(rpp::seconds(1)) == rpp::wait_result::finished, true);
        AssertThat(summed.load(), 7);
    }

    // clang rejects a plain function which returns a cfuture, so the launcher takes the wrapper
    RPP_CORO_WRAPPER static rpp::cfuture<void> module_launch(int&) { return {}; }

    TestCase(future_module_carries_the_whole_surface)
    {
        // gcc-14 crashes when an importer instantiates std::promise, so name these unevaluated, see BUGS.md B16
        static_assert(std::is_same_v<rpp::cpromise<int>, std::promise<int>>);
        static_assert(std::is_same_v<decltype(rpp::async_task(+[]{ return 1; })), rpp::cfuture<int>>);
        static_assert(std::is_same_v<decltype(rpp::make_ready_future(1)), rpp::cfuture<int>>);
        static_assert(std::is_same_v<decltype(rpp::make_ready_future()), rpp::cfuture<void>>);
        static_assert(std::is_same_v<decltype(rpp::make_exceptional_future<int>(1)), rpp::cfuture<int>>);

        // a filled vector needs a real cfuture, and every way to build one runs a promise
        std::vector<rpp::cfuture<int>> no_ints;
        rpp::wait_all(no_ints);
        AssertThat(int(rpp::get_all(no_ints).size()), 0);

        std::vector<rpp::cfuture<void>> no_voids;
        rpp::get_all(no_voids);

        std::vector<int> no_items;
        rpp::run_tasks(no_items, &module_launch);
    }

#if RPP_HAS_COROUTINES
    // an eager task runs to completion at construction, so this needs no event loop
    static rpp::task<int> module_task() { co_return 99; }

    TestCase(task_module_carries_the_whole_surface)
    {
        rpp::task<int> t = module_task();
        AssertThat(t.valid(), true);
        AssertThat(t.done(), true);
        AssertThat(t.await_ready(), true);
    }

    // event_task starts eagerly, so one with no co_await ends before the caller sees it
    static rpp::event_task module_event_task(int* out) { *out = 3; co_return; }

    TestCase(event_loop_module_carries_the_whole_surface)
    {
        rpp::event_loop loop;
        AssertThat(loop.main_thread_id(), rpp::get_thread_id());
        AssertThat(loop.has_pending_work(), false);

        int posted = 0;
        loop.post([&] { ++posted; });
        loop.run_until_idle();
        AssertThat(posted, 1);

        int value = 0;
        rpp::event_task task = module_event_task(&value);
        AssertThat(value, 3);
        AssertThat(task.done(), true);
    }
#endif
};

#endif // RPP_BUILD_WITH_MODULES
