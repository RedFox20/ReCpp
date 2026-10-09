#include <rpp/async.h>
#include <rpp/delegate.h> // rpp::delegate, which an event loop runs
#include <rpp/event_loop.h>
#include <rpp/minmax.h> // rpp::min, rpp::max
#include <rpp/scope_guard.h> // rpp::make_scope_guard
#include <rpp/semaphore.h>
#include <rpp/threads.h> // rpp::get_thread_id
#include <rpp/timepoint.h>
#include <rpp/tests.h>
#include <atomic>
#include <cstdint> // std::uintptr_t
#include <exception> // std::current_exception, std::exception_ptr
#include <map>
#include <stdexcept>
#include <string> // std::string
#include <utility> // std::exchange, std::pair
#include <vector>
#include "warning_capture.h"
#if _MSC_VER
#  include <intrin.h> // _AddressOfReturnAddress
#endif
using namespace rpp;
using namespace std::string_literals;

// NOLINTBEGIN(performance-*)

TestImpl(test_async)
{
    TestInit(test_async) {}

    TestCase(event_loop_pumps_future_results_and_exceptions)
    {
        rpp::event_loop loop;
        promise<int> value;
        future<int> result = value.get_future();
        loop.post([&] { value.set_value(42); });
        AssertThat(loop.run_until_ready(result), 42);
        AssertThat(result.valid(), false);

        promise<void> done;
        future<void> completed = done.get_future();
        loop.post([&] { done.set_value(); });
        loop.run_until_ready(completed);
        AssertThat(completed.valid(), false);

        future<int> failed = rpp::exceptional_future<int>(std::runtime_error{"loop_result"});
        AssertThrows(loop.run_until_ready(failed), std::runtime_error);
        AssertThat(failed.valid(), false);
    }

    TestCase(event_loop_future_timeout_keeps_the_result)
    {
        rpp::event_loop loop;
        promise<int> value;
        future<int> result = value.get_future();
        AssertThat(loop.pump_until_ready(result, rpp::Duration::zero()), false);
        AssertThrows(loop.run_until_ready(result, rpp::Duration::zero()), std::runtime_error);
        value.set_value(7);
        AssertThat(loop.run_until_ready(result, rpp::Duration::zero()), 7);
        AssertThat(loop.pump_until_ready(result), false);
    }

    TestCase(run_tasks_launches_every_future_before_waiting)
    {
        std::vector<int> items {1, 2, 3};
        std::atomic_int total {0};
        std::vector<promise<void>> pending;
        struct launcher
        {
            std::atomic_int& total;
            std::vector<promise<void>>& pending;
            size_t count;
            RPP_CORO_WRAPPER future<void> operator()(int& item) const
            {
                total.fetch_add(item);
                pending.emplace_back();
                future<void> f = pending.back().get_future();
                if (pending.size() == count)
                    for (promise<void>& p : pending) p.set_value();
                return f;
            }
        };
        launcher launch {total, pending, items.size()};
        rpp::run_tasks(items, launch);
        AssertThat(total.load(), 6);
        std::vector<int> empty;
        rpp::run_tasks(empty, launch);
    }

    TestCase(run_tasks_drains_results_before_rethrowing)
    {
        std::vector<int> items {1, 2, 3};
        int launched = 0;
        rpp::semaphore release;
        std::atomic_bool completed {false};
        struct launcher
        {
            int& launched;
            rpp::semaphore& release;
            std::atomic_bool& completed;
            RPP_CORO_WRAPPER future<void> operator()(int& item) const
            {
                ++launched;
                if (item == 1) return rpp::exceptional_future<void>(std::runtime_error{"task_failed"});
                if (item == 2) return rpp::async([this] { release.wait(rpp::seconds(1)); completed.store(true); });
                release.notify();
                return rpp::ready_future();
            }
        };
        launcher launch {launched, release, completed};
        AssertThrows(rpp::run_tasks(items, launch), std::runtime_error);
        AssertThat(launched, 3);
        AssertThat(completed.load(), true);

        completed.store(false);
        struct throwing_launcher
        {
            rpp::semaphore& release;
            std::atomic_bool& completed;
            RPP_CORO_WRAPPER future<void> operator()(int& item) const
            {
                if (item == 2)
                {
                    release.notify();
                    throw std::logic_error{"launcher_failed"};
                }
                return rpp::async([this] { release.wait(rpp::seconds(1)); completed.store(true); });
            }
        };
        throwing_launcher throwing {release, completed};
        AssertThrows(rpp::run_tasks(items, throwing), std::logic_error);
        AssertThat(completed.load(), true);
    }

    TestCase(run_tasks_keeps_the_legacy_launcher)
    {
        struct launcher
        {
            RPP_CORO_WRAPPER cfuture<void> operator()(int& item) const
            {
                return rpp::async_task([&item] { ++item; });
            }
        };
        std::vector<int> items {1, 2};
        rpp::run_tasks(items, launcher{});
        AssertThat(items[0], 2);
        AssertThat(items[1], 3);
    }

    static rpp::event_task await_on_loop(rpp::event_loop& loop, future<int> result, int& value, rpp::uint64& thread)
    {
        value = co_await loop.run_async(std::move(result));
        thread = rpp::get_thread_id();
    }

    TestCase(await_future_resumes_on_the_loop_thread)
    {
        rpp::event_loop loop;
        int value = 0;
        rpp::uint64 thread = 0;
        promise<int> pending;
        rpp::event_task task = await_on_loop(loop, pending.get_future(), value, thread);
        AssertThat(task.done(), false);
        AssertThat(loop.background_tasks(), 1);
        pending.set_value(42);
        AssertThat(task.done(), false);
        loop.run_until_done(task);
        AssertThat(value, 42);
        AssertThat(thread, rpp::get_thread_id());
        AssertThat(loop.background_tasks(), 0);

        task = await_on_loop(loop, rpp::ready_future(7), value, thread);
        AssertThat(task.done(), false);
        loop.run_until_done(task);
        AssertThat(value, 7);

        task = await_on_loop(loop, rpp::exceptional_future<int>(std::runtime_error{"await_failed"}), value, thread);
        AssertThrows(loop.run_until_done(task), std::runtime_error);
    }

    TestCase(loop_drain_waits_for_a_pending_future)
    {
        rpp::event_loop loop;
        int value = 0;
        rpp::uint64 thread = 0;
        promise<int> pending;
        rpp::event_task task = await_on_loop(loop, pending.get_future(), value, thread);
        AssertThat(loop.has_pending_work(), true);
        future<void> producer = rpp::async([&] { pending.set_value(42); });
        loop.run_until_idle();
        producer.get();
        AssertThat(task.done(), true);
        AssertThat(value, 42);
        AssertThat(thread, rpp::get_thread_id());
        AssertThat(loop.has_pending_work(), false);
    }

    TestCase(simple_chaining)
    {
        promise<std::string> loadString;
        future<bool> chain = loadString.get_future().then([](std::string arg) { return arg == "future string"; });
        loadString.set_value("future string"); // this line starts the continuation as a pool task
        AssertThat(chain.get(), true);
    }

    TestCase(chain_mutate_void_to_string)
    {
        promise<void> loadSomething;
        future<std::string> chained = loadSomething.get_future().then([] { return "operation complete!"s; });
        loadSomething.set_value();
        AssertThat(chained.get(), "operation complete!"s);
    }

    TestCase(chain_decay_string_to_void)
    {
        promise<std::string> loadString;
        std::string received;
        future<void> chained = loadString.get_future().then([&](std::string s) { received = s; });
        loadString.set_value("some string");
        chained.get();
        AssertThat(received, "some string"s);
    }

    TestCase(cross_thread_exception_propagation)
    {
        future<void> asyncThrowingTask = rpp::async([] { throw std::runtime_error("background_thread_exception_msg"); });
        std::string result;
        try { asyncThrowingTask.get(); }
        catch (const std::runtime_error& e) { result = e.what(); }
        AssertThat(result, "background_thread_exception_msg"s);
    }

    TestCase(composable_future_type)
    {
        int totalCalls = 0;
        std::string first;
        int second = 0;
        future<std::string> f = rpp::async([] { return "future string"s; });
        f.then([&](std::string s) { ++totalCalls; first = s; return 42; }).then([&](int x) { ++totalCalls; second = x; }).get();
        AssertThat(totalCalls, 2);
        AssertThat(first, "future string"s);
        AssertThat(second, 42);
    }

    TestCase(except_handler)
    {
        future<void> f = rpp::async([] { throw std::runtime_error("background_thread_exception_msg"); });
        bool taskCalled = false;
        std::string handled;
        int result = f.then([&] { taskCalled = true; return 0; },
                            [&](const std::exception& e) { handled = e.what(); return 42; }).get();
        AssertThat(taskCalled, false);
        AssertThat(handled, "background_thread_exception_msg"s);
        AssertThat(result, 42);
    }

    TestCase(except_handlers_catch_first)
    {
        future<void> f = rpp::async([] { throw std::domain_error("background_thread_exception_msg"); });
        std::string handled;
        int result = f.then([] { return 0; }, [&](std::domain_error e) { handled = e.what(); return 42; },
                            [](std::runtime_error) { return 21; }).get();
        AssertThat(handled, "background_thread_exception_msg"s);
        AssertThat(result, 42);
    }

    TestCase(except_handlers_catch_second)
    {
        future<void> f = rpp::async([] { throw std::runtime_error("background_thread_exception_msg"); });
        std::string handled;
        int result = f.then([] { return 0; }, [](std::domain_error) { return 21; },
                            [&](std::runtime_error e) { handled = e.what(); return 42; }).get();
        AssertThat(handled, "background_thread_exception_msg"s);
        AssertThat(result, 42);
    }

    struct SpecificError : std::range_error
    {
        using std::range_error::range_error;
    };

    TestCase(except_handlers_catch_third)
    {
        future<void> f = rpp::async([] { throw std::runtime_error("background_thread_exception_msg"); });
        std::string handled;
        int result = f.then([] { return 0; }, [](const SpecificError&) { return 1; }, [](const std::range_error&) { return 2; },
                            [&](const std::runtime_error& e) { handled = e.what(); return 3; }).get();
        AssertThat(handled, "background_thread_exception_msg"s);
        AssertThat(result, 3);
    }

    // the handlers read like a chain of catch blocks, so the first match wins and not the most specific one
    TestCase(except_handlers_the_first_match_wins)
    {
        future<void> f = rpp::exceptional_future<void>(SpecificError{"specific_error_msg"});
        int result = f.then([] { return 0; },
                            [](const std::range_error&) { return 1; },
                            [](const SpecificError&) { return 2; }).get();
        AssertThat(result, 1);
    }

    // a catch block never catches what a sibling catch block throws, so neither does a later handler
    TestCase(a_handler_which_throws_skips_the_later_handlers)
    {
        future<void> f = rpp::exceptional_future<void>(std::domain_error{"domain_msg"});
        future<int> chained = f.then([] { return 0; },
                                     [](const std::domain_error&) -> int { throw std::runtime_error("handler_msg"); },
                                     [](const std::runtime_error&) { return 2; });
        AssertThrows((void)chained.get(), std::runtime_error);
    }

    TestCase(except_handler_chaining)
    {
        std::string handled;
        future<int> failed = rpp::async([] { return "future string"s; }).then([](std::string) -> int {
            throw std::runtime_error("future_continuation_exception_msg");
        });
        int result = failed.then([](int) { return 5; }, [&](const std::exception& e) { handled = e.what(); return 42; }).get();
        AssertThat(handled, "future_continuation_exception_msg"s);
        AssertThat(result, 42);
    }

    TestCase(chain_async_futures_void)
    {
        std::string ran; // each task appends its number, so the order shows too
        future<void> tasks;
        tasks.chain_async([&] { ran += '1'; }).chain_async([&] { ran += '2'; throw std::runtime_error("task2 failed"); });
        tasks.chain_async([&] { ran += '3'; });
        tasks.get(); // waits for task3, which ran after task1 and task2
        AssertThat(ran, "123"s);
    }

    TestCase(chain_async_futures_T)
    {
        std::string ran;
        future<std::string> tasks;
        tasks.chain_async([&] { ran += '1'; return "task1"s; });
        tasks.chain_async([&]() -> std::string { ran += '2'; throw std::runtime_error("task2 failed"); });
        tasks.chain_async([&] { ran += '3'; return "task3"s; });
        AssertThat(tasks.get(), "task3"s); // waits for task3, which ran after task1 and task2
        AssertThat(ran, "123"s);
    }

    TestCase(chain_async_futures_continue_completed)
    {
        std::string ran;
        future<void> tasks;
        tasks.chain_async([&] { ran += '1'; }).chain_async([&] { ran += '2'; });
        tasks.wait(); // waits for task2, which ran after task1
        AssertThat(ran, "12"s);

        tasks.chain_async([&] { ran += '3'; });
        tasks.get();
        AssertThat(ran, "123"s);
    }

    TestCase(chain_async_takes_a_future)
    {
        future<int> tasks;
        tasks.chain_async(ready_future(1)); // an invalid future becomes the one it takes
        tasks.chain_async(rpp::async([] { return 2; }));
        AssertThat(tasks.get(), 2);
    }

    TestCase(ready_future)
    {
        future<int> f = rpp::ready_future(42);
        AssertThat(f.await_ready(), true);
        AssertThat(f.get(), 42);

        future<std::vector<int>> braced = rpp::ready_future<std::vector<int>>({1, 2}); // a braced list deduces no type
        AssertThat(braced.get().size(), size_t{2});
    }

    TestCase(exceptional_future)
    {
        std::string what;
        try { (void)rpp::exceptional_future<int>(std::runtime_error{"aargh!"s}).get(); }
        catch (const std::exception& e) { what = e.what(); }
        AssertThat(what, "aargh!"s);

        // an exception_ptr from catch (...) names the error, so get() rethrows that error and not the pointer
        std::exception_ptr e = std::make_exception_ptr(std::runtime_error{"kept_msg"});
        AssertThrows((void)rpp::exceptional_future<int>(e).get(), std::runtime_error);
    }

    // counts its live copies, so a case sees when the last owner freed the exception
    struct counted_error : std::runtime_error
    {
        int* alive;
        explicit counted_error(int* counter) : std::runtime_error{"counted_error"}, alive{counter} { ++*alive; }
        counted_error(const counted_error& e) noexcept : std::runtime_error{e}, alive{e.alive} { ++*alive; }
        ~counted_error() override { --*alive; }
    };

    // the live copies after get() rethrew one, while the promise still holds the state
    template<class T> static int exceptions_left_after_get()
    {
        int alive = 0;
        promise<T> p;
        future<T> f = p.get_future();
        p.set_exception(std::make_exception_ptr(counted_error{&alive}));
        AssertThrows((void)f.get(), counted_error);
        return alive;
    }

    // the future frees the exception it took, so a late ~promise() on another thread never does. See BUGS.md C31
    TestCase(get_takes_ownership_of_the_exception)
    {
        AssertThat(exceptions_left_after_get<void>(), 0);
        AssertThat(exceptions_left_after_get<int>(), 0);
    }

    // the worker destroys the task and publishes after its catch block ends. See BUGS.md C32
    TestCase(async_publishes_after_its_catch_block)
    {
        bool insideCatch = true; // a destructor which never runs also fails the case
        auto probe = rpp::make_scope_guard([&] { insideCatch = std::current_exception() != nullptr; });
        future<void> f = rpp::async([probe=std::move(probe)] { throw std::runtime_error{"catch_probe_msg"}; });
        AssertThrows(f.get(), std::runtime_error);
        AssertThat(insideCatch, false);
    }

    // a guard which ends late, so get() returns first when a result publishes before the guard ends
    static auto slow_probe(std::atomic_bool& ended)
    {
        return rpp::make_scope_guard([&ended] { rpp::sleep_ms(5); ended = true; });
    }

    // a step destroys its task before it publishes, so get() never returns while the task lives. See BUGS.md C32
    TestCase(a_step_destroys_its_task_before_it_publishes)
    {
        std::atomic_bool ended = false;
        AssertThat(rpp::async([p=slow_probe(ended)] { return 1; }).get(), 1);
        AssertThat(ended.exchange(false), true);
        rpp::async([p=slow_probe(ended)] {}).get();
        AssertThat(ended.exchange(false), true);
        AssertThrows(rpp::async([p=slow_probe(ended)] { throw std::runtime_error{"probe_msg"}; }).get(), std::runtime_error);
        AssertThat(ended.exchange(false), true);
        AssertThat(rpp::ready_future(1).then([p=slow_probe(ended)](int x) { return x; }).get(), 1);
        AssertThat(ended.load(), true);
    }

    // counts its copies and moves, so a case can pin how often a step builds its task
    struct move_counter
    {
        int* copies;
        int* moves;
        move_counter(int* c, int* m) noexcept : copies{c}, moves{m} {}
        move_counter(const move_counter& o) noexcept : copies{o.copies}, moves{o.moves} { ++*copies; }
        move_counter(move_counter&& o) noexcept : copies{o.copies}, moves{o.moves} { ++*moves; }
        int operator()() const noexcept { return 1; }
        int operator()(int x) const noexcept { return x; }
    };

    // a move_counter which then() can take as an error handler
    struct handler_counter : move_counter
    {
        using move_counter::move_counter;
        int operator()(const std::runtime_error& /*e*/) const { return -1; }
    };

    // a step builds its task in place, so a temporary task moves once, and a named task copies once and moves once
    TestCase(a_step_moves_a_temporary_task_once)
    {
        int copies = 0;
        int moves = 0;
        AssertThat(rpp::async(move_counter{&copies, &moves}).get(), 1);
        AssertThat(rpp::ready_future(2).then(move_counter{&copies, &moves}, handler_counter{&copies, &moves}).get(), 2);
        rpp::ready_future(2).continue_with(move_counter{&copies, &moves});
        AssertThat(rpp::ready_future(2).chain_async(move_counter{&copies, &moves}).get(), 1);
        AssertThat(moves, 5);
        AssertThat(copies, 0);

        move_counter task { &copies, &moves };
        AssertThat(rpp::async(task).get(), 1);
        AssertThat(rpp::ready_future(2).then(task).get(), 2);
        AssertThat(moves, 7);
        AssertThat(copies, 2);
    }

    // the argument lives to the end of the comma expression on the Itanium ABI, so it must hold no reference
    TestCase(set_exception_keeps_no_reference)
    {
        int alive = 0;
        bool caught = false;
        promise<void> p;
        future<void> f = p.get_future();
        future<void> consumer = rpp::async([&] { try { f.get(); } catch (const counted_error&) { caught = true; } });
        std::exception_ptr error = std::make_exception_ptr(counted_error{&alive});
        int aliveAfterConsumer = -1;
        wait_result woke = wait_result::timeout;
        (p.set_exception(std::exchange(error, nullptr)), woke = consumer.wait_for(rpp::seconds(1)), aliveAfterConsumer = alive);
        consumer.get();
        AssertThat(woke, wait_result::finished); // a hang guard, the consumer releases it
        AssertThat(caught, true);
        AssertThat(aliveAfterConsumer, 0);
    }

    // the argument lives to the end of the comma expression on the Itanium ABI, so it must hold no reference
    TestCase(exceptional_future_keeps_no_reference)
    {
        int alive = 0;
        bool caught = false;
        future<void> f;
        future<void> consumer;
        std::exception_ptr error = std::make_exception_ptr(counted_error{&alive});
        int aliveAfterConsumer = -1;
        wait_result woke = wait_result::timeout;
        (f = rpp::exceptional_future<void>(std::exchange(error, nullptr)), consumer = rpp::async([&] {
            try { f.get(); } catch (const counted_error&) { caught = true; }
        }), woke = consumer.wait_for(rpp::seconds(1)), aliveAfterConsumer = alive);
        consumer.get();
        AssertThat(woke, wait_result::finished); // a hang guard, the consumer releases it
        AssertThat(caught, true);
        AssertThat(aliveAfterConsumer, 0);
    }

    TestCase(basic_async_task_chaining)
    {
        std::string received;
        rpp::async([] { return "future string"s; }).then([&](std::string s) { received = s; }).get();
        AssertThat(received, "future string"s);
    }

    TestCase(invalidates_after_get)
    {
        future<std::string> f1 = rpp::async([] { return "future string"s; });
        AssertThat(f1.get(), "future string"s);
        AssertThat(f1.valid(), false);
    }

    TestCase(invalidates_after_get_void)
    {
        future<void> f1 = rpp::async([] { });
        f1.get();
        AssertThat(f1.valid(), false);
    }

    // a ready future which nobody collected was not abandoned, so its destructor or a move assignment collects it
    TestCase(destructor_collects_a_ready_future)
    {
        { future<int> value = rpp::ready_future(42); }
        { future<void> nothing = rpp::ready_future(); }
        {
            future<int> waited = rpp::async([] { return 7; });
            waited.wait(); // the result is ready, and get() never runs
        }
        future<int> f = rpp::ready_future(1);
        f = rpp::ready_future(2);
        AssertThat(f.get(), 2);
    }

    // rpp::future has no deferred state, so a timed wait reports timeout until the promise publishes
    TestCase(wait_for_reports_timeout_until_the_result_arrives)
    {
        promise<int> p;
        future<int> f = p.get_future();
        AssertThat(f.await_ready(), false);
        AssertThat(f.wait_for(rpp::Duration::zero()), wait_result::timeout);
        AssertThat(f.wait_until(rpp::TimePoint::monotonic_now()), wait_result::timeout);

        p.set_value(42);
        AssertThat(f.await_ready(), true);
        AssertThat(f.wait_for(rpp::Duration::zero()), wait_result::finished);
        AssertThat(f.wait_until(rpp::TimePoint::monotonic_now()), wait_result::finished);
        AssertThat(f.get(), 42);
    }

    TestCase(collect_ready_and_collect_wait)
    {
        promise<int> p;
        future<int> f = p.get_future();
        int result = 0;
        AssertThat(f.collect_ready(&result), false); // the promise has not published
        p.set_value(7);
        AssertThat(f.collect_ready(&result), true);
        AssertThat(result, 7);
        AssertThat(f.collect_wait(&result), false); // collect_ready consumed the state

        future<void> nothing = rpp::ready_future();
        AssertThat(nothing.collect_wait(), true);
        AssertThat(nothing.valid(), false);
    }

    TestCase(a_promise_destroyed_without_a_result_breaks_its_future)
    {
        future<int> f;
        {
            promise<int> p;
            f = p.get_future();
        }
        AssertThrows((void)f.get(), std::logic_error);
    }

    // std::promise gives its future after set_value(), and a port from cpromise relies on that order
    TestCase(a_promise_gives_one_future_and_publishes_once)
    {
        promise<int> p;
        p.set_value(1);
        future<int> f = p.get_future();
        AssertThat(f.await_ready(), true);
        AssertThrows((void)p.get_future(), std::logic_error);
        AssertThrows(p.set_value(2), std::logic_error);
        AssertThrows(p.set_exception(std::make_exception_ptr(std::runtime_error{"late"})), std::logic_error);
        AssertThat(f.get(), 1);
    }

    // a value whose constructor holds its publisher, so a second publisher arrives while the first one stores
    struct held_value
    {
        int value;
        held_value(int v, rpp::semaphore& entered, rpp::semaphore& release) : value{v}
        {
            entered.notify();
            (void)release.wait(rpp::seconds(1)); // a hang guard, the case releases it
        }
    };

    // std::promise throws on a second publish, so two racing publishers never both store a result
    TestCase(a_second_publisher_throws_while_the_first_one_stores)
    {
        promise<held_value> p;
        future<held_value> f = p.get_future();
        rpp::semaphore entered;
        rpp::semaphore release;
        future<void> first = rpp::async([&] { p.set_value(1, entered, release); });
        AssertThat(entered.wait(rpp::seconds(1)), rpp::semaphore::notified); // a hang guard, the first publisher releases it
        AssertThrows(p.set_exception(std::make_exception_ptr(std::runtime_error{"second_publisher_msg"})), std::logic_error);
        release.notify();
        first.get();
        AssertThat(f.get().value, 1);
    }

    // a result whose move throws, so its store fails after the step claimed the promise
    struct throws_on_move
    {
        throws_on_move() = default;
        throws_on_move(throws_on_move&& /*moved*/) { throw std::runtime_error{"throws_on_move_msg"}; }
    };

    // a store which throws frees the claim, so the step still publishes the exception
    TestCase(a_result_which_throws_on_its_move_publishes_the_throw)
    {
        AssertThrows((void)rpp::async([] { return throws_on_move{}; }).get(), std::runtime_error);
    }

    // ready_future() publishes before it gives its future, so a move which throws leaves no future to terminate on
    TestCase(ready_future_rethrows_a_move_which_throws)
    {
        AssertThrows((void)rpp::ready_future(throws_on_move{}), std::runtime_error);
    }

    // std::vector::erase() needs the move assignment, which breaks the promise it replaces
    TestCase(a_promise_moves_like_std_promise)
    {
        std::vector<promise<int>> promises(2);
        future<int> first = promises[0].get_future();
        promises.erase(promises.begin());
        AssertThrows((void)first.get(), std::logic_error);

        future<int> second = promises[0].get_future();
        promises[0].set_value(5);
        AssertThat(second.get(), 5);
    }

    // std::optional held a const value, and the holder which replaced it keeps that
    TestCase(a_promise_of_a_const_value)
    {
        promise<const int> p;
        future<const int> f = p.get_future();
        p.set_value(3);
        AssertThat(f.get(), 3);
    }

    // a runner publishes through its copy, and the caller takes the future from its own copy later
    TestCase(a_promise_copy_shares_its_state)
    {
        promise<int> caller;
        rpp::async([runner=caller]() mutable { runner.set_value(7); }).get();
        future<int> f = caller.get_future();
        promise<int> late = caller;
        AssertThrows((void)late.get_future(), std::logic_error);
        AssertThrows(late.set_value(8), std::logic_error);
        AssertThat(f.get(), 7);

        future<int> broken;
        {
            promise<int> first;
            broken = first.get_future();
            promise<int> second;
            second = first;
            first = promise<int>{};
            AssertThat(broken.await_ready(), false); // `second` still shares the state
        }
        AssertThrows((void)broken.get(), std::logic_error);
    }

    // copies which end together on pool threads break the future exactly once
    TestCase(the_last_promise_copy_breaks_the_future)
    {
        for (int round = 0; round < 100; ++round)
        {
            promise<int> p;
            future<int> f = p.get_future();
            std::atomic_bool go { false };
            std::vector<future<void>> ends;
            for (int i = 0; i < 4; ++i)
                ends.push_back(rpp::async([&go, copy=p] { while (!go) rpp::yield(); })); // the copy ends with the task
            p = promise<int>{};
            go = true;
            for (future<void>& end : ends) end.get();
            if (f.wait_for(rpp::seconds(1)) != rpp::wait_result::finished) // a hang guard, the last copy publishes
            {
                f.detach(); // the case fails, and the destructor does not terminate on it
                AssertFailed("no promise copy published in round %d", round);
                break;
            }
            AssertThrows((void)f.get(), std::logic_error);
        }
    }

    TestCase(detach_abandons_an_unready_future)
    {
        promise<std::string> p;
        future<std::string> f = p.get_future();
        f.detach();
        AssertThat(f.valid(), false);
        p.set_value("nobody reads this"); // the promise frees the state, and a leak fails the ASAN build
    }

    TestCase(then_downcasts_to_void)
    {
        std::atomic_bool ran = false;
        future<void> done = rpp::async([&] { ran = true; return 42; }).then();
        done.get();
        AssertThat(ran.load(), true);

        future<void> same = rpp::async([] {}).then(); // a future<void> gives its own state
        same.get();
    }

    TestCase(then_waits_for_the_next_future)
    {
        promise<void> first;
        future<std::string> chained = first.get_future().then(rpp::async([] { return "next"s; }));
        first.set_value();
        AssertThat(chained.get(), "next"s);
    }

    // the chain fails with the first error, so it abandons `next` and does not terminate on it
    TestCase(then_abandons_the_next_future_when_this_one_fails)
    {
        promise<int> next;
        future<int> chained = rpp::exceptional_future<void>(std::domain_error{"first_msg"}).then(next.get_future());
        AssertThrows((void)chained.get(), std::domain_error);
        next.set_value(1); // nobody reads this, and a leak fails the ASAN build
    }

    TestCase(continue_with_passes_the_result)
    {
        rpp::semaphore done;
        std::atomic_int received = 0;
        rpp::async([] { return 42; }).continue_with([&](int x) {
            received = x;
            done.notify();
        });
        AssertThat(done.wait(rpp::seconds(1)), rpp::semaphore::notified); // a hang guard, the task releases it
        AssertThat(received.load(), 42);
    }

    TestCase(continue_with_handler_recovers)
    {
        rpp::semaphore done;
        std::string handled;
        rpp::async([] { throw std::runtime_error("continue_with_exception_msg"); }).continue_with([&] {
            done.notify();
        }, [&](const std::runtime_error& e) {
            handled = e.what();
            done.notify();
        });
        AssertThat(done.wait(rpp::seconds(1)), rpp::semaphore::notified); // a hang guard, the handler releases it
        AssertThat(handled, "continue_with_exception_msg"s);
    }

    // @returns true when the failed task logs a warning which contains `marker` and names continue_with()
    template<class Task> static bool continue_with_warns(Task failing, const char* marker)
    {
        warning_capture warning { marker };
        future<void> failed = rpp::async(std::move(failing));
        failed.continue_with([] {}, [](const std::invalid_argument&) {}); // this handler takes another type
        return warning.wait("continue_with()");
    }

    // nobody awaits the step of continue_with(), so a failed task logs a warning and does not stop the program
    TestCase(continue_with_logs_an_error_which_no_handler_takes)
    {
        auto failing = [] { throw std::domain_error{"continue_with_unhandled_msg"}; };
        AssertThat(continue_with_warns(failing, "continue_with_unhandled_msg"), true);
        AssertThat(continue_with_warns([] { throw 42; }, "of an unknown type"), true);
    }

    // holds a task until the case attached the steps after it, and records the worker which ran the task
    struct task_gate
    {
        rpp::semaphore gate;
        rpp::semaphore::wait_result opened = rpp::semaphore::timeout;
        rpp::uint64 worker = 0;
        void hold()
        {
            opened = gate.wait(rpp::seconds(1)); // a hang guard, open() releases it
            worker = rpp::get_thread_id();
        }
        void open() { gate.notify(); }
    };

    // the worker which publishes runs the next step inline, so no pool thread waits for a result
    TestCase(a_chain_runs_on_the_worker_which_ran_the_first_task)
    {
        task_gate first;
        rpp::uint64 second = 0;
        rpp::uint64 third = 0;
        future<int> f = rpp::async([&] { first.hold(); return 1; });
        f = f.then([&](int x) { second = rpp::get_thread_id(); return x + 1; });
        f = f.then([&](int x) { third = rpp::get_thread_id(); return x + 1; });
        first.open();
        AssertThat(f.get(), 3);
        AssertThat(first.opened, rpp::semaphore::notified);
        AssertThat(second, first.worker);
        AssertThat(third, first.worker);
    }

    // chain_async() attaches the next task, so the worker which ran the first task also runs the second
    TestCase(chain_async_runs_the_next_task_on_the_worker_which_ran_the_first)
    {
        task_gate first;
        rpp::uint64 second = 0;
        future<void> tasks;
        tasks.chain_async([&] { first.hold(); }).chain_async([&] { second = rpp::get_thread_id(); });
        first.open();
        tasks.get();
        AssertThat(first.opened, rpp::semaphore::notified);
        AssertThat(second, first.worker);
    }

    // then(future&&) forwards a result which is already out on the same worker, so no thread waits for either future
    TestCase(then_a_ready_future_runs_on_the_worker_which_ran_the_first_task)
    {
        task_gate first;
        rpp::uint64 after = 0;
        future<int> f = rpp::async([&] { first.hold(); }).then(rpp::ready_future(2)).then([&](int x) {
            after = rpp::get_thread_id();
            return x;
        });
        first.open();
        AssertThat(f.get(), 2);
        AssertThat(first.opened, rpp::semaphore::notified);
        AssertThat(after, first.worker);
    }

    // the address of this frame, which moves down the stack while one step runs inside another
    static NOINLINE std::uintptr_t frame_address() noexcept
    {
    #if _MSC_VER
        return reinterpret_cast<std::uintptr_t>(_AddressOfReturnAddress());
    #else
        return reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0));
    #endif
    }

    // each step publishes inside the step before it, so a long chain must unwind its stack before the stack grows deep
    TestCase(a_long_chain_keeps_the_stack_of_each_thread_short)
    {
        constexpr int steps = 4000;
        std::map<rpp::uint64, std::pair<std::uintptr_t, std::uintptr_t>> spans; // the lowest and highest frame on each thread
        task_gate first;
        future<int> f = rpp::async([&] { first.hold(); return 0; });
        for (int i = 0; i < steps; ++i)
        {
            f = f.then([&](int x) {
                std::uintptr_t frame = frame_address();
                auto span = spans.try_emplace(rpp::get_thread_id(), frame, frame).first;
                span->second.first = rpp::min(span->second.first, frame);
                span->second.second = rpp::max(span->second.second, frame);
                return x + 1;
            });
        }
        first.open();
        AssertThat(f.get(), steps);
        AssertThat(first.opened, rpp::semaphore::notified);

        std::uintptr_t deepest = 0;
        for (const auto& [thread, span] : spans)
            deepest = rpp::max(deepest, span.second - span.first);
        AssertLess(deepest, std::uintptr_t{256 * 1024});
        AssertThat(spans.size(), size_t{1}); // the pool task runs a postponed step itself, so the chain stays on its worker
    }

    // a promise of the caller may publish under a lock, so its next step starts as a pool task and never inline
    TestCase(a_user_promise_starts_the_next_step_as_a_pool_task)
    {
        rpp::uint64 publisher = 0;
        rpp::uint64 ranOn = 0;
        future<int> next;
        rpp::async([&] {
            promise<int> p;
            next = p.get_future().then([&](int x) { ranOn = rpp::get_thread_id(); return x; });
            publisher = rpp::get_thread_id();
            p.set_value(1);
        }).get();
        AssertThat(next.get(), 1);
        AssertNotEqual(ranOn, publisher);
    }

    TestCase(wait_all_and_get_all)
    {
        std::vector<future<int>> ints;
        ints.push_back(rpp::async([] { return 1; }));
        ints.push_back(rpp::ready_future(2));
        wait_all(ints);
        std::vector<int> expected { 1, 2 };
        AssertThat(get_all(ints), expected);

        // a future which stays valid in the vector would terminate in its destructor, so get_all() collects every one
        ints.clear();
        ints.push_back(rpp::exceptional_future<int>(std::runtime_error{"first_msg"}));
        ints.push_back(rpp::async([] { return 2; }));
        ints.push_back(rpp::exceptional_future<int>(std::domain_error{"second_msg"}));
        AssertThrows((void)get_all(ints), std::runtime_error);
        AssertThat(ints[1].valid(), false);
        AssertThat(ints[2].valid(), false);

        std::vector<future<void>> nothing;
        nothing.push_back(rpp::async([] {}));
        nothing.push_back(rpp::ready_future());
        get_all(nothing);
        AssertThat(nothing[0].valid(), false);

        nothing.clear();
        nothing.push_back(rpp::exceptional_future<void>(std::runtime_error{"first_msg"}));
        nothing.push_back(rpp::exceptional_future<void>(std::domain_error{"second_msg"}));
        AssertThrows(get_all(nothing), std::runtime_error);
        AssertThat(nothing[1].valid(), false);
    }

    static future<int> twice_async(int x)
    {
        int value = co_await rpp::async([x] { return x; });
        co_return value * 2;
    }

    static future<void> throw_after_await()
    {
        co_await rpp::async([] {});
        throw std::runtime_error("coroutine_exception_msg");
    }

    static future<int> await_an_invalid_future()
    {
        future<int> invalid;
        int value = co_await invalid;
        co_return value;
    }

    TestCase(coroutine_returns_its_value_or_rethrows)
    {
        AssertThat(twice_async(21).get(), 42);
        AssertThrows(throw_after_await().get(), std::runtime_error);
        AssertThrows((void)await_an_invalid_future().get(), std::logic_error);
    }

    static future<int> coro_with_a_local(std::atomic_bool* localDestroyed)
    {
        auto local = slow_probe(*localDestroyed);
        int value = co_await rpp::async([] { return 21; });
        co_return value * 2;
    }

    TestCase(coroutine_destroys_its_locals_before_the_result_publishes)
    {
        std::atomic_bool localDestroyed = false;
        future<int> f = coro_with_a_local(&localDestroyed);
        AssertThat(f.get(), 42);
        AssertThat(localDestroyed.load(), true);
    }

    static future<rpp::uint64> thread_after_await(future<int> f)
    {
        (void)co_await f;
        co_return rpp::get_thread_id();
    }

    // the worker which publishes resumes the coroutine, so no pool thread waits for the result
    TestCase(a_coroutine_resumes_on_the_worker_which_published)
    {
        task_gate first;
        future<rpp::uint64> resumed = thread_after_await(rpp::async([&] { first.hold(); return 1; }));
        first.open();
        rpp::uint64 resumedOn = resumed.get();
        AssertThat(first.opened, rpp::semaphore::notified);
        AssertThat(resumedOn, first.worker);
    }

    template<class OnExit> static future<int> await_with_a_parameter(future<int> f, OnExit /*onExit*/)
    {
        co_return co_await f;
    }

    // the frame still holds its parameters when the result publishes, so the next step runs as a pool task outside it
    TestCase(the_step_after_a_coroutine_never_runs_inside_its_frame)
    {
        task_gate first;
        rpp::semaphore parameterEnded;
        rpp::semaphore::wait_result ended = rpp::semaphore::timeout;
        rpp::uint64 ranOn = 0;
        auto onExit = rpp::make_scope_guard([&] { parameterEnded.notify(); });
        future<int> f = await_with_a_parameter(rpp::async([&] { first.hold(); return 1; }), std::move(onExit)).then([&](int x) {
            ranOn = rpp::get_thread_id();
            ended = parameterEnded.wait(rpp::seconds(1)); // a hang guard, the frame releases it after the result published
            return x;
        });
        first.open();
        AssertThat(f.get(), 1);
        AssertThat(first.opened, rpp::semaphore::notified);
        AssertThat(ended, rpp::semaphore::notified);
        AssertNotEqual(ranOn, first.worker);
    }

    TestCase(then_and_continue_with_on_a_loop_run_the_task_on_the_loop_thread)
    {
        rpp::event_loop loop;
        rpp::uint64 ranOn = 0;
        future<int> f = rpp::async([] { return 20; }).then(loop, [&](int x) {
            ranOn = rpp::get_thread_id();
            return x + 1;
        });
        AssertThat(loop.pump_until_ready(f, rpp::seconds(1)), true);
        AssertThat(f.get(), 21);
        AssertThat(ranOn, rpp::get_thread_id());

        ranOn = 0;
        promise<void> ran;
        future<void> done = ran.get_future();
        rpp::async([] { return 42; }).continue_with(loop, [&](int x) {
            ranOn = x == 42 ? rpp::get_thread_id() : 0;
            ran.set_value();
        });
        AssertThat(loop.pump_until_ready(done, rpp::seconds(1)), true);
        AssertThat(ranOn, rpp::get_thread_id());
    }

    // a step on the thread of an event loop starts the next step as a pool task, so a slow step never holds the loop
    TestCase(the_step_after_a_loop_step_runs_on_the_pool)
    {
        rpp::uint64 loopThread = 0;
        rpp::uint64 ranOn = 0;
        bool ran = rpp::async([&] {
            rpp::event_loop loop; // runs inside a pool step, where a step promise may run the next step inline
            loopThread = rpp::get_thread_id();
            future<int> f = rpp::async([] { return 1; }).then(loop, [](int x) { return x; }).then([&](int x) {
                ranOn = rpp::get_thread_id();
                return x;
            });
            return loop.run_until_ready(f, rpp::seconds(1)) == 1;
        }).get();
        AssertThat(ran, true); // a hang guard, the loop runs the first step
        AssertNotEqual(ranOn, loopThread);
    }

    // an event loop which drops every delegate, as a loop which stopped does
    struct dropping_loop
    {
        static void post(rpp::delegate<void()>&& callback) noexcept { rpp::delegate<void()> dropped { std::move(callback) }; }
    };

    // the posted delegate owns the step, so a loop which drops it ends the promise of the step at once
    static void drop_the_step_after(future<int> input)
    {
        dropping_loop loop;
        future<int> f = input.then(loop, [](int x) { return x; });
        bool ended = f.await_ready();
        if (ended) AssertThrows((void)f.get(), std::logic_error);
        else f.detach(); // the case fails below, and the destructor does not terminate on it
        AssertThat(ended, true);
    }

    TestCase(a_loop_which_drops_the_step_ends_its_promise)
    {
        drop_the_step_after(rpp::ready_future(1));
        // a dropped step detaches its input, so an error which nobody read fails no assertion
        drop_the_step_after(rpp::exceptional_future<int>(std::runtime_error{"dropped_input_msg"}));
    }

    // posts a step to a loop which drops it. @returns the thread which ran the step after it, or 0 when none ran in time
    static rpp::uint64 thread_after_a_dropped_step()
    {
        dropping_loop loop;
        promise<int> p;
        future<int> dropped = p.get_future().then(loop, [](int y) { return y; });
        auto recover = [](std::logic_error&) { return get_thread_id(); }; // the dropped step leaves a std::logic_error
        future<rpp::uint64> after = dropped.then([](int) -> rpp::uint64 { return 0; }, recover);
        p.set_value(1);
        // a hang guard, which the worker that runs the step after the dropped one releases
        if (after.wait_for(rpp::seconds(1)) == wait_result::finished) return after.get();
        after.detach();
        return 0;
    }

    // a dropped step ends inside the body which posted it, so the step after it starts as a pool task, at every depth
    TestCase(a_body_can_wait_for_the_step_after_a_dropped_loop_step)
    {
        constexpr int steps = 64; // longer than the depth cap, so one body runs at the cap
        std::atomic_int failures = 0; // the step after the dropped one hung, or ran inside the body
        task_gate first;
        future<int> f = rpp::async([&] { first.hold(); return 0; });
        for (int i = 0; i < steps; ++i)
        {
            f = f.then([&](int x) {
                rpp::uint64 ranOn = thread_after_a_dropped_step();
                if (ranOn == 0 || ranOn == rpp::get_thread_id()) ++failures;
                return x + 1;
            });
        }
        first.open();
        AssertThat(f.get(), steps);
        AssertThat(first.opened, rpp::semaphore::notified);
        AssertThat(failures.load(), 0);
    }
};

// NOLINTEND(performance-*)
