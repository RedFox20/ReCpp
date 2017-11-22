#include <rpp/future.h>
#include <rpp/tests.h>
using namespace rpp;


TestImpl(test_future)
{
    TestInit(test_future)
    {
    }

    TestCase(simple_chaining)
    {
        std::packaged_task<string()> loadString{[]{ 
            this_thread::sleep_for(15ms);
            return "future string";
        }};
        std::thread{[&]{ loadString(); }}.detach();

        future<bool> chain = then(loadString.get_future(), [](string arg) -> bool
        {
            return arg.length() > 0;
        });

        bool chainResult = chain.get();
        AssertThat(chainResult, true);
    }

    TestCase(chain_mutate_void_to_string)
    {
        std::packaged_task<void()> loadSomething{[]{ 
            this_thread::sleep_for(15ms);
        }};
        std::thread{[&]{ loadSomething(); }}.detach();

        future<string> async = then(loadSomething.get_future(), []() -> string
        {
            return "operation complete!"s;
        });

        string result = async.get();
        AssertThat(result, "operation complete!");
    }

    TestCase(chain_decay_string_to_void)
    {
        std::packaged_task<string()> loadString{[]{ 
            this_thread::sleep_for(15ms);
            return "some string";
        }};
        std::thread{[&]{ loadString(); }}.detach();

        bool continuationCalled = false;
        future<void> async = then(loadString.get_future(), [&](string s)
        {
            continuationCalled = true;
            AssertThat(s.empty(), false);
        });

        async.get();
        AssertThat(continuationCalled, true);
    }

    TestCase(cross_thread_exception_propagation)
    {
        future<void> asyncThrowingTask = std::async(launch::async, [] {
            throw runtime_error("background_thread_exception_msg");
        });

        string result;
        try
        {
            asyncThrowingTask.get();
        }
        catch (const runtime_error& e)
        {
            result = e.what();
        }
        AssertThat(result, "background_thread_exception_msg");
    }

    TestCase(composable_future_type)
    {
        composable_future<string> f = std::async(launch::async, [] {
            return "future string"s;
        });

        int totalCalls = 0;
        f.then([&](string s) {
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
        composable_future<void> f = std::async(launch::async, [] {
            throw runtime_error("background_thread_exception_msg");
        });

        bool exceptHandlerCalled = false;
        int result = f.then([] {
            AssertMsg(false, "This callback should never be executed");
            return 0;
        }, [&](const exception& e) {
           exceptHandlerCalled = true;
           AssertThat(e.what(), "background_thread_exception_msg"s);
           return 42;
        }).get();

        AssertThat(exceptHandlerCalled, true);
        AssertThat(result, 42);
    }

    TestCase(except_handler_chaining)
    {
        composable_future<string> f = std::async(launch::async, [] {
            return "future string"s;
        });

        bool secondExceptHandlerCalled = false;
        int result = f.then([](string s) {
            throw runtime_error("future_continuation_exception_msg");
            return 0;
        }).then([](int x) { 
            return 5;
        }, [&](const exception& e) {
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
            auto future = make_exceptional_future<int>(runtime_error{"aargh!"s});
            future.get();
        }
        catch (const exception& e)
        {
            exceptionWasThrown = true;
            AssertThat(e.what(), "aargh!"s);
        }
        AssertThat(exceptionWasThrown, true);
    }

} Impl;