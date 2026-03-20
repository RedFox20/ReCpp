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
    TestCaseCleanup()
    {
        loop.reset();
    }

    // NOLINTBEGIN(cppcoreguidelines-avoid-capturing-lambda-coroutines)

    void assert_on_loop_thread(std::source_location loc = std::source_location::current())
    { AssertThatLoc(loc, rpp::get_thread_id(), main_tid); }

    void idle() const { loop->run_until_idle(); }

    template<typename F>
    auto run_coro(F&& f) { auto future = f(); idle(); return future; }

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
        auto future = run_coro([&]() -> rpp::cfuture<std::string>
        {
            std::string result = co_await run_bg([main_tid=main_tid]() -> std::string
            {
                AssertNotEqual(rpp::get_thread_id(), main_tid);
                rpp::sleep_ms(5);
                return "hello from background";
            });
            assert_on_loop_thread();
            co_return result;
        });

        std::string result = co_await future;
        AssertThat(result, "hello from background"s);
        assert_on_loop_thread(); // still on same thread after co_await future
    }

    // ─── Basic: run_background<void> ────────────────────────────

    TestCaseCoro(basic_run_background_void)
    {
        std::atomic<bool> work_done{false};

        auto future = run_coro([&]() -> rpp::cfuture<void>
        {
            co_await run_bg([&]() {
                rpp::sleep_ms(5);
                work_done = true;
            });
            assert_on_loop_thread();
            co_return;
        });

        co_await future;
        AssertThat(work_done.load(), true);
        assert_on_loop_thread(); // still on same thread after co_await future
    }

    // ─── Multi-stage coroutine: all resumes on loop thread ──────

    TestCaseCoro(multi_stage_resumes_on_loop_thread)
    {
        thread_recorder rec {};

        auto future = run_coro([&]() -> rpp::cfuture<std::string>
        {
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

            co_return s;
        });

        std::string result = co_await future;
        AssertThat(result, "ABC"s);
        AssertThat(rec.count(), 3u);
        AssertThat(rec.all_on_loop_thread(), true);
    }

    // ─── Multiple independent coroutines interleave on the loop ─

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

    TestCase(exception_propagation)
    {
        auto future = run_coro([&]() -> rpp::cfuture<std::string>
        {
            co_await run_bg([&]() {
                throw std::runtime_error{"background error"};
            });
            co_return "should not reach here";
        });

        bool caught = false;
        try
        {
            future.get();
        }
        catch (const std::runtime_error& e)
        {
            caught = true;
            AssertThat(std::string(e.what()), "background error"s);
        }
        AssertThat(caught, true);
    }

    // ─── Exception in middle of multi-stage, with recovery ──────

    TestCase(exception_recovery_multi_stage)
    {
        auto future = run_coro([&]() -> rpp::cfuture<std::string>
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

            co_return s;
        });

        AssertThat(future.get(), "step1_step3"s);
    }

    // ─── delay() awaiter resumes on loop thread ─────────────────

    TestCase(delay_resumes_on_loop_thread)
    {
        auto future = run_coro([&]() -> rpp::cfuture<void>
        {
            rpp::Timer t;
            co_await loop->delay(rpp::millis(20));
            assert_on_loop_thread();
            AssertGreater(t.elapsed_millis(), 18.0);
            co_return;
        });
        future.get();
    }

    // ─── run_once with timeout ──────────────────────────────────

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

    TestCase(background_work_on_different_thread)
    {
        std::atomic<int> bg_on_different_thread{0};

        auto future = run_coro([&]() -> rpp::cfuture<void>
        {
            for (int i = 0; i < 5; ++i)
            {
                co_await run_bg([&]() {
                    if (rpp::get_thread_id() != main_tid)
                        bg_on_different_thread.fetch_add(1);
                    rpp::sleep_ms(1);
                });
                assert_on_loop_thread();
            }
            co_return;
        });
        future.get();

        AssertThat(bg_on_different_thread.load(), 5);
    }

    // ─── Pending work tracking ──────────────────────────────────

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

    TestCase(mixed_coroutines_and_callbacks)
    {
        std::vector<std::string> execution_log;
        rpp::mutex log_m;

        auto log = [&](std::string msg)
        {
            std::lock_guard lock{log_m};
            execution_log.push_back(std::move(msg));
        };

        loop->post([&]() {
            log("callback_1");
            assert_on_loop_thread();
        });

        auto coro = [&]() -> rpp::cfuture<void>
        {
            co_await run_bg([&]() {
                rpp::sleep_ms(5);
            });
            log("coro_resume");
            assert_on_loop_thread();
            co_return;
        };

        auto future = coro();

        loop->post([&]() {
            log("callback_2");
            assert_on_loop_thread();
        });

        idle();
        future.get();

        {
            std::lock_guard lock{log_m};
            AssertThat(execution_log.size(), 3u);
            AssertThat(rpp::contains(execution_log, "callback_1"), true);
            AssertThat(rpp::contains(execution_log, "callback_2"), true);
            AssertThat(rpp::contains(execution_log, "coro_resume"), true);
        }
    }

    // ─── await_future: wait for a cfuture on event loop ─────────

    TestCase(await_future_basic)
    {
        auto producer = [&]() -> rpp::cfuture<int>
        {
            co_return co_await run_bg([]() -> int {
                rpp::sleep_ms(5);
                return 99;
            });
        };

        auto produced_future = producer();

        auto result_future = run_coro([&]() -> rpp::cfuture<int>
        {
            int val = co_await loop->await_future(produced_future);
            assert_on_loop_thread();
            co_return val + 1;
        });

        AssertThat(result_future.get(), 100);
    }

    // ─── await_future: already-ready future resumes immediately ─

    TestCase(await_future_already_ready)
    {
        std::promise<int> prom;
        auto std_future = prom.get_future();
        prom.set_value(42);

        rpp::cfuture<int> ready_future {std::move(std_future)};

        auto result = run_coro([&]() -> rpp::cfuture<int>
        {
            int val = co_await loop->await_future(ready_future);
            assert_on_loop_thread();
            co_return val;
        });

        AssertThat(result.get(), 42);
    }

    // ─── delay_until: time-point based delay ────────────────────

    TestCaseCoro(delay_until_resumes_on_loop_thread)
    {
        auto future = run_coro([&]() -> rpp::cfuture<void>
        {
            rpp::Timer t;
            rpp::TimePoint target = rpp::TimePoint::now() + rpp::millis(20);
            co_await loop->delay_until(target);
            assert_on_loop_thread();
            AssertGreater(t.elapsed_millis(), 18.0);
            co_return;
        });
        co_await future;
    }

    // ─── set_except_handler: custom exception handling ──────────

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

    TestCase(main_thread_id_matches_creator)
    {
        AssertThat(loop->main_thread_id(), rpp::get_thread_id());
    }

    // ─── Constructor with explicit thread pool ──────────────────

    TestCase(custom_thread_pool)
    {
        rpp::thread_pool pool;
        loop = std::make_unique<rpp::event_loop>(0, &pool);

        std::atomic<bool> bg_ran_on_custom_pool{false};

        auto future = run_coro([&]() -> rpp::cfuture<int>
        {
            int result = co_await run_bg([&]() -> int {
                bg_ran_on_custom_pool.store(rpp::get_thread_id() != main_tid);
                return 7;
            });
            assert_on_loop_thread();
            co_return result;
        });

        AssertThat(future.get(), 7);
        AssertThat(bg_ran_on_custom_pool.load(), true);
        AssertThat(loop->main_thread_id(), main_tid);
    }

    // ─── background_tasks / pending_completions count accessors ─

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

    TestCase(post_resume_direct)
    {
        bool resumed = false;

        auto future = run_coro([&]() -> rpp::cfuture<void>
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
            resumed = true;
            assert_on_loop_thread();
            co_return;
        });
        future.get();

        AssertThat(resumed, true);
    }

    // ─── Multiple posted callbacks execute in FIFO order ────────

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
        {
            AssertThat(order[i], i);
        }
    }

    // ─── wait_on_all drains pending work ────────────────────────

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
