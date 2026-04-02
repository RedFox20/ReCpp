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

    void assert_on_main_thread(RPP_SOURCE_LOC) { AssertThatLoc(loc, rpp::get_thread_id(), main_tid); }
    void idle() const { loop->run_until_idle(); }

    // NOLINTBEGIN(cppcoreguidelines-avoid-capturing-lambda-coroutines)

    // ─── Showcase: event_loop API overview ──────────────────────
    //   1. run_async()  – dispatch work to a background thread, co_await
    //                     the result; coroutine resumes on the main thread.
    //   2. post()       – schedule a callback to run on the main (UI) thread
    //                     from anywhere; useful for pushing results back to
    //                     the UI without data-race concerns.
    //   3. After every co_await the coroutine is guaranteed to resume on
    //      the main thread – verified with assert_on_main_thread().
    TestCaseCoro(api_showcase)
    {
        // ── 1. Run heavy work on a background thread ────────────────
        // run_async() dispatches the lambda to the thread pool and suspends
        // this coroutine.  When the lambda finishes, the event loop resumes
        // the coroutine on the main thread so we can safely touch UI state.
        int answer = co_await loop->run_async([]() -> int {
            rpp::sleep_ms(5); // simulate slow computation
            return 42;
        });
        assert_on_main_thread(); // coroutine resumed on main thread
        AssertThat(answer, 42);

        // ── 2. Await a lambda coroutine ─────────────────────────────
        // Lambda coroutines compose naturally: co_await a cfuture-returning
        // lambda just like co_await run_async.  The lambda does its own
        // run_async internally and resumes on the main thread.
        std::string msg = co_await loop->run_async([this,answer]() -> rpp::cfuture<std::string> {
            std::string raw = co_await loop->run_async([answer]() -> std::string {
                rpp::sleep_ms(5); // simulate slow I/O
                return "result_" + std::to_string(answer);
            });
            assert_on_main_thread(); // sub-coroutine also resumes on main thread
            co_return raw;
        });
        assert_on_main_thread(); // still on the main thread after resume
        AssertThat(msg, "result_42"s);

        // ── 3. post() – push a callback onto the main thread queue ──
        // post() enqueues a callable that will be executed on the main
        // (event-loop) thread during the next idle() / run_once() cycle.
        // This is the primary way to marshal work back to the UI thread
        // from arbitrary contexts (background threads, callbacks, etc.).
        bool posted_callback_ran = false;
        loop->post([&]() {
            assert_on_main_thread(); // runs on main thread
            posted_callback_ran = true;
        });

        // The posted callback hasn't run yet – it is just queued.
        // A third async call will give the loop a chance to drain the
        // queue between processing resume events.
        int final_val = co_await loop->run_async([]() -> int {
            rpp::sleep_ms(2);
            return 7;
        });
        assert_on_main_thread();
        AssertThat(final_val, 7);

        // By now loop->run_once() inside run_coro_test has processed the posted
        // callback alongside the coroutine resume events.
        AssertThat(posted_callback_ran, true);
    }

    // ─── Showcase: event_task as a top-level coroutine ──────────
    // event_task is a lightweight coroutine return type designed for
    // event_loop-driven coroutines.  Unlike cfuture<T>, it does not
    // carry a return value — it is fire-and-forget with exception
    // propagation.  The event loop drives it via run_until_idle().
    TestCase(event_task_showcase)
    {
        std::vector<std::string> log;

        // event_task coroutine: starts eagerly, suspends at each co_await,
        // and the event loop resumes it on the main thread.
        auto my_task = [&](rpp::event_loop* ev) -> rpp::event_task
        {
            // 1. Dispatch work to background, resume on main thread
            int val = co_await ev->run_async([]() -> int {
                rpp::sleep_ms(5);
                return 42;
            });
            assert_on_main_thread(); // coroutine resumed on main thread
            log.emplace_back("step1: " + std::to_string(val));

            // 2. Await a lambda coroutine — composes naturally with event_task.
            //    The lambda does its own run_async and resumes on main thread.
            std::string msg = co_await ev->run_async([this,ev,val]() -> rpp::cfuture<std::string> {
                std::string raw = co_await ev->run_async([val]() -> std::string {
                    rpp::sleep_ms(2);
                    return "result_" + std::to_string(val);
                });
                assert_on_main_thread(); // sub-coroutine also resumes on main thread
                co_return raw;
            });
            assert_on_main_thread(); // still on main thread after lambda coroutine
            log.emplace_back("step2: " + msg);

            // 3. Post a callback to the main thread queue
            ev->post([&]() {
                log.emplace_back("posted_callback");
            });
        };

        auto task = my_task(loop.get()); // starts immediately, suspends at first co_await
        AssertThat(task.done(), false);

        loop->run_until_done(task); // drives the loop and rethrows on failure
        AssertThat(task.done(), true);

        AssertThat(log.size(), 3u);
        AssertThat(log[0], "step1: 42"s);
        AssertThat(log[1], "step2: result_42"s);
        AssertThat(log[2], "posted_callback"s);
    }


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

    // ─── Basic: run_async with return value ────────────────
    TestCaseCoro(basic_run_async)
    {
        std::string result = co_await loop->run_async([main_tid=main_tid]() -> std::string
        {
            AssertNotEqual(rpp::get_thread_id(), main_tid);
            rpp::sleep_ms(5);
            return "hello from background";
        });
        assert_on_main_thread();
        AssertThat(result, "hello from background"s);
    }

    // ─── Basic: run_async<void> ────────────────────────────
    TestCaseCoro(basic_run_async_void)
    {
        std::atomic<bool> work_done{false};
        co_await loop->run_async([&]() {
            rpp::sleep_ms(5);
            work_done = true;
        });
        assert_on_main_thread();
        AssertThat(work_done.load(), true);
    }

    // ─── Multi-stage coroutine: all resumes on loop thread ──────
    TestCaseCoro(multi_stage_resumes_on_loop_thread)
    {
        thread_recorder rec {};
        std::string s;

        s += co_await loop->run_async([&]() -> std::string {
            rpp::sleep_ms(2);
            return "A";
        });
        rec.record();

        s += co_await loop->run_async([&]() -> std::string {
            rpp::sleep_ms(2);
            return "B";
        });
        rec.record();

        s += co_await loop->run_async([&]() -> std::string {
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
            assert_on_main_thread();
            std::lock_guard lock{order_m};
            resume_order.push_back(id);
        };

        // Deterministic ordering strategy:
        //   B does its short bg work and resumes on the loop thread first.
        //   B sets b_resumed=true from the loop thread (inside its coroutine body,
        //   AFTER its resume event has been fully processed by idle()).
        //   A's bg task spins on b_resumed, so it only exits AFTER B's resume has
        //   been fully processed and is no longer racing for the queue head.
        //   This avoids any reliance on wall-clock timing.
        std::atomic<bool> b_resumed{false};

        auto coro_a = [&]() -> rpp::cfuture<int>
        {
            co_await loop->run_async([&] {
                while (!b_resumed.load()) rpp::sleep_ms(1);
            });
            record(1);
            co_return 10;
        };

        auto coro_b = [&]() -> rpp::cfuture<int>
        {
            co_await loop->run_async([&] { rpp::sleep_ms(2); });
            // On loop thread: signal A's bg task that B has been resumed.
            // A's post_resume will land in the queue only after this point.
            b_resumed.store(true);
            record(2);
            co_return 20;
        };

        auto fa = coro_a();
        auto fb = coro_b();
        idle();

        AssertThat(fa.get(), 10);
        AssertThat(fb.get(), 20);
        AssertThat(resume_order.size(), 2u);
        // B was resumed and set b_resumed before A could post its resume → B first
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
            std::string result = co_await loop->run_async([&]() -> std::string {
                slow_bg_running.store(true);
                rpp::sleep_ms(50);
                return "slow";
            });
            assert_on_main_thread();
            co_return result;
        };

        auto coro_fast = [&]() -> rpp::cfuture<std::string>
        {
            // Wait in background until the slow task has confirmed it is running
            // before proceeding.  This makes the assertion deterministic: we no
            // longer rely on wall-clock timing to observe slow_bg_running == true.
            co_await loop->run_async([&]() {
                while (!slow_bg_running.load()) rpp::sleep_ms(1);
                rpp::sleep_ms(2);
            });
            fast_resume_count.fetch_add(1);
            assert_on_main_thread();

            co_await loop->run_async([&]() {
                rpp::sleep_ms(2);
            });
            fast_resume_count.fetch_add(1);
            assert_on_main_thread();

            // slow task sleeps for 50ms total; we've only spent ~4ms since it
            // started, so it must still be running.
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
            co_await loop->run_async([&]() {
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
        std::string s = co_await loop->run_async([]() -> std::string {
            return "step1";
        });
        assert_on_main_thread();

        bool caught_ex = false;
        try
        {
            co_await loop->run_async([]() {
                throw std::runtime_error{"step2 failed"};
            });
        }
        catch (const std::runtime_error&)
        {
            caught_ex = true;
        }
        AssertThat(caught_ex, true);
        assert_on_main_thread();

        s += co_await loop->run_async([]() -> std::string {
            return "_step3";
        });
        assert_on_main_thread();

        AssertThat(s, "step1_step3"s);
    }

    // ─── delay() awaiter resumes on loop thread ─────────────────
    TestCaseCoro(delay_resumes_on_loop_thread)
    {
        rpp::Timer t;
        co_await loop->delay(rpp::millis(20));
        assert_on_main_thread();
        AssertGreater(t.elapsed_millis(), 18.0);
    }

    // ─── run_once with timeout ──────────────────────────────────
    // non-coro TestCase: tests run_once() API directly
    TestCase(run_once_basic)
    {
        std::atomic<int> resume_count{0};
        auto coro = [&]() -> rpp::cfuture<int>
        {
            int v = co_await loop->run_async([]() -> int {
                rpp::sleep_ms(5);
                return 42;
            });
            resume_count.fetch_add(1);
            assert_on_main_thread();
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
            assert_on_main_thread();
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
            int result = co_await loop->run_async([id]() -> int {
                rpp::sleep_ms(1 + (id % 5));
                return id * 10;
            });

            total_resumes.fetch_add(1);
            assert_on_main_thread();

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
                int v = co_await loop->run_async([id, stage]() -> int {
                    rpp::sleep_ms(1);
                    return id * 100 + stage;
                });
                accum += v;

                total_resumes.fetch_add(1);
                assert_on_main_thread();
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
            co_await loop->run_async([&]() {
                if (rpp::get_thread_id() != main_tid)
                    bg_on_different_thread.fetch_add(1);
                rpp::sleep_ms(1);
            });
            assert_on_main_thread();
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
            co_await loop->run_async([&]() {
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
            log->emplace_back("callback_1");
            assert_on_main_thread();
        });

        auto coro = [&]() -> rpp::cfuture<void>
        {
            co_await loop->run_async([&]() {
                rpp::sleep_ms(5);
            });
            log->emplace_back("coro_resume");
            assert_on_main_thread();
            co_return;
        };

        auto future = coro();
        loop->post([&]() {
            log->emplace_back("callback_2");
            assert_on_main_thread();
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
            co_return co_await loop->run_async([]() -> int {
                rpp::sleep_ms(5);
                return 99;
            });
        };
        auto produced_future = producer();
        int val = co_await loop->run_async(produced_future);
        assert_on_main_thread();
        AssertThat(val, 99);
    }

    // ─── await_future: already-ready future resumes immediately ─
    TestCaseCoro(await_future_already_ready)
    {
        std::promise<int> prom;
        auto std_future = prom.get_future();
        prom.set_value(42);

        rpp::cfuture<int> ready_future {std::move(std_future)};

        int val = co_await loop->run_async(ready_future);
        assert_on_main_thread();
        AssertThat(val, 42);
    }

    // ─── delay_until: time-point based delay ────────────────────
    TestCaseCoro(delay_until_resumes_on_loop_thread)
    {
        rpp::Timer t;
        rpp::TimePoint target = rpp::TimePoint::monotonic_now() + rpp::millis(20);
        co_await loop->delay_until(target);
        assert_on_main_thread();
        AssertGreater(t.elapsed_millis(), 18.0);
    }

    // ─── set_except_handler: custom exception handling ──────────
    // non-coro TestCase: no coroutine suspension needed
    TestCase(set_except_handler_catches_callback_exception)
    {
        std::string caught_message;
        loop->set_except_handler([&](const std::exception_ptr& ep)
        {
            try { std::rethrow_exception(ep); }
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
            co_await loop->run_async([&]() {
                rpp::sleep_ms(5);
            });
            resumes.fetch_add(1);
            assert_on_main_thread();

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
        int result = co_await loop->run_async([&]() -> int {
            bg_ran_on_custom_pool.store(rpp::get_thread_id() != main_tid);
            return 7;
        });
        assert_on_main_thread();
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
            co_await loop->run_async([&]() {
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

        // let it finish, then spin until the awaiter has posted the resume event.
        // A bare sleep_ms(N) would be a race: the bg thread must finish its work,
        // call fetch_sub, and call post_resume before we sample the counters.
        bg_may_finish.store(true);
        while (loop->pending_completions() == 0)
            rpp::sleep_ms(1);

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
        assert_on_main_thread();
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
            co_await loop->run_async([&]() {
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

    // ─── UI simulation using fork(): fire-and-forget concurrency ──
    //
    // Same scenario as before — two serialized execution paths interleaving
    // on the event loop — but using fork() for cleaner structured concurrency.
    //
    //   Path 1 – message_processor: 4 incoming UI commands, each parsed on
    //            a background thread (8 ms), committed on the main thread.
    //   Path 2 – video_pipeline: 5 pipeline stages with varying durations.
    //
    // fork() starts each coroutine eagerly on the main thread and tracks it
    // internally.  run_until_idle() drives all forks to completion and
    // automatically cleans up completed forks (exceptions go through
    // except_handler).
    //
    // Verified properties:
    //   1. Within each path the steps are ordered (sequential co_await chain).
    //   2. The two paths run concurrently (interleaved on the event loop).
    //   3. All shared-state mutations happen on the loop thread (asserted).
    //   4. At least one message is committed before the first video stage.
    TestCase(ui_simulation_with_fork)
    {
        // ── Shared state – written ONLY on the event loop thread ─────────
        // No mutex needed: the event loop serialises all resumes onto a
        // single thread, giving us thread-safe access "for free".
        struct SharedState
        {
            std::vector<std::string> messages;      // processed messages in arrival order
            std::vector<std::string> pipeline_log;  // pipeline stage names in stage order
            std::vector<std::string> timeline;      // interleaved event log (both paths)
            int total_frames = 0;
            bool pipeline_active = false;
        };
        SharedState state;

        const std::vector<std::string> raw_messages = {
            "connect_camera",
            "set_resolution_720p",
            "start_recording",
            "apply_filter_blur",
        };

        // ── Fork path 1: message processor ──────────────────────────────
        loop->fork([&]() -> rpp::event_task
        {
            for (const std::string& msg : raw_messages)
            {
                // background: slow parse / validation (8 ms per message)
                std::string result = co_await loop->run_async([msg]() -> std::string {
                    rpp::sleep_ms(8);
                    return "cmd:" + msg;
                });

                // main thread: commit result to shared state (no lock needed)
                assert_on_main_thread();
                state.messages.push_back(result);
                state.timeline.push_back("msg:" + msg);
            }
        });
        assert_on_main_thread();

        // ── Fork path 2: video pipeline ─────────────────────────────────
        // Stage timings (background sleep):
        //   init       20 ms  → message_processor should commit ~2 messages first
        //   add_source 15 ms
        //   start      25 ms
        //   frames ×3  10 ms each
        //   teardown   15 ms
        loop->fork([&]() -> rpp::event_task
        {
            // Stage 1 – init (20 ms)
            co_await loop->run_async([] { rpp::sleep_ms(20); });
            assert_on_main_thread();
            state.pipeline_active = true;
            state.pipeline_log.emplace_back("init");
            state.timeline.emplace_back("vid:init");

            // Stage 2 – add video source (15 ms)
            co_await loop->run_async([] { rpp::sleep_ms(15); });
            assert_on_main_thread();
            state.pipeline_log.emplace_back("add_source");
            state.timeline.emplace_back("vid:add_source");

            // Stage 3 – start pipeline (25 ms)
            co_await loop->run_async([] { rpp::sleep_ms(25); });
            assert_on_main_thread();
            state.pipeline_log.emplace_back("start");
            state.timeline.emplace_back("vid:start");

            // Stage 4 – process 3 frame batches (10 ms each)
            for (int batch = 0; batch < 3; ++batch)
            {
                int frames = co_await loop->run_async([batch]() -> int {
                    rpp::sleep_ms(10);
                    return (batch + 1) * 10; // 10, 20, 30
                });
                assert_on_main_thread();
                state.total_frames += frames;
                state.pipeline_log.emplace_back("frames_" + std::to_string(batch));
                state.timeline.emplace_back("vid:frames_" + std::to_string(batch));
            }

            // Stage 5 – teardown (15 ms)
            co_await loop->run_async([] { rpp::sleep_ms(15); });
            assert_on_main_thread();
            state.pipeline_active = false;
            state.pipeline_log.emplace_back("teardown");
            state.timeline.emplace_back("vid:teardown");
        });
        assert_on_main_thread();

        // ── Drive the event loop until all forks complete ────────────────
        // run_until_idle() automatically cleans up completed forks
        AssertThat(loop->num_forks(), 2);
        idle();

        // ── Verify message path: order preserved ─────────────────────────
        AssertThat(state.messages.size(), 4u);
        AssertThat(state.messages[0], "cmd:connect_camera"s);
        AssertThat(state.messages[1], "cmd:set_resolution_720p"s);
        AssertThat(state.messages[2], "cmd:start_recording"s);
        AssertThat(state.messages[3], "cmd:apply_filter_blur"s);

        // ── Verify video path: stage order preserved ──────────────────────
        AssertThat(state.pipeline_log.size(), 7u);
        AssertThat(state.pipeline_log[0], "init"s);
        AssertThat(state.pipeline_log[1], "add_source"s);
        AssertThat(state.pipeline_log[2], "start"s);
        AssertThat(state.pipeline_log[3], "frames_0"s);
        AssertThat(state.pipeline_log[4], "frames_1"s);
        AssertThat(state.pipeline_log[5], "frames_2"s);
        AssertThat(state.pipeline_log[6], "teardown"s);
        AssertThat(state.total_frames, 60); // 10 + 20 + 30
        AssertThat(state.pipeline_active, false);

        // ── Verify interleaving: both paths progressed concurrently ───────
        AssertThat(state.timeline.size(), 11u);

        int msg_events = 0;
        int vid_events = 0;
        for (const auto& e : state.timeline)
        {
            if (e.starts_with("msg:")) ++msg_events;
            if (e.starts_with("vid:")) ++vid_events;
        }
        AssertThat(msg_events, 4);
        AssertThat(vid_events, 7);

        // At least one message committed before the first video resume.
        // (2 × 8 ms < 20 ms, so ~2 in practice; we guard conservatively.)
        size_t first_vid_pos = state.timeline.size();
        for (size_t i = 0; i < state.timeline.size(); ++i)
            if (state.timeline[i].starts_with("vid:")) { first_vid_pos = i; break; }

        int msgs_before_first_vid = 0;
        for (size_t i = 0; i < first_vid_pos; ++i)
            if (state.timeline[i].starts_with("msg:")) ++msgs_before_first_vid;

        AssertGreater(msgs_before_first_vid, 0);
        AssertThat(loop->num_forks(), 0); // all forks completed
    }

    // ─── join_forks with timeout: soft deadlock prevention ──────
    //
    // Demonstrates the timeout pattern for handling slow forks.
    // A fast fork completes within the timeout; a slow fork does not.
    // join_forks() returns the number of forks still active, allowing
    // the caller to continue with partial results instead of hanging.
    TestCase(fork_join_with_timeout)
    {
        auto main_task = [&]() -> rpp::event_task
        {
            std::atomic<bool> slow_started{false};

            // fast fork: completes quickly
            loop->fork([&]() -> rpp::event_task
            {
                co_await loop->run_async([] { rpp::sleep_ms(10); });
                assert_on_main_thread();
            });
            assert_on_main_thread();

            // slow fork: takes much longer than our timeout
            loop->fork([&]() -> rpp::event_task
            {
                slow_started.store(true);
                co_await loop->run_async([] { rpp::sleep_ms(100); });
                assert_on_main_thread();
            });
            assert_on_main_thread();

            AssertThat(loop->num_forks(), 2);

            // join with a short timeout — fast fork finishes, slow fork does not
            int remaining = co_await loop->join_forks(rpp::millis(30));
            assert_on_main_thread();
            AssertGreater(remaining, 0);      // at least one fork still active
            AssertThat(slow_started.load(), true); // slow fork did start
        };

        auto task = main_task();
        loop->run_until_done(task);

        // let the slow fork complete so we don't leak background work
        // run_until_idle() automatically cleans up completed forks
        idle();
    }

    // NOLINTEND(cppcoreguidelines-avoid-capturing-lambda-coroutines)

#endif // RPP_HAS_COROUTINES
};
