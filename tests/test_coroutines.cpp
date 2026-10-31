#include <rpp/coroutines.h>
#include <rpp/async.h>
#include <rpp/thread_pool.h>
#include <rpp/timer.h>

#include <rpp/tests.h>
#include <string> // std::string
using namespace rpp;
using namespace std::this_thread;
using namespace std::string_literals;

TestImpl(test_coroutines)
{
    rpp::mutex m;

    TestInit(test_coroutines)
    {
    }

    TestCleanup()
    {
        rpp::thread_pool::global().clear_idle_tasks();
    }


    template<class T>
    void set_locked(T& out, const T& value)
    {
        std::lock_guard lock{m};
        out = value;
    }
    template<class T>
    T get_locked(const T& value)
    {
        std::lock_guard lock{m};
        return value;
    }

    static rpp::future<void> duration_coro(int millis)
    {
        co_await rpp::millis(millis);
    }
    TestCase(basic_duration_coro)
    {
        // The lower bound is the real property: the sleep must not wake early. The upper
        // bound only catches a gross overshoot, because a sanitizer adds tens of ms.
        rpp::Timer t1;
        duration_coro(50).get();
        double e = t1.elapsed_millis();
        AssertGreater(e, 49.0);
        AssertLess(e, 75.0);

        rpp::Timer t2;
        duration_coro(15).get();
        double e2 = t2.elapsed_millis();
        AssertGreater(e2, 14.0);
        AssertLess(e2, 40.0);
    }


    static rpp::future<std::string> string_coro()
    {
        co_await rpp::millis(1);
        co_return "string from coro";
    }
    TestCase(basic_string_coro)
    {
        AssertThat(string_coro().get(), "string from coro"s);
    }


    rpp::future<void> void_coro(std::string* result)
    {
        co_await rpp::millis(1);
        set_locked(*result, co_await string_coro());
        co_return;
    }
    TestCase(basic_void_coro)
    {
        std::string result = "default";
        auto f = void_coro(&result);
        AssertThat(get_locked(result), "default"s);
        f.get();
        AssertThat(result, "string from coro"s);
    }


    static rpp::future<std::string> as_async(std::string s)
    {
        co_await rpp::millis(1);
        co_return s;
    }
    static rpp::future<std::string> multi_stage_coro()
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


    static rpp::future<std::string> future_string_coro()
    {
        co_return co_await rpp::async([] {
            return "future string"s;
        });
    }
    TestCase(await_on_async)
    {
        AssertThat(future_string_coro().get(), "future string"s);
    }


    static rpp::future<std::string> exceptional_coro()
    {
        co_await rpp::millis(1);

        throw std::runtime_error{"aargh!"};
        co_return "aargh!";
    }
    static rpp::future<std::string> exception_handling_coro()
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


    static rpp::future<std::vector<int>> destructor_sequence_coro()
    {
        rpp::mutex m;
        std::vector<int> destructor_ids;

        struct destructor_recorder
        {
            rpp::mutex& m;
            std::vector<int>& results;
            const int id;
            ~destructor_recorder() noexcept {
                std::lock_guard lock{m};
                results.push_back(id); // NOLINT(bugprone-exception-escape)
            }
        };

        rpp::Timer t1;
        co_await [&m, &destructor_ids]() -> rpp::future<void> // NOLINT(cppcoreguidelines-avoid-capturing-lambda-coroutines)
        {
            destructor_recorder dr1 {m, destructor_ids, 1};
            co_await rpp::millis(10);
        };
        print_warning("dtor sequence #1 elapsed: %.2fms\n", t1.elapsed_millis());
        {
            std::lock_guard lock { m };
            AssertThat(destructor_ids.size(), 1u);
            if (!destructor_ids.empty()) AssertThat(destructor_ids[0], 1);
        }

        rpp::Timer t2;
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
        std::string fstr = co_await [&m, &destructor_ids]() -> rpp::future<std::string>
        {
            destructor_recorder dr2 {m, destructor_ids, 2};
            co_await rpp::millis(5);
            co_return "test";
        };
        print_warning("dtor sequence #2 elapsed: %.2fms\n", t2.elapsed_millis());
        AssertThat(fstr, "test"s);
        {
            std::lock_guard lock { m };
            AssertThat(destructor_ids.size(), 2u);
            if (destructor_ids.size() >= 2) AssertThat(destructor_ids[1], 2);
        }

        rpp::Timer t3;
        co_await [&m, &destructor_ids]() -> rpp::future<void> // NOLINT(cppcoreguidelines-avoid-capturing-lambda-coroutines)
        {
            destructor_recorder dr3 {m, destructor_ids, 3};
            co_return;
        };
        print_warning("dtor sequence #3 elapsed: %.2fms\n", t3.elapsed_millis());
        {
            std::lock_guard lock { m };
            AssertThat(destructor_ids.size(), 3u);
            if (destructor_ids.size() >= 3) AssertThat(destructor_ids[2], 3);
        }

        co_return destructor_ids;
    }
    TestCase(ensure_destructors_are_called_sequentially)
    {
        auto f = destructor_sequence_coro();
        std::vector<int> destructor_ids = f.get();
        AssertThat(destructor_ids.size(), 3u);
        if (destructor_ids.size() >= 1) AssertThat(destructor_ids[0], 1); // NOLINT(readability-container-size-empty)
        if (destructor_ids.size() >= 2) AssertThat(destructor_ids[1], 2);
        if (destructor_ids.size() >= 3) AssertThat(destructor_ids[2], 3);
    }


    // the coroutine resumes on a pool worker, where an assertion cannot fail the case, so it returns the result
    static rpp::future<std::string> functor_nonfuture_void_coro()
    {
        std::string result = "default";
        // tests functor_awaiter<void>, lambda is converted to rpp::delegate<void()>
        co_await [&]() -> void {
            rpp::sleep_ms(1);
            result = "string from lambda coro with dynamic alloc";
        };
        co_return result;
    }
    TestCase(functor_nonfuture_void_coro)
    {
        AssertThat(functor_nonfuture_void_coro().get(), "string from lambda coro with dynamic alloc"s);
    }

    static rpp::future<std::string> functor_nonfuture_string_coro()
    {
        // tests functor_awaiter<T> with a return value, lambda is converted to rpp::delegate<std::string()>
        std::string result = co_await [&]() -> std::string {
            rpp::sleep_ms(1);
            std::string r = "string from lambda coro with dynamic alloc";
            return r;
        };
        co_return result;
    }
    TestCase(functor_nonfuture_string_coro)
    {
        AssertThat(functor_nonfuture_string_coro().get(), "string from lambda coro with dynamic alloc"s);
    }
};
