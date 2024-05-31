#include <rpp/coroutines.h>
#include <rpp/future.h>
#include <rpp/thread_pool.h>
#include <rpp/timer.h>

#include <rpp/tests.h>
using namespace rpp;
using namespace std::chrono_literals;
using namespace std::this_thread;

using mutex = rpp::pool_worker::mutex;

TestImpl(test_coroutines)
{
    TestInit(test_coroutines)
    {
    }

    TestCleanup()
    {
        rpp::thread_pool::global().clear_idle_tasks();
    }

#if RPP_HAS_COROUTINES

    rpp::cfuture<void> chrono_coro(int millis)
    {
        co_await std::chrono::milliseconds{millis};
    }
    TestCase(basic_chrono_coro)
    {
        rpp::Timer t1;
        chrono_coro(50).get();
        double e = t1.elapsed_millis();
        // require some level of acceptable accuracy in the sleeps
        AssertGreater(e, 49.0);
        AssertLess(e, 55.0);

        rpp::Timer t2;
        chrono_coro(15).get();
        double e2 = t2.elapsed_millis();
        // require some level of acceptable accuracy in the sleeps
        AssertGreater(e2, 14.0);
        AssertLess(e2, 20.0);
    }


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
        mutex m;
        std::vector<int> destructor_ids;

        struct destructor_recorder
        {
            mutex& m;
            std::vector<int>& results;
            const int id;
            ~destructor_recorder() noexcept {
                std::lock_guard lock{m};
                results.push_back(id);
            }
        };

        rpp::Timer t1;
        co_await [&m, &destructor_ids]() -> cfuture<void>
        {
            destructor_recorder dr {m, destructor_ids, 1};
            co_await std::chrono::milliseconds{10};
        };
        printf("t1 elapsed: %f\n", t1.elapsed_millis());
        AssertThat(destructor_ids.size(), 1u);
        AssertThat(destructor_ids[0], 1);

        rpp::Timer t2;
        std::string fstr = co_await [&m, &destructor_ids]() -> cfuture<std::string>
        {
            destructor_recorder dr {m, destructor_ids, 2};
            co_await std::chrono::milliseconds{5};
            co_return "test";
        };
        printf("t2 elapsed: %f\n", t2.elapsed_millis());
        AssertThat(fstr, "test"s);
        AssertThat(destructor_ids.size(), 2u);
        AssertThat(destructor_ids[1], 2);

        rpp::Timer t3;
        co_await [&m, &destructor_ids]() -> cfuture<void>
        {
            destructor_recorder dr {m, destructor_ids, 3};
            co_return;
        };
        printf("t3 elapsed: %f\n", t3.elapsed_millis());
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


    cfuture<void> std_future_void_coro()
    {
        co_await std::chrono::milliseconds{10};
        co_return;
    }
    TestCase(std_future_void_coro)
    {
        std_future_void_coro().get();
    }


    cfuture<std::string> std_future_string_coro()
    {
        std::string s = co_await std::async(std::launch::async, [] {
            return "future string"s;
        });
        co_return s;
    }
    TestCase(std_future_string_coro)
    {
        std::string s = std_future_string_coro().get();
        AssertThat(s, "future string"s);
    }


    cfuture<std::string> std_future_lambda_coro()
    {
        // await on a lambda which returns a future
        std::string s = co_await []() -> std::future<std::string> {
            return std::async(std::launch::async, [] {
                return "future string"s;
            });
        };
        co_return s;
    }

    TestCase(std_future_lambda_coro)
    {
        std::string s = std_future_lambda_coro().get();
        AssertThat(s, "future string"s);
    }

#endif
};
