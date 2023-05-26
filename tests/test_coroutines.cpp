#include <rpp/coroutines.h>
#include <rpp/future.h>

#include <rpp/tests.h>
using namespace rpp;
using namespace std::chrono_literals;
using namespace std::this_thread;

TestImpl(test_coroutines)
{
    TestInit(test_coroutines)
    {
    }

#if RPP_HAS_COROUTINES

    rpp::cfuture<std::string> string_coro()
    {
        co_await std::chrono::milliseconds{1};
        co_return "string from coro";
    }
    TestCase(basic_string_coro)
    {
        AssertThat(string_coro().get(), "string from coro"s);
    }

    cfuture<void> void_coro(std::string& result)
    {
        co_await std::chrono::milliseconds{1};
        result = co_await string_coro();
        co_return;
    }
    TestCase(basic_void_coro)
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
    TestCase(multi_stage_coro)
    {
        AssertThat(multi_stage_coro().get(), "123_456_789"s);
    }

    cfuture<std::string> future_string_coro()
    {
        co_return co_await async_task([] {
            return "future string"s;
        });
    }
    TestCase(await_on_async_task)
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
    TestCase(exception_handling)
    {
        AssertThat(exception_handling_coro().get(), "abcdef"s);
    }

    cfuture<std::vector<int>> destructor_sequence_coro()
    {
        std::vector<int> destructor_ids;

        struct destructor_recorder
        {
            std::vector<int>& results;
            const int id;
            ~destructor_recorder() noexcept { results.push_back(id); }
        };

        co_await [&destructor_ids]() -> cfuture<void>
        {
            destructor_recorder dr {destructor_ids, 1};
            co_await std::chrono::milliseconds{10};
        };
        AssertThat(destructor_ids.size(), 1u);
        AssertThat(destructor_ids[0], 1);

        co_await [&destructor_ids]() -> cfuture<void>
        {
            destructor_recorder dr {destructor_ids, 2};
            co_await std::chrono::milliseconds{5};
        };
        AssertThat(destructor_ids.size(), 2u);
        AssertThat(destructor_ids[1], 2);

        co_await [&destructor_ids]() -> cfuture<void>
        {
            destructor_recorder dr {destructor_ids, 3};
            co_return;
        };
        AssertThat(destructor_ids.size(), 3u);
        AssertThat(destructor_ids[2], 3);

        co_return destructor_ids;
    }

    TestCase(ensure_destructors_are_called_sequentially)
    {
        auto f = destructor_sequence_coro();
        std::vector<int> destructor_ids = f.get();
        AssertThat(destructor_ids.size(), 3u);
        AssertThat(destructor_ids[0], 1);
        AssertThat(destructor_ids[1], 2);
        AssertThat(destructor_ids[2], 3);
    }
#endif
};
