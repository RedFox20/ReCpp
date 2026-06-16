#include <rpp/event_loop.h>
#include <rpp/task.h>
#include <rpp/tests.h>
#include <rpp/threads.h> // rpp::get_thread_id
#include <rpp/timer.h> // rpp::sleep_ms
#include <atomic>
#include <memory>
#include <stdexcept>
using namespace rpp;

// rpp::task<T> is eager (starts at construction) and resumes its awaiter on whatever thread the
// body completes on — never a background worker (unlike a bare `co_await cfuture<T>`).
// The leaves here use loop.run_async(), which hops to a pool worker and posts the resume back to
// the loop, so a task chain over those leaves stays on the loop thread end-to-end.

#if RPP_HAS_COROUTINES

// NOLINTBEGIN(cppcoreguidelines-avoid-capturing-lambda-coroutines)
namespace
{
    rpp::task<int> task_leaf(rpp::event_loop& loop)
    {
        int v = co_await loop.run_async(
            []
            {
                rpp::sleep_ms(2);
                return 7;
            });
        co_return v;
    }
    rpp::task<int> task_chain(rpp::event_loop& loop) // nested task: symmetric transfer, stays on loop
    {
        int v = co_await task_leaf(loop);
        co_return v * 6;
    }
    rpp::task<int> task_sync() // no co_await: completes synchronously when started
    {
        co_return 99;
    }
    rpp::task<int> task_throws(rpp::event_loop& loop)
    {
        co_await loop.run_async([] { rpp::sleep_ms(2); });
        throw std::runtime_error("boom");
        co_return 0; // unreachable
    }
    rpp::task<void> task_void(rpp::event_loop& loop, std::atomic<bool>& ran)
    {
        co_await loop.run_async([] { rpp::sleep_ms(2); });
        ran.store(true);
    }
    rpp::task<int> task_eager_marker(rpp::event_loop& loop, std::atomic<bool>& started)
    {
        started.store(true); // runs at CONSTRUCTION (eager), before the first co_await suspends us
        co_await loop.run_async([] { rpp::sleep_ms(2); });
        co_return 5;
    }

    // deferred (LAZY) equivalents: the body does not run until the result is awaited / started.
    rpp::deferred<int> deferred_leaf(rpp::event_loop& loop, std::atomic<bool>& started)
    {
        started.store(true); // runs only once this deferred is awaited (NOT at construction)
        int v = co_await loop.run_async(
            []
            {
                rpp::sleep_ms(2);
                return 7;
            });
        co_return v;
    }
    rpp::deferred<int> deferred_chain(rpp::event_loop& loop, std::atomic<bool>& started)
    {
        int v = co_await deferred_leaf(loop, started); // nested deferred: symmetric-transfer start
        co_return v * 6;
    }
    // counts body entries so a test can assert the task ran exactly once.
    rpp::deferred<int> deferred_counting(rpp::event_loop& loop, std::atomic<int>& runs)
    {
        runs.fetch_add(1); // body entry — must happen exactly once across start()/co_await
        co_await loop.run_async([] { rpp::sleep_ms(2); });
        co_return 7;
    }
} // namespace
#endif

TestImpl(test_task)
{
    TestInit(test_task) {}

#if RPP_HAS_COROUTINES

    std::unique_ptr<rpp::event_loop> loop;
    const uint64 main_tid = rpp::get_thread_id();

    TestCaseSetup()
    {
        loop = std::make_unique<rpp::event_loop>();
    }
    TestCaseCleanup()
    {
        loop.reset();
    }

    // drives a TestCaseCoro body by pumping our event_loop until the coroutine completes
    void run_coro_test(test_coro & coro) override
    {
        while (!coro.done())
            loop->run_once(rpp::millis(15));
        coro.rethrow_if_exception();
    }

    // A chained task over a background leaf returns its value and resumes the awaiter on the loop.
    TestCase(resumes_on_loop_with_value)
    {
        std::atomic<int> result{-1};
        std::atomic<uint64> resume_tid{0};
        loop->run_async_void(
            [&]() -> rpp::event_task
            {
                result = co_await task_chain(*loop);
                resume_tid = rpp::get_thread_id();
            });
        loop->run_until_idle();
        AssertThat(result.load(), 42);
        AssertThat(resume_tid.load(), main_tid); // resumed on the loop, NOT a pool worker
    }

    // A task with no internal await completes synchronously and returns its value.
    TestCase(synchronous_task_returns_value)
    {
        std::atomic<int> result{-1};
        loop->run_async_void([&]() -> rpp::event_task { result = co_await task_sync(); });
        loop->run_until_idle();
        AssertThat(result.load(), 99);
    }

    // An exception thrown in the body propagates out of co_await.
    TestCase(propagates_exception)
    {
        std::atomic<bool> caught{false};
        loop->run_async_void(
            [&]() -> rpp::event_task
            {
                try
                {
                    (void)co_await task_throws(*loop);
                }
                catch (const std::exception&)
                {
                    caught = true;
                }
            });
        loop->run_until_idle();
        AssertThat(caught.load(), true);
    }

    // The void specialization runs its body and resumes the awaiter on the loop.
    TestCase(void_task_runs_and_resumes_on_loop)
    {
        std::atomic<bool> ran{false};
        std::atomic<uint64> resume_tid{0};
        loop->run_async_void(
            [&]() -> rpp::event_task
            {
                co_await task_void(*loop, ran);
                resume_tid = rpp::get_thread_id();
            });
        loop->run_until_idle();
        AssertThat(ran.load(), true);
        AssertThat(resume_tid.load(), main_tid);
    }

    // EAGER: the body runs at construction (before any co_await), so the work is launched
    // immediately at the call site — not deferred until the awaiter or the loop gets to it.
    TestCase(starts_eagerly_at_construction)
    {
        std::atomic<bool> started{false};
        std::atomic<bool> started_before_await{false};
        std::atomic<int> result{-1};
        loop->run_async_void(
            [&]() -> rpp::event_task
            {
                rpp::task<int> t = task_eager_marker(*loop, started);
                started_before_await = started.load(); // body already ran up to its first co_await
                result = co_await t;
            });
        loop->run_until_idle();
        AssertThat(started_before_await.load(), true); // proves eager start at construction
        AssertThat(result.load(), 5);
    }

    // event_loop drives a top-level (un-awaited) eager task to completion via run_until_done,
    // and done() reports completion without awaiting.
    TestCase(run_until_done_drives_task_to_completion)
    {
        rpp::task<int> t = task_chain(*loop); // eager: already in flight, not finished yet
        AssertThat(t.done(), false);
        int result = loop->run_until_done(t);
        AssertThat(t.done(), true);
        AssertThat(result, 42);
    }

    // deferred is LAZY: the body does NOT run at construction — only when first awaited/started.
    // (Contrast starts_eagerly_at_construction above, where the eager task ran at construction.)
    TestCase(deferred_does_not_start_until_driven)
    {
        std::atomic<bool> started{false};
        rpp::deferred<int> d = deferred_chain(*loop, started);
        AssertThat(started.load(), false); // lazy: nothing ran yet
        int result = loop->run_until_done(d); // run_until_done starts it, then pumps to completion
        AssertThat(started.load(), true);
        AssertThat(result, 42);
    }

    // A deferred co_awaited inside a coroutine starts at the co_await and resumes on the loop.
    TestCase(deferred_resumes_on_loop_via_co_await)
    {
        std::atomic<bool> started{false};
        std::atomic<int> result{-1};
        std::atomic<uint64> resume_tid{0};
        loop->run_async_void(
            [&]() -> rpp::event_task
            {
                rpp::deferred<int> d = deferred_chain(*loop, started);
                // still lazy here — not started until the co_await below
                result = co_await d;
                resume_tid = rpp::get_thread_id();
            });
        loop->run_until_idle();
        AssertThat(started.load(), true);
        AssertThat(result.load(), 42);
        AssertThat(resume_tid.load(), main_tid);
    }

    // coroutine-style: an eager task has ALREADY run its body by the time we co_await it.
    TestCaseCoro(task_starts_eagerly_before_co_await)
    {
        std::atomic<bool> started{false};
        rpp::task<int> t = task_eager_marker(*loop, started);
        AssertThat(started.load(), true); // eager: body ran at construction, before this co_await
        int result = co_await t;
        AssertThat(result, 5);
    }

    // coroutine-style: a deferred task has NOT run its body until we co_await it.
    TestCaseCoro(deferred_starts_only_on_co_await)
    {
        std::atomic<bool> started{false};
        rpp::deferred<int> d = deferred_chain(*loop, started);
        AssertThat(started.load(), false); // lazy: nothing has run yet
        int result = co_await d; // co_await starts the body
        AssertThat(started.load(), true);
        AssertThat(result, 42);
    }

    // start() then co_await must run the body EXACTLY once. co_await must not re-launch an
    // in-flight deferred (that would double-resume); and the deferred may even finish during
    // start(), in which case await_ready short-circuits the co_await.
    TestCaseCoro(deferred_start_then_co_await_runs_once)
    {
        std::atomic<int> runs{0};
        rpp::deferred<int> d = deferred_counting(*loop, runs);
        d.start();
        int result = co_await d;
        AssertThat(runs.load(), 1); // body entered exactly once
        AssertThat(result, 7);
    }

    // Repeated start() (and run_until_done's own internal start()) must not run the body twice —
    // a task completes exactly once; re-running would make it a generator (a different pattern).
    TestCase(deferred_repeated_start_runs_once)
    {
        std::atomic<int> runs{0};
        rpp::deferred<int> d = deferred_counting(*loop, runs);
        d.start();
        d.start(); // idempotent: no second launch
        int result = loop->run_until_done(d); // also calls start() internally — still once
        AssertThat(runs.load(), 1);
        AssertThat(result, 7);
    }

    // NOLINTEND(cppcoreguidelines-avoid-capturing-lambda-coroutines)

#endif // RPP_HAS_COROUTINES
};
