#include <rpp/async.h>
#include <rpp/semaphore.h>
#include <rpp/timepoint.h>
#include <rpp/tests.h>
#include <atomic>
#include <stdexcept>
#include <string> // std::string
#include <vector>
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
        loadString.set_value("future string"); // the continuation waits on the pool until this line
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
    // and returns. Only an unready future is a fatal error, so this case fails by terminating
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
};

// NOLINTEND(performance-*)
