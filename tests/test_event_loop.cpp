#include <rpp/event_loop.h>
#include <rpp/coroutines.h>
#include <rpp/future.h>
#include <rpp/thread_pool.h>
#include <rpp/timer.h>
#include <rpp/threads.h>
#include <rpp/collections.h>

#include <rpp/tests.h>
#include <atomic>
#include <memory>
#include <vector>
#include <string>
using namespace rpp;
using namespace std::chrono_literals;

TestImpl(test_event_loop)
{
    TestInit(test_event_loop)
    {
    }

    TestCleanup()
    {
        rpp::thread_pool::global().clear_idle_tasks();
    }

#if RPP_HAS_COROUTINES

    std::unique_ptr<rpp::event_loop> loop;
    const uint64 main_tid = rpp::get_thread_id();

    TestCaseSetup()
    {
        loop = std::make_unique<rpp::event_loop>();
    }
    // this sets up a custom loop runner for TestCaseCoro()
    // so we can test how a loop system would actually work
    void run_coro_test(test_coro& coro) override
    {
        while (!coro.done())
            loop->run_once(rpp::Duration::from_millis(15));
        coro.rethrow_if_exception();
    }
    TestCaseCleanup()
    {
        loop.reset();
    }

    // NOLINTBEGIN(cppcoreguidelines-avoid-capturing-lambda-coroutines)
    void assert_on_loop_thread(std::source_location loc = std::source_location::current())
    { AssertThatLoc(loc, rpp::get_thread_id(), main_tid); }
    void idle() const { loop->run_until_idle(); }

    template<typename F>
    auto run_bg(F&& f) { return loop->run_background(std::forward<F>(f)); }


    // ─── Helper: record which thread resumed each step ──────────
    struct thread_recorder
    {
        rpp::mutex m;
        std::vector<uint64> resume_thread_ids;
        uint64 loop_thread = rpp::get_thread_id();
        void record()
        {
            std::lock_guard lock{m};
            resume_thread_ids.push_back(rpp::get_thread_id());
        }
        bool all_on_loop_thread()
        {
            std::lock_guard lock{m};
            for (auto id : resume_thread_ids)
                if (id != loop_thread) return false;
            return true;
        }
        size_t count()
        {
            std::lock_guard lock{m};
            return resume_thread_ids.size();
        }
    };

    // ─── Basic: run_background with return value ────────────────
    TestCaseCoro(basic_run_background)
    {
        std::string result = co_await run_bg([main_tid=main_tid]() -> std::string
        {
            AssertNotEqual(rpp::get_thread_id(), main_tid);
            rpp::sleep_ms(5);
            return "hello from background";
        });
        assert_on_loop_thread();
        AssertThat(result, "hello from background"s);
    }

    // ─── Basic: run_background<void> ────────────────────────────
    TestCaseCoro(basic_run_background_void)
    {
        std::atomic<bool> work_done{false};
        co_await run_bg([&]() {
            rpp::sleep_ms(5);
            work_done = true;
        });
        assert_on_loop_thread();
        AssertThat(work_done.load(), true);
    }

    // ─── Multi-stage coroutine: all resumes on loop thread ──────
    TestCaseCoro(multi_stage_resumes_on_loop_thread)
    {
        thread_recorder rec {};
        std::string s;

        s += co_await run_bg([&]() -> std::string {
            rpp::sleep_ms(2);
            return "A";
        });
        rec.record();

        s += co_await run_bg([&]() -> std::string {
            rpp::sleep_ms(2);
            return "B";
        });
        rec.record();

        s += co_await run_bg([&]() -> std::string {
            rpp::sleep_ms(2);
            return "C";
        });
        rec.record();

        AssertThat(s, "ABC"s);
        AssertThat(rec.count(), 3u);
        AssertThat(rec.all_on_loop_thread(), true);
    }

    // ─── Multiple independent coroutines interleave on the loop ─
    // non-coro TestCase: launches multiple independent coroutines
    TestCase(multiple_coroutines_interleave)
    {
        rpp::mutex order_m;
        std::vector<int> resume_order;

        auto record = [&](int id)
        {
            assert_on_loop_thread();
            std::lock_guard lock{order_m};
            resume_order.push_back(id);
        };

        auto coro_a = [&]() -> rpp::cfuture<int>
        {
            co_await run_bg([&] {
                rpp::sleep_ms(15); // slower task
            });
            record(1);
            co_return 10;
        };

        auto coro_b = [&]() -> rpp::cfuture<int>
        {
            co_await run_bg([&] {
                rpp::sleep_ms(2); // faster task
            });
            record(2);
            co_return 20;
        };

        auto fa = coro_a();
        auto fb = coro_b();
        idle();

        AssertThat(fa.get(), 10);
        AssertThat(fb.get(), 20);
        AssertThat(resume_order.size(), 2u);
        // B (2ms) finishes before A (15ms), so B resumes first
        AssertThat(resume_order[0], 2); // B first
        AssertThat(resume_order[1], 1); // A second
    }

    // ─── Suspended tasks are paused while other tasks resume ────
    // non-coro TestCase: launches multiple independent coroutines
    TestCase(suspended_tasks_paused_while_others_resume)
    {
        // while coro_slow is suspended (waiting on a long background task),
        // coro_fast can be resumed on the loop thread.

        std::atomic<int> fast_resume_count{0};
        std::atomic<bool> slow_bg_running{false};
        std::atomic<bool> fast_completed_while_slow_suspended{false};

        auto coro_slow = [&]() -> rpp::cfuture<std::string>
        {
            std::string result = co_await run_bg([&]() -> std::string {
                slow_bg_running.store(true);
                rpp::sleep_ms(50);
                return "slow";
            });
            assert_on_loop_thread();
            co_return result;
        };

        auto coro_fast = [&]() -> rpp::cfuture<std::string>
        {
            co_await run_bg([&]() {
                rpp::sleep_ms(2);
            });
            fast_resume_count.fetch_add(1);
            assert_on_loop_thread();

            co_await run_bg([&]() {
                rpp::sleep_ms(2);
            });
            fast_resume_count.fetch_add(1);
            assert_on_loop_thread();

            if (slow_bg_running.load())
                fast_completed_while_slow_suspended.store(true);

            co_return "fast";
        };

        auto fs = coro_slow();
        auto ff = coro_fast();
        idle();

        AssertThat(fs.get(), "slow"s);
        AssertThat(ff.get(), "fast"s);
        AssertThat(fast_resume_count.load(), 2);
        AssertThat(fast_completed_while_slow_suspended.load(), true);
    }

    // ─── Exception propagation through event loop ───────────────
    TestCaseCoro(exception_propagation)
    {
        bool caught = false;
        try
        {
            co_await run_bg([&]() {
                throw std::runtime_error{"background error"};
            });
        }
        catch (const std::runtime_error& e)
        {
            caught = true;
            AssertThat(std::string(e.what()), "background error"s);
        }
        AssertThat(caught, true);
    }

    // ─── Exception in middle of multi-stage, with recovery ──────
    TestCaseCoro(exception_recovery_multi_stage)
    {
        std::string s = co_await run_bg([]() -> std::string {
            return "step1";
        });
        assert_on_loop_thread();

        bool caught_ex = false;
        try
        {
            co_await run_bg([]() {
                throw std::runtime_error{"step2 failed"};
            });
        }
        catch (const std::runtime_error&)
        {
            caught_ex = true;
        }
        AssertThat(caught_ex, true);
        assert_on_loop_thread();

        s += co_await run_bg([]() -> std::string {
            return "_step3";
        });
        assert_on_loop_thread();

        AssertThat(s, "step1_step3"s);
    }

    // ─── delay() awaiter resumes on loop thread ─────────────────
    TestCaseCoro(delay_resumes_on_loop_thread)
    {
        rpp::Timer t;
        co_await loop->delay(rpp::millis(20));
        assert_on_loop_thread();
        AssertGreater(t.elapsed_millis(), 18.0);
    }

    // ─── run_once with timeout ──────────────────────────────────
    // non-coro TestCase: tests run_once() API directly
    TestCase(run_once_basic)
    {
        std::atomic<int> resume_count{0};
        auto coro = [&]() -> rpp::cfuture<int>
        {
            int v = co_await run_bg([]() -> int {
                rpp::sleep_ms(5);
                return 42;
            });
            resume_count.fetch_add(1);
            assert_on_loop_thread();
            co_return v;
        };

        auto future = coro();

        // poll until done
        rpp::Timer t;
        while (loop->has_pending_work() && t.elapsed_millis() < 500.0)
        {
            loop->run_once(rpp::Duration::from_millis(10));
        }
        AssertThat(resume_count.load(), 1);
        AssertThat(future.get(), 42);
    }

    // ─── run_once with zero timeout is non-blocking ─────────────
    // non-coro TestCase: tests run_once() API directly, no coroutine needed
    TestCase(run_once_nonblocking)
    {
        // nothing in the queue, zero timeout should return immediately
        rpp::Timer t;
        bool processed = loop->run_once(rpp::Duration::zero());
        double elapsed = t.elapsed_millis();

        AssertThat(processed, false);
        AssertLess(elapsed, 5.0); // should be essentially instant
    }

    // ─── post() generic callback ────────────────────────────────
    // non-coro TestCase: no coroutine suspension needed
    TestCase(post_generic_callback)
    {
        int callback_value = 0;

        loop->post([&]() {
            callback_value = 42;
            assert_on_loop_thread();
        });

        idle();
        AssertThat(callback_value, 42);
    }

    // ─── Stress: many coroutines, all resume on loop thread ─────
    // non-coro TestCase: launches multiple independent coroutines
    TestCase(stress_many_coroutines_all_resume_on_loop)
    {
        constexpr int N = 20;
        std::atomic<int> total_resumes{0};

        auto make_coro = [&](int id) -> rpp::cfuture<int>
        {
            int result = co_await run_bg([id]() -> int {
                rpp::sleep_ms(1 + (id % 5));
                return id * 10;
            });

            total_resumes.fetch_add(1);
            assert_on_loop_thread();

            co_return result;
        };

        std::vector<rpp::cfuture<int>> futures;
        futures.reserve(N);
        for (int i = 0; i < N; ++i)
        {
            futures.emplace_back(make_coro(i));
        }

        idle();

        int sum = 0;
        for (int i = 0; i < N; ++i)
        {
            sum += futures[i].get();
        }
        AssertThat(sum, 1900);
        AssertThat(total_resumes.load(), N);
    }

    // ─── Multi-stage stress: nested awaits all on loop thread ───
    // non-coro TestCase: launches multiple independent coroutines
    TestCase(stress_multi_stage_coroutines)
    {
        constexpr int N = 8;
        constexpr int STAGES = 4;
        std::atomic<int> total_resumes{0};
        auto make_coro = [&](int id) -> rpp::cfuture<int>
        {
            int accum = 0;
            for (int stage = 0; stage < STAGES; ++stage)
            {
                int v = co_await run_bg([id, stage]() -> int {
                    rpp::sleep_ms(1);
                    return id * 100 + stage;
                });
                accum += v;

                total_resumes.fetch_add(1);
                assert_on_loop_thread();
            }
            co_return accum;
        };

        std::vector<rpp::cfuture<int>> futures;
        futures.reserve(N);
        for (int i = 0; i < N; ++i)
        {
            futures.emplace_back(make_coro(i));
        }

        idle();
        for (int i = 0; i < N; ++i)
        {
            // each coro: sum(i*100 + stage) for stage 0..3 = 4*i*100 + 0+1+2+3 = 400*i + 6
            int expected = 400 * i + 6;
            AssertThat(futures[i].get(), expected);
        }
        AssertThat(total_resumes.load(), N * STAGES);
    }

    // ─── Verify background work actually runs on worker threads ─
    TestCaseCoro(background_work_on_different_thread)
    {
        std::atomic<int> bg_on_different_thread{0};
        for (int i = 0; i < 5; ++i)
        {
            co_await run_bg([&]() {
                if (rpp::get_thread_id() != main_tid)
                    bg_on_different_thread.fetch_add(1);
                rpp::sleep_ms(1);
            });
            assert_on_loop_thread();
        }
        AssertThat(bg_on_different_thread.load(), 5);
    }

    // ─── Pending work tracking ──────────────────────────────────
    // non-coro TestCase: inspects loop state at specific interleaving points
    TestCase(pending_work_tracking)
    {
        // initially no pending work
        AssertThat(loop->has_pending_work(), false);

        std::atomic<bool> bg_started{false};
        std::atomic<bool> bg_may_finish{false};

        auto coro = [&]() -> rpp::cfuture<void>
        {
            co_await run_bg([&]() {
                bg_started.store(true);
                while (!bg_may_finish.load())
                    rpp::sleep_ms(1);
            });
            co_return;
        };

        auto future = coro();

        // wait until background task has actually started
        while (!bg_started.load())
            rpp::sleep_ms(1);

        // should have pending work now
        AssertThat(loop->has_pending_work(), true);

        // let it finish
        bg_may_finish.store(true);

        idle();
        future.get();

        // after run_until_idle completes, no more pending work
        AssertThat(loop->has_pending_work(), false);
    }

    // ─── Mixed: coroutines + posted callbacks ───────────────────
    // non-coro TestCase: launches multiple independent coroutines + callbacks
    TestCase(mixed_coroutines_and_callbacks)
    {
        rpp::synchronized<std::vector<std::string>> log;

        loop->post([&]() {
            log->push_back("callback_1");
            assert_on_loop_thread();
        });

        auto coro = [&]() -> rpp::cfuture<void>
        {
            co_await run_bg([&]() {
                rpp::sleep_ms(5);
            });
            log->push_back("coro_resume");
            assert_on_loop_thread();
            co_return;
        };

        auto future = coro();
        loop->post([&]() {
            log->push_back("callback_2");
            assert_on_loop_thread();
        });
        idle();
        future.get();

        AssertThat(log->size(), 3u);
        AssertThat(rpp::contains(**log, "callback_1"), true);
        AssertThat(rpp::contains(**log, "callback_2"), true);
        AssertThat(rpp::contains(**log, "coro_resume"), true);
    }

    // ─── await_future: wait for a cfuture on event loop ─────────
    TestCaseCoro(await_future_basic)
    {
        auto producer = [&]() -> rpp::cfuture<int>
        {
            co_return co_await run_bg([]() -> int {
                rpp::sleep_ms(5);
                return 99;
            });
        };
        auto produced_future = producer();
        int val = co_await loop->await_future(produced_future);
        assert_on_loop_thread();
        AssertThat(val, 99);
    }

    // ─── await_future: already-ready future resumes immediately ─
    TestCaseCoro(await_future_already_ready)
    {
        std::promise<int> prom;
        auto std_future = prom.get_future();
        prom.set_value(42);

        rpp::cfuture<int> ready_future {std::move(std_future)};

        int val = co_await loop->await_future(ready_future);
        assert_on_loop_thread();
        AssertThat(val, 42);
    }

    // ─── delay_until: time-point based delay ────────────────────
    TestCaseCoro(delay_until_resumes_on_loop_thread)
    {
        rpp::Timer t;
        rpp::TimePoint target = rpp::TimePoint::now() + rpp::millis(20);
        co_await loop->delay_until(target);
        assert_on_loop_thread();
        AssertGreater(t.elapsed_millis(), 18.0);
    }

    // ─── set_except_handler: custom exception handling ──────────
    // non-coro TestCase: no coroutine suspension needed
    TestCase(set_except_handler_catches_callback_exception)
    {
        std::string caught_message;
        loop->set_except_handler([&](std::exception_ptr ep)
        {
            try { std::rethrow_exception(std::move(ep)); }
            catch (const std::runtime_error& e) { caught_message = e.what(); }
        });

        loop->post([]() {
            throw std::runtime_error{"callback threw"};
        });

        idle();
        AssertThat(caught_message, "callback threw"s);
    }

    // ─── run_loop + stop: the primary event loop pattern ────────
    // non-coro TestCase: tests run_loop() API directly
    TestCase(run_loop_with_stop)
    {
        std::atomic<int> resumes{0};

        auto coro = [&]() -> rpp::cfuture<void>
        {
            co_await run_bg([&]() {
                rpp::sleep_ms(5);
            });
            resumes.fetch_add(1);
            assert_on_loop_thread();

            loop->stop();
            co_return;
        };

        auto future = coro();
        bool all_done = loop->run_loop();

        AssertThat(resumes.load(), 1);
        AssertThat(all_done, true);
        future.get();
    }

    // ─── main_thread_id accessor ────────────────────────────────
    // non-coro TestCase: no coroutine suspension needed
    TestCase(main_thread_id_matches_creator)
    {
        AssertThat(loop->main_thread_id(), rpp::get_thread_id());
    }

    // ─── Constructor with explicit thread pool ──────────────────
    TestCaseCoro(custom_thread_pool)
    {
        rpp::thread_pool pool;
        loop = std::make_unique<rpp::event_loop>(0, &pool);

        std::atomic<bool> bg_ran_on_custom_pool{false};
        int result = co_await run_bg([&]() -> int {
            bg_ran_on_custom_pool.store(rpp::get_thread_id() != main_tid);
            return 7;
        });
        assert_on_loop_thread();
        AssertThat(result, 7);
        AssertThat(bg_ran_on_custom_pool.load(), true);
        AssertThat(loop->main_thread_id(), main_tid);
    }

    // ─── background_tasks / pending_completions count accessors ─
    // non-coro TestCase: inspects loop state at specific interleaving points
    TestCase(count_accessors)
    {
        AssertThat(loop->background_tasks(), 0);
        AssertThat(loop->pending_completions(), 0);

        std::atomic<bool> bg_started{false};
        std::atomic<bool> bg_may_finish{false};

        auto coro = [&]() -> rpp::cfuture<void>
        {
            co_await run_bg([&]() {
                bg_started.store(true);
                while (!bg_may_finish.load())
                    rpp::sleep_ms(1);
            });
            co_return;
        };

        auto future = coro();

        // wait for bg task to start
        while (!bg_started.load())
            rpp::sleep_ms(1);

        AssertGreater(loop->background_tasks(), 0);

        // let it finish and give time for the resume event to arrive
        bg_may_finish.store(true);
        rpp::sleep_ms(15);

        // the background task finished: the resume event should be pending
        // and background_tasks should drop back to 0
        AssertThat(loop->background_tasks(), 0);
        AssertGreater(loop->pending_completions(), 0);

        idle();
        future.get();

        AssertThat(loop->background_tasks(), 0);
        AssertThat(loop->pending_completions(), 0);
    }

    // ─── post_resume: direct coroutine handle posting ───────────
    TestCaseCoro(post_resume_direct)
    {
        struct manual_suspend
        {
            event_loop& loop;
            // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
            bool await_ready() const noexcept { return false; }
            void await_suspend(rpp::coro_handle<> h) noexcept
            {
                rpp::parallel_task_detached([this, h]() {
                    rpp::sleep_ms(5);
                    loop.post_resume(h);
                });
            }
            void await_resume() const noexcept {}
        };
        co_await manual_suspend{*loop};
        assert_on_loop_thread();
    }

    // ─── Multiple posted callbacks execute in FIFO order ────────
    // non-coro TestCase: no coroutine suspension needed
    TestCase(post_callbacks_fifo_order)
    {
        std::vector<int> order;
        for (int i = 0; i < 5; ++i)
        {
            loop->post([&order, i]() {
                order.push_back(i);
            });
        }
        idle();
        AssertThat(order.size(), 5u);
        for (int i = 0; i < 5; ++i)
            AssertThat(order[i], i);
    }

    // ─── wait_on_all drains pending work ────────────────────────
    // non-coro TestCase: tests wait_on_all() API directly
    TestCase(wait_on_all_drains)
    {
        std::atomic<bool> callback_ran{false};
        auto coro = [&]() -> rpp::cfuture<void>
        {
            co_await run_bg([&]() {
                rpp::sleep_ms(5);
                callback_ran.store(true);
            });
            co_return;
        };
        auto future = coro();

        // use wait_on_all instead of run_until_idle
        bool done = loop->wait_on_all(rpp::millis(500));

        AssertThat(done, true);
        AssertThat(callback_ran.load(), true);
        // future should be ready
        future.get();
    }

    // NOLINTEND(cppcoreguidelines-avoid-capturing-lambda-coroutines)

#endif // RPP_HAS_COROUTINES
};
