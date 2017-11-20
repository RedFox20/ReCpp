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
        std::packaged_task<string(string)> loadString{
            [](string s) { return "future string: "+s; }
        };

        future<bool> chain = then(loadString.get_future(), [](string arg) -> bool
        {
            return arg.length() > 0;
        });
        loadString("arg");

        bool chainResult = chain.get();
        AssertThat(chainResult, true);
    }

    TestCase(void_chain)
    {
        std::packaged_task<void()> loadSomething{
            []{ }
        };

        future<string> async = then(loadSomething.get_future(), []() -> string
        {
            return "operation complete!";
        });
        loadSomething();

        string result = async.get();
        AssertThat(result, "operation complete!");
    }

    TestCase(cross_thread_exception_propagation)
    {
        future<void> asyncThrowingTask = std::async(launch::async, [] 
        {
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

} Impl;