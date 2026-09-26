#include <rpp/future.h>
#include <rpp/tests.h>
#include <stdexcept> // std::domain_error, std::invalid_argument
#include <string> // std::string
#include "warning_capture.h"
using namespace rpp;
using namespace std::chrono_literals;
using namespace std::this_thread;
using namespace std::string_literals;

// NOLINTBEGIN(performance-*)

TestImpl(test_future)
{
    TestInit(test_future)
    {
    }

    TestCase(simple_chaining)
    {
        std::packaged_task<std::string()> loadString{[]{ 
            ::sleep_for(15ms);
            return "future string";
        }};

        // race condition: make sure future is retrieved before starting the task
        cfuture<std::string> futureString = loadString.get_future();

        std::thread{[&]{ loadString(); }}.detach();
        cfuture<bool> chain = futureString.then([](std::string arg) -> bool
        {
            return !arg.empty() && arg == "future string";
        });

        bool chainResult = chain.get();
        AssertThat(chainResult, true);
    }

    TestCase(chain_mutate_void_to_string)
    {
        std::packaged_task<void()> loadSomething{[]{ 
            ::sleep_for(15ms);
        }};
        std::thread{[&]{ loadSomething(); }}.detach();

        cfuture<void> futureSomething = loadSomething.get_future();
        cfuture<std::string> async = futureSomething.then([]() -> std::string
        {
            return "operation complete!"s;
        });

        std::string result = async.get();
        AssertThat(result, "operation complete!");
    }

    TestCase(chain_decay_string_to_void)
    {
        std::packaged_task<std::string()> loadString{[]{ 
            ::sleep_for(15ms);
            return "some string";
        }};

        // race condition: make sure future is retrieved before starting the task
        cfuture<std::string> futureString = loadString.get_future();

        bool continuationCalled = false;
        std::thread{[&]{ loadString(); }}.detach();
        cfuture<void> async = futureString.then([&](std::string s)
        {
            continuationCalled = true;
            AssertThat(s.empty(), false);
        });

        async.get();
        AssertThat(continuationCalled, true);
    }

    TestCase(cross_thread_exception_propagation)
    {
        cfuture<void> asyncThrowingTask = std::async(std::launch::async, [] {
            throw std::runtime_error("background_thread_exception_msg");
        });

        std::string result;
        try
        {
            asyncThrowingTask.get();
        }
        catch (const std::runtime_error& e)
        {
            result = e.what();
        }
        AssertThat(result, "background_thread_exception_msg");
    }

    TestCase(composable_future_type)
    {
        cfuture<std::string> f = std::async(std::launch::async, [] {
            return "future string"s;
        });

        int totalCalls = 0;
        f.then([&](std::string s) {
            ++totalCalls;
            AssertThat(s, "future string");
            return 42;
        }).then([&](int x) {
            ++totalCalls;
            AssertThat(x, 42);
        }).get();
        AssertThat(totalCalls, 2);
    }

    TestCase(except_handler)
    {
        cfuture<void> f = std::async(std::launch::async, [] {
            throw std::runtime_error("background_thread_exception_msg");
        });

        bool exceptHandlerCalled = false;
        int result = f.then([&] {
            AssertMsg(false, "This callback should never be executed");
            return 0;
        }, [&](const std::exception& e) {
           exceptHandlerCalled = true;
           AssertThat(e.what(), "background_thread_exception_msg"s);
           return 42;
        }).get();

        AssertThat(exceptHandlerCalled, true);
        AssertThat(result, 42);
    }

    TestCase(except_handlers_catch_first)
    {
        cfuture<void> f = std::async(std::launch::async, [] {
            throw std::domain_error("background_thread_exception_msg");
        });

        bool exceptHandlerCalled = false;
        int result = f.then([&] {
            AssertMsg(false, "This callback should never be executed");
            return 0;
        },
        [&](std::domain_error e) {
            exceptHandlerCalled = true;
            AssertThat(e.what(), "background_thread_exception_msg"s);
            return 42;
        },
        [&](std::runtime_error e) {
            (void)e;
            return 21;
        }).get();

        AssertThat(exceptHandlerCalled, true);
        AssertThat(result, 42);
    }

    TestCase(except_handlers_catch_second)
    {
        cfuture<void> f = std::async(std::launch::async, [] {
            throw std::runtime_error("background_thread_exception_msg");
        });

        bool exceptHandlerCalled = false;
        int result = f.then([&] {
            AssertMsg(false, "This callback should never be executed");
            return 0;
        },
        [&](std::domain_error e) {
            (void)e;
            return 21;
        },
        [&](std::runtime_error e) {
            exceptHandlerCalled = true;
            AssertThat(e.what(), "background_thread_exception_msg"s);
            return 42;
        }).get();

        AssertThat(exceptHandlerCalled, true);
        AssertThat(result, 42);
    }

    struct SpecificError : std::range_error
    {
        using std::range_error::range_error;
    };

    TestCase(except_handlers_catch_third)
    {
        cfuture<void> f = std::async(std::launch::async, [] {
            throw std::runtime_error("background_thread_exception_msg");
        });

        bool exceptHandlerCalled = false;
        int result = f.then([&] {
            AssertMsg(false, "This callback should never be executed");
            return 0;
        },
        [&](const SpecificError& e) {
            (void)e;
            return 1;
        },
        [&](const std::range_error& e) {
            (void)e;
            return 2;
        },
        [&](const std::runtime_error& e) {
            exceptHandlerCalled = true;
            AssertThat(e.what(), "background_thread_exception_msg"s);
            return 3;
        }).get();

        AssertThat(exceptHandlerCalled, true);
        AssertThat(result, 3);
    }

    TestCase(except_handler_chaining)
    {
        cfuture<std::string> f = std::async(std::launch::async, [] {
            return "future string"s;
        });

        bool secondExceptHandlerCalled = false;
        int result = f.then([](std::string s) {
            (void)s;
            throw std::runtime_error("future_continuation_exception_msg");
            return 0;
        }).then([](int x) {
            (void)x;
            return 5;
        }, [&](const std::exception& e) {
            secondExceptHandlerCalled = true;
            AssertThat(e.what(), "future_continuation_exception_msg"s);
            return 42;
        }).get();

        AssertThat(secondExceptHandlerCalled, true);
        AssertThat(result, 42);
    }

    // nobody awaits the task of continue_with(), so a failed task logs a warning and does not stop the program
    TestCase(continue_with_logs_an_error_which_no_handler_takes)
    {
        warning_capture warning { "cfuture_unhandled_msg" };
        cfuture<int> failed = rpp::async_task([]() -> int { throw std::domain_error{"cfuture_unhandled_msg"}; });
        failed.continue_with([](int) {}, [](const std::invalid_argument&) {}); // this handler takes another type
        AssertThat(warning.wait().find("continue_with()") != std::string::npos, true);

        warning_capture bare { "cfuture_bare_msg" };
        rpp::async_task([]() -> int { throw std::domain_error{"cfuture_bare_msg"}; }).continue_with([](int) {});
        AssertThat(bare.wait().find("continue_with()") != std::string::npos, true);
    }

    // a handler of cfuture<void> which takes another type must not stop the program
    TestCase(continue_with_on_a_void_future_logs_an_error_of_an_unknown_type)
    {
        warning_capture warning { "of an unknown type" };
        cfuture<void> failed = rpp::async_task([] { throw 42; });
        failed.continue_with([] {}, [](const std::invalid_argument&) {}); // this handler takes another type
        AssertThat(warning.wait().find("continue_with()") != std::string::npos, true);

        warning_capture bare { "cfuture_void_bare_msg" };
        rpp::async_task([] { throw std::domain_error{"cfuture_void_bare_msg"}; }).continue_with([] {});
        AssertThat(bare.wait().find("continue_with()") != std::string::npos, true);
    }

    TestCase(chain_async_futures_void)
    {
        bool task1Called = false;
        bool task2Called = false;
        bool task3Called = false;

        cfuture<void> tasks;
        tasks.chain_async(
            [&]{ task1Called = true; }
        ).chain_async(
            [&]{ task2Called = true; throw std::runtime_error("task2 failed"); }
        ).chain_async(
            [&]{ task3Called = true; }
        );
        tasks.get(); // waits until task3 is completed to be completed in sequence

        AssertThat(task1Called, true);
        AssertThat(task2Called, true);
        AssertThat(task3Called, true);
    }

    TestCase(chain_async_futures_T)
    {
        bool task1Called = false;
        bool task2Called = false;
        bool task3Called = false;

        cfuture<std::string> tasks;
        tasks.chain_async(
            [&]() -> std::string { task1Called = true; return "task1"; }
        ).chain_async(
            [&]() -> std::string { task2Called = true; throw std::runtime_error("task2 failed"); }
        ).chain_async(
            [&]() -> std::string { task3Called = true; return "task3"; }
        );
        // waits until task3 is completed to be completed in sequence
        std::string result = tasks.get();

        AssertThat(task1Called, true);
        AssertThat(task2Called, true);
        AssertThat(task3Called, true);
        AssertThat(result, "task3");
    }

    TestCase(chain_async_futures_continue_completed)
    {
        bool task1Called = false;
        bool task2Called = false;
        bool task3Called = false;

        cfuture<void> tasks;
        tasks.chain_async(
            [&]{ task1Called = true; }
        ).chain_async(
            [&]{ task2Called = true; }
        );
        tasks.wait(); // waits until tasks are completed in sequence

        AssertThat(task1Called, true);
        AssertThat(task2Called, true);

        tasks.chain_async(
            [&]{ task3Called = true; }
        );
        tasks.get();
        AssertThat(task3Called, true);
    }

    TestCase(ready_future)
    {
        auto future = make_ready_future(42);
        int result = future.get();
        AssertThat(result, 42);
    }

    TestCase(exceptional_future)
    {
        bool exceptionWasThrown = false;
        try
        {
            auto future = make_exceptional_future<int>(std::runtime_error{"aargh!"s});
            (void)future.get();
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
        cfuture<std::string> f = async_task([] {
            return "future string"s;
        });
        AssertThat(f.get(), "future string"s);
    }

    TestCase(basic_async_task_chaining)
    {
        cfuture<std::string> f = async_task([] {
            return "future string"s;
        });

        bool continuationCalled = false;
        f.then([&](std::string s) {
            continuationCalled = true;
            AssertThat(s.empty(), false);
        }).get();

        AssertThat(continuationCalled, true);
    }

    TestCase(invalidates_after_get)
    {
        cfuture<std::string> f1 = async_task([] {
            return "future string"s;
        });
        AssertThat(f1.get(), "future string"s);
        AssertThat(f1.valid(), false);
    }

    TestCase(invalidates_after_get_void)
    {
        cfuture<void> f1 = async_task([] { });
        f1.get();
        AssertThat(f1.valid(), false);
    }

    // a ready future which nobody collected was not abandoned, so its destructor collects it
    // and returns. If the destructor terminates on a ready result, the test run stops here
    TestCase(destructor_collects_a_ready_future)
    {
        { cfuture<int> value = make_ready_future(42); }
        { cfuture<void> nothing = make_ready_future(); }
        {
            cfuture<int> waited = async_task([] { return 7; });
            waited.wait(); // the result is ready, and get() never runs
        }
    }

    // a deferred future runs on get(), so no wait finishes it. Reporting finished would send
    // a caller which polls with a zero timeout straight into a blocking get()
    TestCase(deferred_future_never_reports_finished)
    {
        bool ran = false;
        cfuture<int> f = std::async(std::launch::deferred, [&] { ran = true; return 42; });

        AssertThat(f.wait_for(rpp::Duration::zero()) == wait_result::deferred, true);
        AssertThat(f.wait_for(rpp::millis(1)) == wait_result::finished, false);
        AssertThat(ran, false); // the poll must not have run the callable

        AssertThat(f.get(), 42);
        AssertThat(ran, true);
    }

};

// NOLINTEND(performance-*)
