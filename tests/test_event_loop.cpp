#include <rpp/event_loop.h>
#include <rpp/coroutines.h>
#include <rpp/future.h>
#include <rpp/thread_pool.h>
#include <rpp/timer.h>
#include <rpp/threads.h>

#include <rpp/tests.h>
#include <atomic>
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

    // ─── Helper: record which thread resumed each step ──────────

    struct thread_recorder
    {
        rpp::mutex m;
        std::vector<uint64> resume_thread_ids;
        uint64 loop_thread = 0;

        void record()
        {
            std::lock_guard lock{m};
            resume_thread_ids.push_back(rpp::get_thread_id());
        }

        void set_loop_thread()
        {
            loop_thread = rpp::get_thread_id();
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

    TestCase(basic_run_background)
    {
        rpp::event_loop loop;
        uint64 main_tid = rpp::get_thread_id();
        uint64 resume_tid = 0;

        auto coro = [&]() -> rpp::cfuture<std::string>
        {
            std::string result = co_await loop.run_background([&]() -> std::string {
                // this should run on a worker thread, not the main thread
                AssertNotEqual(rpp::get_thread_id(), main_tid);
                rpp::sleep_ms(5);
                return "hello from background";
            });
            // after co_await, we should be back on the event loop thread
            resume_tid = rpp::get_thread_id();
            co_return result;
        };

        auto future = coro();
        loop.run_loop();
        std::string result = future.get();

        AssertThat(result, "hello from background"s);
        AssertThat(resume_tid, main_tid);
    }

    // ─── Basic: run_background<void> ────────────────────────────

    TestCase(basic_run_background_void)
    {
        rpp::event_loop loop;
        uint64 main_tid = rpp::get_thread_id();
        std::atomic<bool> work_done{false};
        uint64 resume_tid = 0;

        auto coro = [&]() -> rpp::cfuture<void>
        {
            co_await loop.run_background([&]() {
                rpp::sleep_ms(5);
                work_done.store(true);
            });
            resume_tid = rpp::get_thread_id();
            co_return;
        };

        auto future = coro();
        loop.run_loop();
        future.get();

        AssertThat(work_done.load(), true);
        AssertThat(resume_tid, main_tid);
    }

    // ─── Multi-stage coroutine: all resumes on loop thread ──────

    TestCase(multi_stage_resumes_on_loop_thread)
    {
        rpp::event_loop loop;
        thread_recorder rec;
        rec.set_loop_thread();

        auto coro = [&]() -> rpp::cfuture<std::string>
        {
            std::string s;

            s += co_await loop.run_background([&]() -> std::string {
                rpp::sleep_ms(2);
                return "A";
            });
            rec.record(); // should be on loop thread

            s += co_await loop.run_background([&]() -> std::string {
                rpp::sleep_ms(2);
                return "B";
            });
            rec.record(); // should be on loop thread

            s += co_await loop.run_background([&]() -> std::string {
                rpp::sleep_ms(2);
                return "C";
            });
            rec.record(); // should be on loop thread

            co_return s;
        };

        auto future = coro();
        loop.run_loop();
        std::string result = future.get();

        AssertThat(result, "ABC"s);
        AssertThat(rec.count(), 3u);
        AssertThat(rec.all_on_loop_thread(), true);
    }

    // ─── Multiple independent coroutines interleave on the loop ─

    TestCase(multiple_coroutines_interleave)
    {
        rpp::event_loop loop;
        uint64 main_tid = rpp::get_thread_id();

        // Track the order of resumes to verify interleaving is possible
        rpp::mutex order_m;
        std::vector<int> resume_order;
        std::vector<uint64> resume_tids;

        auto record = [&](int id)
        {
            std::lock_guard lock{order_m};
            resume_order.push_back(id);
            resume_tids.push_back(rpp::get_thread_id());
        };

        auto coro_a = [&]() -> rpp::cfuture<int>
        {
            co_await loop.run_background([&] {
                rpp::sleep_ms(15); // slower task
            });
            record(1); // coro A resumed
            co_return 10;
        };

        auto coro_b = [&]() -> rpp::cfuture<int>
        {
            co_await loop.run_background([&] {
                rpp::sleep_ms(2); // faster task
            });
            record(2); // coro B resumed
            co_return 20;
        };

        auto fa = coro_a();
        auto fb = coro_b();
        loop.run_loop();

        int ra = fa.get();
        int rb = fb.get();

        AssertThat(ra, 10);
        AssertThat(rb, 20);

        // Both resumes happened
        AssertThat(resume_order.size(), 2u);

        // All resumes were on the main/loop thread
        for (auto tid : resume_tids)
        {
            AssertThat(tid, main_tid);
        }

        // B should have finished first since it's faster
        // (not a hard requirement per spec, but highly likely)
        if (resume_order.size() == 2u)
        {
            print_info("resume order: %d, %d (expected 2, 1)\n",
                       resume_order[0], resume_order[1]);
        }
    }

    // ─── Suspended tasks are paused while other tasks resume ────

    TestCase(suspended_tasks_paused_while_others_resume)
    {
        rpp::event_loop loop;
        uint64 main_tid = rpp::get_thread_id();

        // This test verifies the core requirement:
        // while coro_slow is suspended (waiting on a long background task),
        // coro_fast can be resumed on the loop thread.

        std::atomic<int> fast_resume_count{0};
        std::atomic<bool> slow_bg_running{false};
        std::atomic<bool> fast_completed_while_slow_suspended{false};

        auto coro_slow = [&]() -> rpp::cfuture<std::string>
        {
            std::string result = co_await loop.run_background([&]() -> std::string {
                slow_bg_running.store(true);
                rpp::sleep_ms(50); // long background task
                return "slow";
            });
            // by the time slow resumes, fast should have already completed
            AssertThat(rpp::get_thread_id(), main_tid);
            co_return result;
        };

        auto coro_fast = [&]() -> rpp::cfuture<std::string>
        {
            // first stage: quick background
            co_await loop.run_background([&]() {
                rpp::sleep_ms(2);
            });
            fast_resume_count.fetch_add(1);
            AssertThat(rpp::get_thread_id(), main_tid);

            // second stage: another quick background
            co_await loop.run_background([&]() {
                rpp::sleep_ms(2);
            });
            fast_resume_count.fetch_add(1);
            AssertThat(rpp::get_thread_id(), main_tid);

            // check that slow is still suspended (its bg task is still running)
            if (slow_bg_running.load())
                fast_completed_while_slow_suspended.store(true);

            co_return "fast";
        };

        auto fs = coro_slow();
        auto ff = coro_fast();
        loop.run_loop();

        std::string rs = fs.get();
        std::string rf = ff.get();

        AssertThat(rs, "slow"s);
        AssertThat(rf, "fast"s);
        AssertThat(fast_resume_count.load(), 2);
        AssertThat(fast_completed_while_slow_suspended.load(), true);
    }

    // ─── Exception propagation through event loop ───────────────

    TestCase(exception_propagation)
    {
        rpp::event_loop loop;

        auto coro = [&]() -> rpp::cfuture<std::string>
        {
            co_await loop.run_background([&]() {
                throw std::runtime_error{"background error"};
            });
            co_return "should not reach here";
        };

        auto future = coro();
        loop.run_loop();

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
        rpp::event_loop loop;
        uint64 main_tid = rpp::get_thread_id();

        auto coro = [&]() -> rpp::cfuture<std::string>
        {
            std::string s = co_await loop.run_background([]() -> std::string {
                return "step1";
            });
            AssertThat(rpp::get_thread_id(), main_tid);

            bool caught_ex = false;
            try
            {
                co_await loop.run_background([]() {
                    throw std::runtime_error{"step2 failed"};
                });
            }
            catch (const std::runtime_error&)
            {
                caught_ex = true;
            }
            AssertThat(caught_ex, true);
            AssertThat(rpp::get_thread_id(), main_tid);

            s += co_await loop.run_background([]() -> std::string {
                return "_step3";
            });
            AssertThat(rpp::get_thread_id(), main_tid);

            co_return s;
        };

        auto future = coro();
        loop.run_loop();

        AssertThat(future.get(), "step1_step3"s);
    }

    // ─── delay() awaiter resumes on loop thread ─────────────────

    TestCase(delay_resumes_on_loop_thread)
    {
        rpp::event_loop loop;
        uint64 main_tid = rpp::get_thread_id();
        uint64 resume_tid = 0;

        auto coro = [&]() -> rpp::cfuture<void>
        {
            rpp::Timer t;
            co_await loop.delay(std::chrono::milliseconds{20});
            resume_tid = rpp::get_thread_id();
            double elapsed = t.elapsed_millis();
            // allow some tolerance
            AssertGreater(elapsed, 18.0);
            co_return;
        };

        auto future = coro();
        loop.run_loop();
        future.get();

        AssertThat(resume_tid, main_tid);
    }

    // ─── run_once with timeout ──────────────────────────────────

    TestCase(run_once_basic)
    {
        rpp::event_loop loop;
        uint64 main_tid = rpp::get_thread_id();
        std::atomic<int> resume_count{0};

        auto coro = [&]() -> rpp::cfuture<int>
        {
            int v = co_await loop.run_background([]() -> int {
                rpp::sleep_ms(5);
                return 42;
            });
            resume_count.fetch_add(1);
            AssertThat(rpp::get_thread_id(), main_tid);
            co_return v;
        };

        auto future = coro();

        // poll until done
        rpp::Timer t;
        while (loop.has_pending_work() && t.elapsed_millis() < 500.0)
        {
            loop.run_once(rpp::Duration::from_millis(10));
        }

        AssertThat(resume_count.load(), 1);
        AssertThat(future.get(), 42);
    }

    // ─── run_once with zero timeout is non-blocking ─────────────

    TestCase(run_once_nonblocking)
    {
        rpp::event_loop loop;

        // nothing in the queue, zero timeout should return immediately
        rpp::Timer t;
        bool processed = loop.run_once(rpp::Duration::zero());
        double elapsed = t.elapsed_millis();

        AssertThat(processed, false);
        AssertLess(elapsed, 5.0); // should be essentially instant
    }

    // ─── post() generic callback ────────────────────────────────

    TestCase(post_generic_callback)
    {
        rpp::event_loop loop;
        int callback_value = 0;
        uint64 main_tid = rpp::get_thread_id();
        uint64 callback_tid = 0;

        loop.post([&]() {
            callback_value = 42;
            callback_tid = rpp::get_thread_id();
        });

        loop.run_loop();
        AssertThat(callback_value, 42);
        AssertThat(callback_tid, main_tid);
    }

    // ─── Stress: many coroutines, all resume on loop thread ─────

    TestCase(stress_many_coroutines_all_resume_on_loop)
    {
        rpp::event_loop loop;
        uint64 main_tid = rpp::get_thread_id();
        constexpr int N = 20;

        std::atomic<int> total_resumes{0};
        std::atomic<int> wrong_thread_resumes{0};

        auto make_coro = [&](int id) -> rpp::cfuture<int>
        {
            int result = co_await loop.run_background([id]() -> int {
                rpp::sleep_ms(1 + (id % 5)); // vary sleep times
                return id * 10;
            });

            total_resumes.fetch_add(1);
            if (rpp::get_thread_id() != main_tid)
                wrong_thread_resumes.fetch_add(1);

            co_return result;
        };

        std::vector<rpp::cfuture<int>> futures;
        futures.reserve(N);
        for (int i = 0; i < N; ++i)
        {
            futures.emplace_back(make_coro(i));
        }

        loop.run_loop();

        int sum = 0;
        for (int i = 0; i < N; ++i)
        {
            sum += futures[i].get();
        }

        // sum of i*10 for i=0..19 = 10*(0+1+...+19) = 10*190 = 1900
        AssertThat(sum, 1900);
        AssertThat(total_resumes.load(), N);
        AssertThat(wrong_thread_resumes.load(), 0);
    }

    // ─── Multi-stage stress: nested awaits all on loop thread ───

    TestCase(stress_multi_stage_coroutines)
    {
        rpp::event_loop loop;
        uint64 main_tid = rpp::get_thread_id();
        constexpr int N = 8;
        constexpr int STAGES = 4;

        std::atomic<int> total_resumes{0};
        std::atomic<int> wrong_thread_resumes{0};

        auto make_coro = [&](int id) -> rpp::cfuture<int>
        {
            int accum = 0;
            for (int stage = 0; stage < STAGES; ++stage)
            {
                int v = co_await loop.run_background([id, stage]() -> int {
                    rpp::sleep_ms(1);
                    return id * 100 + stage;
                });
                accum += v;

                total_resumes.fetch_add(1);
                if (rpp::get_thread_id() != main_tid)
                    wrong_thread_resumes.fetch_add(1);
            }
            co_return accum;
        };

        std::vector<rpp::cfuture<int>> futures;
        futures.reserve(N);
        for (int i = 0; i < N; ++i)
        {
            futures.emplace_back(make_coro(i));
        }

        loop.run_loop();

        for (int i = 0; i < N; ++i)
        {
            // each coro: sum(i*100 + stage) for stage 0..3 = 4*i*100 + 0+1+2+3 = 400*i + 6
            int expected = 400 * i + 6;
            AssertThat(futures[i].get(), expected);
        }

        AssertThat(total_resumes.load(), N * STAGES);
        AssertThat(wrong_thread_resumes.load(), 0);
    }

    // ─── Verify background work actually runs on worker threads ─

    TestCase(background_work_on_different_thread)
    {
        rpp::event_loop loop;
        uint64 main_tid = rpp::get_thread_id();

        std::atomic<int> bg_on_different_thread{0};

        auto coro = [&]() -> rpp::cfuture<void>
        {
            for (int i = 0; i < 5; ++i)
            {
                co_await loop.run_background([&]() {
                    if (rpp::get_thread_id() != main_tid)
                        bg_on_different_thread.fetch_add(1);
                    rpp::sleep_ms(1);
                });
                // resume should be on loop thread
                AssertThat(rpp::get_thread_id(), main_tid);
            }
            co_return;
        };

        auto future = coro();
        loop.run_loop();
        future.get();

        // all background tasks should have run on different threads
        AssertThat(bg_on_different_thread.load(), 5);
    }

    // ─── Pending work tracking ──────────────────────────────────

    TestCase(pending_work_tracking)
    {
        rpp::event_loop loop;

        // initially no pending work
        AssertThat(loop.has_pending_work(), false);
        AssertThat(loop.pending_tasks(), 0);

        std::atomic<bool> bg_started{false};
        std::atomic<bool> bg_may_finish{false};

        auto coro = [&]() -> rpp::cfuture<void>
        {
            co_await loop.run_background([&]() {
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
        AssertThat(loop.has_pending_work(), true);

        // let it finish
        bg_may_finish.store(true);

        loop.run_loop();
        future.get();

        // after run_loop completes, no more pending work
        AssertThat(loop.has_pending_work(), false);
    }

    // ─── Mixed: coroutines + posted callbacks ───────────────────

    TestCase(mixed_coroutines_and_callbacks)
    {
        rpp::event_loop loop;
        uint64 main_tid = rpp::get_thread_id();

        std::vector<std::string> execution_log;
        rpp::mutex log_m;

        auto log = [&](std::string msg)
        {
            std::lock_guard lock{log_m};
            execution_log.push_back(std::move(msg));
        };

        // post a callback
        loop.post([&]() {
            log("callback_1");
            AssertThat(rpp::get_thread_id(), main_tid);
        });

        auto coro = [&]() -> rpp::cfuture<void>
        {
            co_await loop.run_background([&]() {
                rpp::sleep_ms(5);
            });
            log("coro_resume");
            AssertThat(rpp::get_thread_id(), main_tid);
            co_return;
        };

        auto future = coro();

        loop.post([&]() {
            log("callback_2");
            AssertThat(rpp::get_thread_id(), main_tid);
        });

        loop.run_loop();
        future.get();

        // all three entries should be present
        {
            std::lock_guard lock{log_m};
            AssertThat(execution_log.size(), 3u);
            // callbacks 1 and 2 should be present, order between callbacks and coro
            // is not strictly guaranteed, but all must have executed
            bool has_cb1 = false, has_cb2 = false, has_coro = false;
            for (auto& s : execution_log)
            {
                if (s == "callback_1") has_cb1 = true;
                if (s == "callback_2") has_cb2 = true;
                if (s == "coro_resume") has_coro = true;
            }
            AssertThat(has_cb1, true);
            AssertThat(has_cb2, true);
            AssertThat(has_coro, true);
        }
    }

#endif // RPP_HAS_COROUTINES
};
