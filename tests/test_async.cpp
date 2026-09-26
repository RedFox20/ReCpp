#include <rpp/async.h>
#include <rpp/delegate.h> // rpp::delegate, which an event loop runs
#include <rpp/event_loop.h>
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
    TestInit(test_async)
    {
    }

    TestCase(simple_chaining)
    {
        promise<std::string> loadString;
        future<std::string> futureString = loadString.get_future();
        future<bool> chain = futureString.then([](std::string arg) -> bool
        {
            return !arg.empty() && arg == "future string";
        });
        loadString.set_value("future string"); // this line starts the continuation as a pool task
        AssertThat(chain.get(), true);
    }

    TestCase(chain_mutate_void_to_string)
    {
        promise<void> loadSomething;
        future<void> futureSomething = loadSomething.get_future();
        future<std::string> chained = futureSomething.then([]() -> std::string
        {
            return "operation complete!"s;
        });
        loadSomething.set_value();
        AssertThat(chained.get(), "operation complete!"s);
    }

    TestCase(chain_decay_string_to_void)
    {
        promise<std::string> loadString;
        future<std::string> futureString = loadString.get_future();
        std::string received;
        future<void> chained = futureString.then([&](std::string s)
        {
            received = s;
        });
        loadString.set_value("some string");
        chained.get();
        AssertThat(received, "some string"s);
    }

    TestCase(cross_thread_exception_propagation)
    {
        future<void> asyncThrowingTask = rpp::async([] {
            throw std::runtime_error("background_thread_exception_msg");
        });

        std::string result;
        try { asyncThrowingTask.get(); }
        catch (const std::runtime_error& e) { result = e.what(); }
        AssertThat(result, "background_thread_exception_msg"s);
    }

    TestCase(composable_future_type)
    {
        future<std::string> f = rpp::async([] {
            return "future string"s;
        });

        int totalCalls = 0;
        std::string first;
        int second = 0;
        f.then([&](std::string s) {
            ++totalCalls;
            first = s;
            return 42;
        }).then([&](int x) {
            ++totalCalls;
            second = x;
        }).get();
        AssertThat(totalCalls, 2);
        AssertThat(first, "future string"s);
        AssertThat(second, 42);
    }

    TestCase(except_handler)
    {
        future<void> f = rpp::async([] {
            throw std::runtime_error("background_thread_exception_msg");
        });

        bool taskCalled = false;
        std::string handled;
        int result = f.then([&] {
            taskCalled = true;
            return 0;
        }, [&](const std::exception& e) {
            handled = e.what();
            return 42;
        }).get();

        AssertThat(taskCalled, false);
        AssertThat(handled, "background_thread_exception_msg"s);
        AssertThat(result, 42);
    }

    TestCase(except_handlers_catch_first)
    {
        future<void> f = rpp::async([] {
            throw std::domain_error("background_thread_exception_msg");
        });

        std::string handled;
        int result = f.then([] {
            return 0;
        },
        [&](std::domain_error e) {
            handled = e.what();
            return 42;
        },
        [](std::runtime_error e) {
            (void)e;
            return 21;
        }).get();

        AssertThat(handled, "background_thread_exception_msg"s);
        AssertThat(result, 42);
    }

    TestCase(except_handlers_catch_second)
    {
        future<void> f = rpp::async([] {
            throw std::runtime_error("background_thread_exception_msg");
        });

        std::string handled;
        int result = f.then([] {
            return 0;
        },
        [](std::domain_error e) {
            (void)e;
            return 21;
        },
        [&](std::runtime_error e) {
            handled = e.what();
            return 42;
        }).get();

        AssertThat(handled, "background_thread_exception_msg"s);
        AssertThat(result, 42);
    }

    struct SpecificError : std::range_error
    {
        using std::range_error::range_error;
    };

    TestCase(except_handlers_catch_third)
    {
        future<void> f = rpp::async([] {
            throw std::runtime_error("background_thread_exception_msg");
        });

        std::string handled;
        int result = f.then([] {
            return 0;
        },
        [](const SpecificError& e) {
            (void)e;
            return 1;
        },
        [](const std::range_error& e) {
            (void)e;
            return 2;
        },
        [&](const std::runtime_error& e) {
            handled = e.what();
            return 3;
        }).get();

        AssertThat(handled, "background_thread_exception_msg"s);
        AssertThat(result, 3);
    }

    // the handlers read like a chain of catch blocks, so the first match wins and not the most specific one
    TestCase(except_handlers_the_first_match_wins)
    {
        future<void> f = rpp::async([] {
            throw SpecificError("specific_error_msg");
        });

        int result = f.then([] { return 0; },
                            [](const std::range_error&) { return 1; },
                            [](const SpecificError&) { return 2; }).get();
        AssertThat(result, 1);
    }

    // a catch block never catches what a sibling catch block throws, so neither does a later handler
    TestCase(a_handler_which_throws_skips_the_later_handlers)
    {
        future<void> f = rpp::async([] {
            throw std::domain_error("background_thread_exception_msg");
        });

        future<int> chained = f.then([] { return 0; },
                                     [](const std::domain_error&) -> int { throw std::runtime_error("handler_msg"); },
                                     [](const std::runtime_error&) { return 2; });
        AssertThrows((void)chained.get(), std::runtime_error);
    }

    TestCase(except_handler_chaining)
    {
        future<std::string> f = rpp::async([] {
            return "future string"s;
        });

        std::string handled;
        int result = f.then([](std::string s) {
            (void)s;
            throw std::runtime_error("future_continuation_exception_msg");
            return 0;
        }).then([](int x) {
            (void)x;
            return 5;
        }, [&](const std::exception& e) {
            handled = e.what();
            return 42;
        }).get();

        AssertThat(handled, "future_continuation_exception_msg"s);
        AssertThat(result, 42);
    }

    TestCase(chain_async_futures_void)
    {
        bool task1Called = false;
        bool task2Called = false;
        bool task3Called = false;

        future<void> tasks;
        tasks.chain_async(
            [&]{ task1Called = true; }
        ).chain_async(
            [&]{ task2Called = true; throw std::runtime_error("task2 failed"); }
        ).chain_async(
            [&]{ task3Called = true; }
        );
        tasks.get(); // waits for task3, which ran after task1 and task2

        AssertThat(task1Called, true);
        AssertThat(task2Called, true);
        AssertThat(task3Called, true);
    }

    TestCase(chain_async_futures_T)
    {
        bool task1Called = false;
        bool task2Called = false;
        bool task3Called = false;

        future<std::string> tasks;
        tasks.chain_async(
            [&]() -> std::string { task1Called = true; return "task1"; }
        ).chain_async(
            [&]() -> std::string { task2Called = true; throw std::runtime_error("task2 failed"); }
        ).chain_async(
            [&]() -> std::string { task3Called = true; return "task3"; }
        );
        std::string result = tasks.get(); // waits for task3, which ran after task1 and task2

        AssertThat(task1Called, true);
        AssertThat(task2Called, true);
        AssertThat(task3Called, true);
        AssertThat(result, "task3"s);
    }

    TestCase(chain_async_futures_continue_completed)
    {
        bool task1Called = false;
        bool task2Called = false;
        bool task3Called = false;

        future<void> tasks;
        tasks.chain_async(
            [&]{ task1Called = true; }
        ).chain_async(
            [&]{ task2Called = true; }
        );
        tasks.wait(); // waits for task2, which ran after task1

        AssertThat(task1Called, true);
        AssertThat(task2Called, true);

        tasks.chain_async(
            [&]{ task3Called = true; }
        );
        tasks.get();
        AssertThat(task3Called, true);
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
    }

    TestCase(exceptional_future)
    {
        bool exceptionWasThrown = false;
        try
        {
            future<int> f = rpp::exceptional_future<int>(std::runtime_error{"aargh!"s});
            (void)f.get();
        }
        catch (const std::exception& e)
        {
            exceptionWasThrown = true;
            AssertThat(e.what(), "aargh!"s);
        }
        AssertThat(exceptionWasThrown, true);
    }

    // an exception_ptr from catch (...) names the error, so get() rethrows that error and not the pointer
    TestCase(exceptional_future_keeps_an_exception_ptr)
    {
        std::exception_ptr e = std::make_exception_ptr(std::runtime_error{"kept_msg"});
        future<int> f = rpp::exceptional_future<int>(e);
        AssertThrows((void)f.get(), std::runtime_error);
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

    // records whether the last owner of the task destroyed it inside a catch block
    struct catch_probe
    {
        bool* insideCatch;
        explicit catch_probe(bool* inside) noexcept : insideCatch{inside} {}
        catch_probe(catch_probe&& p) noexcept : insideCatch{std::exchange(p.insideCatch, nullptr)} {}
        ~catch_probe() { if (insideCatch) *insideCatch = std::current_exception() != nullptr; }
    };

    // the worker destroys the task and publishes after its catch block ends. See BUGS.md C32
    TestCase(async_publishes_after_its_catch_block)
    {
        bool insideCatch = true; // a destructor which never runs also fails the case
        future<void> f = rpp::async([probe=catch_probe{&insideCatch}] { throw std::runtime_error{"catch_probe_msg"}; });
        AssertThrows(f.get(), std::runtime_error);
        AssertThat(insideCatch, false);
    }

    // its destructor ends late, so get() returns first when a step publishes before it destroys its task
    struct slow_probe
    {
        std::atomic_bool* ended;
        explicit slow_probe(std::atomic_bool* e) noexcept : ended{e} {}
        slow_probe(slow_probe&& p) noexcept : ended{std::exchange(p.ended, nullptr)} {}
        ~slow_probe()
        {
            if (ended)
            {
                rpp::sleep_ms(5);
                *ended = true;
            }
        }
    };

    // a step destroys its task before it publishes, so get() never returns while the task lives. See BUGS.md C32
    TestCase(a_step_destroys_its_task_before_it_publishes)
    {
        std::atomic_bool ended = false;
        AssertThat(rpp::async([p=slow_probe{&ended}] { return 1; }).get(), 1);
        AssertThat(ended.exchange(false), true);
        rpp::async([p=slow_probe{&ended}] {}).get();
        AssertThat(ended.exchange(false), true);
        AssertThrows(rpp::async([p=slow_probe{&ended}] { throw std::runtime_error{"probe_msg"}; }).get(), std::runtime_error);
        AssertThat(ended.exchange(false), true);
        AssertThat(rpp::ready_future(1).then([p=slow_probe{&ended}](int x) { return x; }).get(), 1);
        AssertThat(ended.load(), true);
    }

    // the argument lives to the end of the comma expression on the Itanium ABI, so it must hold no reference
    TestCase(set_exception_keeps_no_reference)
    {
        int alive = 0;
        bool caught = false;
        promise<void> p;
        future<void> f = p.get_future();
        rpp::semaphore consumed;
        future<void> consumer = rpp::async([&] {
            try { f.get(); } catch (const counted_error&) { caught = true; }
            consumed.notify();
        });
        std::exception_ptr error = std::make_exception_ptr(counted_error{&alive});
        int aliveAfterConsumer = -1;
        rpp::semaphore::wait_result woke = rpp::semaphore::timeout;
        (p.set_exception(std::exchange(error, nullptr)), woke = consumed.wait(rpp::seconds(1)), aliveAfterConsumer = alive);
        consumer.get();
        AssertThat(woke, rpp::semaphore::notified); // a hang guard, the consumer releases it
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
        rpp::semaphore consumed;
        std::exception_ptr error = std::make_exception_ptr(counted_error{&alive});
        int aliveAfterConsumer = -1;
        rpp::semaphore::wait_result woke = rpp::semaphore::timeout;
        (f = rpp::exceptional_future<void>(std::exchange(error, nullptr)), consumer = rpp::async([&] {
            try { f.get(); } catch (const counted_error&) { caught = true; }
            consumed.notify();
        }), woke = consumed.wait(rpp::seconds(1)), aliveAfterConsumer = alive);
        consumer.get();
        AssertThat(woke, rpp::semaphore::notified); // a hang guard, the consumer releases it
        AssertThat(caught, true);
        AssertThat(aliveAfterConsumer, 0);
    }

    TestCase(basic_async_task)
    {
        future<std::string> f = rpp::async([] {
            return "future string"s;
        });
        AssertThat(f.get(), "future string"s);
    }

    TestCase(basic_async_task_chaining)
    {
        future<std::string> f = rpp::async([] {
            return "future string"s;
        });

        std::string received;
        f.then([&](std::string s) {
            received = s;
        }).get();

        AssertThat(received, "future string"s);
    }

    TestCase(invalidates_after_get)
    {
        future<std::string> f1 = rpp::async([] {
            return "future string"s;
        });
        AssertThat(f1.get(), "future string"s);
        AssertThat(f1.valid(), false);
    }

    TestCase(invalidates_after_get_void)
    {
        future<void> f1 = rpp::async([] { });
        f1.get();
        AssertThat(f1.valid(), false);
    }

    // a ready future which nobody collected was not abandoned, so its destructor collects it
    // and returns. If the destructor terminates on a ready result, the test run stops here
    TestCase(destructor_collects_a_ready_future)
    {
        { future<int> value = rpp::ready_future(42); }
        { future<void> nothing = rpp::ready_future(); }
        {
            future<int> waited = rpp::async([] { return 7; });
            waited.wait(); // the result is ready, and get() never runs
        }
    }

    TestCase(move_assignment_collects_a_ready_future)
    {
        future<int> f = rpp::ready_future(1);
        f = rpp::ready_future(2); // the first result is ready, so the assignment collects it
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

    TestCase(a_promise_gives_one_future_and_publishes_once)
    {
        promise<int> p;
        future<int> f = p.get_future();
        AssertThrows((void)p.get_future(), std::logic_error);

        p.set_value(1);
        AssertThrows(p.set_value(2), std::logic_error);
        AssertThrows(p.set_exception(std::make_exception_ptr(std::runtime_error{"late"})), std::logic_error);
        AssertThat(f.get(), 1);
    }

    // std::promise gives its future after set_value(), and a port from cpromise relies on that order
    TestCase(a_promise_gives_its_future_after_it_published)
    {
        promise<int> p;
        p.set_value(7);
        future<int> f = p.get_future();
        AssertThat(f.await_ready(), true);
        AssertThat(f.get(), 7);
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

    // the warning which contains `marker`, or an empty string when no warning arrives
    template<class Task> static std::string continue_with_warning(Task failing, const char* marker)
    {
        warning_capture warning { marker };
        future<void> failed = rpp::async(std::move(failing));
        failed.continue_with([] {}, [](const std::invalid_argument&) {}); // this handler takes another type
        return warning.wait();
    }

    // nobody awaits the step of continue_with(), so a failed task logs a warning and does not stop the program
    TestCase(continue_with_logs_an_error_which_no_handler_takes)
    {
        auto failing = [] { throw std::domain_error{"continue_with_unhandled_msg"}; };
        std::string text = continue_with_warning(failing, "continue_with_unhandled_msg");
        AssertThat(text.find("continue_with()") != std::string::npos, true);
    }

    TestCase(continue_with_logs_an_error_of_an_unknown_type)
    {
        std::string text = continue_with_warning([] { throw 42; }, "of an unknown type");
        AssertThat(text.find("continue_with()") != std::string::npos, true);
    }

    // the worker which publishes runs the next step inline, so no pool thread waits for a result
    TestCase(a_chain_runs_on_the_worker_which_ran_the_first_task)
    {
        rpp::semaphore gate;
        rpp::semaphore::wait_result opened = rpp::semaphore::timeout;
        rpp::uint64 first = 0;
        rpp::uint64 second = 0;
        rpp::uint64 third = 0;
        future<int> f = rpp::async([&] {
            opened = gate.wait(rpp::seconds(1)); // holds the task until both steps attached
            first = rpp::get_thread_id();
            return 1;
        }).then([&](int x) {
            second = rpp::get_thread_id();
            return x + 1;
        }).then([&](int x) {
            third = rpp::get_thread_id();
            return x + 1;
        });
        gate.notify();
        AssertThat(f.get(), 3);
        AssertThat(opened, rpp::semaphore::notified); // a hang guard, the test releases it
        AssertThat(second, first);
        AssertThat(third, first);
    }

    // chain_async() attaches the next task, so the worker which ran the first task also runs the second
    TestCase(chain_async_runs_the_next_task_on_the_worker_which_ran_the_first)
    {
        rpp::semaphore gate;
        rpp::semaphore::wait_result opened = rpp::semaphore::timeout;
        rpp::uint64 first = 0;
        rpp::uint64 second = 0;
        future<void> tasks;
        tasks.chain_async([&] {
            opened = gate.wait(rpp::seconds(1)); // holds the task until the second task attached
            first = rpp::get_thread_id();
        }).chain_async([&] { second = rpp::get_thread_id(); });
        gate.notify();
        tasks.get();
        AssertThat(opened, rpp::semaphore::notified); // a hang guard, the test releases it
        AssertThat(second, first);
    }

    // then(future&&) forwards a result which is already out on the same worker, so no thread waits for either future
    TestCase(then_a_ready_future_runs_on_the_worker_which_ran_the_first_task)
    {
        rpp::semaphore gate;
        rpp::semaphore::wait_result opened = rpp::semaphore::timeout;
        rpp::uint64 first = 0;
        rpp::uint64 after = 0;
        future<int> f = rpp::async([&] {
            opened = gate.wait(rpp::seconds(1)); // holds the task until the steps after it attached
            first = rpp::get_thread_id();
        }).then(rpp::ready_future(2)).then([&](int x) {
            after = rpp::get_thread_id();
            return x;
        });
        gate.notify();
        AssertThat(f.get(), 2);
        AssertThat(opened, rpp::semaphore::notified); // a hang guard, the test releases it
        AssertThat(after, first);
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

    // each step publishes inside the step before it, so a long chain must move to a new pool task before the stack grows deep
    TestCase(a_long_chain_keeps_the_stack_of_each_thread_short)
    {
        constexpr int steps = 4000;
        std::map<rpp::uint64, std::pair<std::uintptr_t, std::uintptr_t>> spans; // the lowest and highest frame on each thread
        rpp::semaphore gate;
        rpp::semaphore::wait_result opened = rpp::semaphore::timeout;
        future<int> f = rpp::async([&] {
            opened = gate.wait(rpp::seconds(1)); // holds the task until every step attached
            return 0;
        });
        for (int i = 0; i < steps; ++i)
        {
            f = f.then([&](int x) {
                std::uintptr_t frame = frame_address();
                auto span = spans.try_emplace(rpp::get_thread_id(), frame, frame).first;
                if (frame < span->second.first) span->second.first = frame;
                if (frame > span->second.second) span->second.second = frame;
                return x + 1;
            });
        }
        gate.notify();
        AssertThat(f.get(), steps);
        AssertThat(opened, rpp::semaphore::notified); // a hang guard, the test releases it

        std::uintptr_t deepest = 0;
        for (const auto& [thread, span] : spans)
            if (span.second - span.first > deepest) deepest = span.second - span.first;
        AssertLess(deepest, std::uintptr_t{256 * 1024});
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

        std::vector<future<void>> nothing;
        nothing.push_back(rpp::async([] {}));
        nothing.push_back(rpp::ready_future());
        get_all(nothing);
        AssertThat(nothing[0].valid(), false);
    }

    // a future which stays valid in the vector would terminate in its destructor, so get_all() collects every one
    TestCase(get_all_collects_every_future_before_it_rethrows)
    {
        std::vector<future<int>> ints;
        ints.push_back(rpp::exceptional_future<int>(std::runtime_error{"first_msg"}));
        ints.push_back(rpp::async([] { return 2; }));
        ints.push_back(rpp::exceptional_future<int>(std::domain_error{"second_msg"}));
        AssertThrows((void)get_all(ints), std::runtime_error);
        AssertThat(ints[1].valid(), false);
        AssertThat(ints[2].valid(), false);

        std::vector<future<void>> nothing;
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

    TestCase(coroutine_returns_the_value)
    {
        AssertThat(twice_async(21).get(), 42);
    }

    TestCase(coroutine_rethrows_its_exception)
    {
        AssertThrows(throw_after_await().get(), std::runtime_error);
        AssertThrows((void)await_an_invalid_future().get(), std::logic_error);
    }

    // sets the flag late, so a result which publishes before the runtime destroys the frame reaches get() first
    struct set_on_exit
    {
        std::atomic_bool* flag;
        ~set_on_exit()
        {
            rpp::sleep_ms(5);
            *flag = true;
        }
    };

    static future<int> coro_with_a_local(std::atomic_bool* localDestroyed)
    {
        set_on_exit local { localDestroyed };
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
        rpp::semaphore gate;
        rpp::semaphore::wait_result opened = rpp::semaphore::timeout;
        rpp::uint64 worker = 0;
        future<rpp::uint64> resumed = thread_after_await(rpp::async([&] {
            opened = gate.wait(rpp::seconds(1)); // holds the task until the coroutine suspended
            worker = rpp::get_thread_id();
            return 1;
        }));
        gate.notify();
        rpp::uint64 resumedOn = resumed.get();
        AssertThat(opened, rpp::semaphore::notified); // a hang guard, the test releases it
        AssertThat(resumedOn, worker);
    }

    // notifies `ended` when the coroutine frame destroys its copy. A moved-from copy notifies nothing
    struct notify_on_exit
    {
        rpp::semaphore* ended;
        explicit notify_on_exit(rpp::semaphore* s) noexcept : ended{s} {}
        notify_on_exit(notify_on_exit&& other) noexcept : ended{std::exchange(other.ended, nullptr)} {}
        ~notify_on_exit() { if (ended) ended->notify(); }
    };

    static future<int> await_with_a_parameter(future<int> f, notify_on_exit /*onExit*/)
    {
        co_return co_await f;
    }

    // the frame still holds its parameters when the result publishes, so the next step runs as a pool task outside it
    TestCase(the_step_after_a_coroutine_never_runs_inside_its_frame)
    {
        rpp::semaphore gate;
        rpp::semaphore parameterEnded;
        rpp::semaphore::wait_result opened = rpp::semaphore::timeout;
        rpp::semaphore::wait_result ended = rpp::semaphore::timeout;
        rpp::uint64 worker = 0;
        rpp::uint64 ranOn = 0;
        future<int> f = await_with_a_parameter(rpp::async([&] {
            opened = gate.wait(rpp::seconds(1)); // holds the task until the step after the coroutine attached
            worker = rpp::get_thread_id();
            return 1;
        }), notify_on_exit{&parameterEnded}).then([&](int x) {
            ranOn = rpp::get_thread_id();
            ended = parameterEnded.wait(rpp::seconds(1)); // a hang guard, the frame releases it after the result published
            return x;
        });
        gate.notify();
        AssertThat(f.get(), 1);
        AssertThat(opened, rpp::semaphore::notified); // a hang guard, the test releases it
        AssertThat(ended, rpp::semaphore::notified);
        AssertNotEqual(ranOn, worker);
    }

    // runs `loop` on this thread until `f` holds its result, or until the hang guard ends
    template<class T> static bool run_until_ready(rpp::event_loop& loop, future<T>& f)
    {
        rpp::TimePoint deadline = rpp::TimePoint::monotonic_now() + rpp::seconds(1);
        while (!f.await_ready() && rpp::TimePoint::monotonic_now() < deadline)
            loop.run_once(rpp::millis(5));
        if (f.await_ready()) return true;
        f.detach(); // the case fails on the result, and the destructor does not terminate on it
        return false;
    }

    TestCase(then_on_a_loop_runs_the_task_on_the_loop_thread)
    {
        rpp::event_loop loop;
        rpp::uint64 ranOn = 0;
        future<int> f = rpp::async([] { return 20; }).then(loop, [&](int x) {
            ranOn = rpp::get_thread_id();
            return x + 1;
        });
        AssertThat(run_until_ready(loop, f), true); // a hang guard, the loop runs the task
        AssertThat(f.get(), 21);
        AssertThat(ranOn, rpp::get_thread_id());
    }

    TestCase(continue_with_on_a_loop_runs_the_task_on_the_loop_thread)
    {
        rpp::event_loop loop;
        rpp::uint64 ranOn = 0;
        promise<void> ran;
        future<void> done = ran.get_future();
        rpp::async([] { return 42; }).continue_with(loop, [&](int x) {
            ranOn = x == 42 ? rpp::get_thread_id() : 0;
            ran.set_value();
        });
        AssertThat(run_until_ready(loop, done), true); // a hang guard, the loop runs the task
        AssertThat(ranOn, rpp::get_thread_id());
    }

    // a step on the thread of an event loop starts the next step as a pool task, so a slow step never holds the loop
    TestCase(the_step_after_a_loop_step_runs_on_the_pool)
    {
        rpp::uint64 loopThread = 0;
        rpp::uint64 ranOn = 0;
        bool ran = rpp::async([&] {
            rpp::event_loop loop; // runs inside a pool step, where a promise of the library may run the next step inline
            loopThread = rpp::get_thread_id();
            future<int> f = rpp::async([] { return 1; }).then(loop, [](int x) { return x; }).then([&](int x) {
                ranOn = rpp::get_thread_id();
                return x;
            });
            return run_until_ready(loop, f) && f.get() == 1;
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
    TestCase(a_loop_which_drops_the_step_ends_its_promise)
    {
        dropping_loop loop;
        future<int> f = rpp::ready_future(1).then(loop, [](int x) { return x; });
        bool ended = f.await_ready();
        if (ended) AssertThrows((void)f.get(), std::logic_error);
        else f.detach(); // the case fails below, and the destructor does not terminate on it
        AssertThat(ended, true);
    }
};

// NOLINTEND(performance-*)
