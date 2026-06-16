#include <atomic>
#include <memory>
#include <rpp/event_loop.h>
#include <rpp/task.h>
#include <rpp/tests.h>
#include <rpp/threads.h> // rpp::get_thread_id
#include <rpp/timer.h> // rpp::sleep_ms
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
} // namespace
#endif

TestImpl(test_task)
{
    TestInit(test_task) {}

#if RPP_HAS_COROUTINES

    std::unique_ptr<rpp::event_loop> loop;
    const uint64 main_tid = rpp::get_thread_id();

    TestCaseSetup() { loop = std::make_unique<rpp::event_loop>(); }
    TestCaseCleanup() { loop.reset(); }

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

    // NOLINTEND(cppcoreguidelines-avoid-capturing-lambda-coroutines)

#endif // RPP_HAS_COROUTINES
};
