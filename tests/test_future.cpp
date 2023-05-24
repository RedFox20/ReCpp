#include <rpp/future.h>
#include <rpp/tests.h>
using namespace rpp;
using namespace std::chrono_literals;
using namespace std::this_thread;

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
        std::thread{[&]{ loadString(); }}.detach();

        cfuture<std::string> futureString = loadString.get_future();
        cfuture<bool> chain = futureString.then([](std::string arg) -> bool
        {
            return arg.length() > 0;
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
        std::thread{[&]{ loadString(); }}.detach();

        bool continuationCalled = false;
        cfuture<std::string> futureString = loadString.get_future();
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

    TestCase(sharing_future_string)
    {
        cfuture<std::string> f1 = async_task([] {
            return "future string"s;
        });
        cfuture<std::string> f2 = f1;  // NOLINT
        AssertThat(f1.get(), "future string"s);
        AssertThat(f2.get(), "future string"s);
    }

    TestCase(sharing_future_void)
    {
        cfuture<void> f1 = async_task([] { });
        cfuture<void> f2 = f1;
        f1.get();
        f2.get(); // already OK if this doesn't throw exception
    }

    TestCase(get_tasks_string)
    {
        std::vector<std::string> items = {
            "stringA",
            "stringB",
            "stringC"
        };
        std::vector<std::string> Tasks = rpp::get_tasks(items, [&](std::string& s) {
            return rpp::async_task([&] {
                return "future " + s;
            });
        });
        AssertThat(Tasks.size(), 3u);
        AssertThat(Tasks[0], "future stringA"s);
        AssertThat(Tasks[1], "future stringB"s);
        AssertThat(Tasks[2], "future stringC"s);
    }

    TestCase(get_async_tasks)
    {
        std::vector<std::string> items = {
            "stringA",
            "stringB",
            "stringC"
        };
        std::vector<std::string> Tasks = rpp::get_async_tasks(items, [&](std::string& s) {
            return "future " + s;
        });
        AssertThat(Tasks.size(), 3u);
        AssertThat(Tasks[0], "future stringA"s);
        AssertThat(Tasks[1], "future stringB"s);
        AssertThat(Tasks[2], "future stringC"s);
    }

// C++20 coro support
#if RPP_HAS_CXX20

    rpp::cfuture<std::string> string_coro()
    {
        co_await std::chrono::milliseconds{1};
        co_return "string from coro";
    }

    TestCase(coroutines_basic_string_coro)
    {
        AssertThat(string_coro().get(), "string from coro"s);
    }

    cfuture<void> void_coro(std::string& result)
    {
        co_await std::chrono::milliseconds{1};
        result = co_await string_coro();
        co_return;
    }

    TestCase(coroutines_basic_void_coro)
    {
        std::string result = "default";
        auto f = void_coro(result);
        AssertThat(result, "default"s);
        f.get();
        AssertThat(result, "string from coro"s);
    }

    cfuture<std::string> as_async(std::string s)
    {
        co_await std::chrono::milliseconds{1};
        co_return s;
    }

    cfuture<std::string> multi_stage_coro()
    {
        std::string s = co_await as_async("123_");
        s += co_await as_async("456_");
        s += co_await as_async("789");
        co_return s;
    }

    TestCase(coroutines_multi_stage_coro)
    {
        AssertThat(multi_stage_coro().get(), "123_456_789"s);
    }

    cfuture<std::string> future_string_coro()
    {
        co_return co_await async_task([] {
            return "future string"s;
        });
    }

    TestCase(coroutines_await_on_async_task)
    {
        AssertThat(future_string_coro().get(), "future string"s);
    }

    cfuture<std::string> exceptional_coro()
    {
        co_await std::chrono::milliseconds{1};

        throw std::runtime_error{"aargh!"};
        co_return "aargh!";
    }

    cfuture<std::string> exception_handling_coro()
    {
        std::string s = co_await as_async("abc");
        bool exceptionWasThrown = false;
        try
        {
            s += co_await exceptional_coro();
            s += "xyz";
        }
        catch (const std::exception& e)
        {
            exceptionWasThrown = true;
            AssertThat(e.what(), "aargh!"s);
        }
        AssertThat(exceptionWasThrown, true);

        s += co_await as_async("def");
        co_return s;
    }
    TestCase(coroutines_exception_handling)
    {
        AssertThat(exception_handling_coro().get(), "abcdef"s);
    }

#endif
};
