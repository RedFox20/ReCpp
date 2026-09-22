#include <rpp/event_loop.h>
#include <rpp/coroutines.h>
#include <rpp/future.h>
#include <rpp/thread_pool.h>
#include <rpp/timer.h>
#include <rpp/threads.h>
#include <rpp/collections.h>
#include <rpp/semaphore.h>

#include <rpp/tests.h>
#include <atomic>
#include <memory>
#include <vector>
#include <string>
#include <thread>
using namespace rpp;
using namespace std::chrono_literals;
using namespace std::string_literals;

namespace rpp
{
    /// Friend seam which calls the private event_loop::start_in_background(), see BUGS.md B26.
    class event_loop_test
    {
    public:
        static void run_background(event_loop& loop, rpp::delegate<void()> body) noexcept
        {
            loop.start_in_background(std::move(body));
        }
    };
}

TestImpl(test_event_loop)
{
    TestInit(test_event_loop)
    {
    }

    TestCleanup()
    {
        rpp::thread_pool::global().clear_idle_tasks();
    }


    rpp::AtomicTimeSource clock;
    // a loop can hold a raw pointer to this pool, so the pool must outlive the loop
    std::unique_ptr<rpp::thread_pool> custom_pool;
    std::unique_ptr<rpp::event_loop> loop;
    rpp::socket server, peer, client; // a loopback TCP pair, so a case feeds one end and awaits the other
    const uint64 main_tid = rpp::get_thread_id();
    // assert_failed() only records on the thread which owns the case, so a worker
    // reports a spin_until() timeout through this flag instead
    std::atomic_bool spin_timed_out { false };

    TestCaseSetup()
    {
        spin_timed_out = false;
        loop = std::make_unique<rpp::event_loop>(0, nullptr, &clock);
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
        loop.reset();        // the loop may point at custom_pool, so it goes first
        custom_pool.reset(); // and only then may the pool and its mutex die
        client.close();
        peer.close();
        server.close();
        AssertFalse(spin_timed_out.load()); // a worker thread could not report it itself
    }

    void assert_on_main_thread(RPP_SOURCE_LOC) { AssertThatLoc(loc, rpp::get_thread_id(), main_tid); }
    void idle() const { loop->run_until_idle(); }

    std::atomic_bool wait_done { false };
    bool wait_ready = false;
    // forks a coroutine which waits until `client` is readable, and reports through wait_ready and wait_done
    void fork_wait_readable(rpp::Duration timeout)
    {
        wait_done = false;
        // fork() keeps the closure alive for the coroutine, and the fixture outlives the loop
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
        loop->fork([this, timeout]() -> rpp::event_task
        {
            wait_ready = co_await loop->wait_readable(client, timeout);
            assert_on_main_thread();
            wait_done = true;
        });
    }

    void connect_pair()
    {
        server = rpp::make_tcp_randomport(rpp::SO_NonBlock);
        AssertTrue(server.good());
        client = rpp::socket::connect_to(rpp::ipaddress4{"127.0.0.1", server.port()}, 10, rpp::SO_NonBlock);
        AssertTrue(client.good());
        peer = server.accept(10);
        AssertTrue(peer.good());
    }

    // spins without pumping the loop, so a test can order itself against a worker thread
    template<class Predicate> void spin_until(const Predicate& pred, RPP_SOURCE_LOC)
    {
        rpp::TimePoint start = rpp::TimePoint::monotonic_now();
        while ((rpp::TimePoint::monotonic_now() - start) < rpp::seconds(1))
        {
            if (pred()) return;
            rpp::yield();
        }
        spin_timed_out = true;
        AssertFailedLoc(loc, "spin_until timed out");
    }

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
                spin_until([&]{ return b_resumed.load(); });
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
                spin_until([&]{ return slow_bg_running.load(); });
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

        rpp::cfuture<int> future = coro();

        // pumps the loop until the future resolves, instead of polling has_pending_work()
        AssertThat(loop->pump_until_ready(future, rpp::millis(500)), true);
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
                spin_until([&]{ return bg_may_finish.load(); });
            });
            co_return;
        };

        auto future = coro();

        // wait until background task has actually started
        spin_until([&]{ return bg_started.load(); });

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
        // a local pool would die when this case returns, and TestCaseCleanup destroys the
        // loop after that, so a worker could touch the destroyed pool mutex
        custom_pool = std::make_unique<rpp::thread_pool>();
        loop = std::make_unique<rpp::event_loop>(0, custom_pool.get());

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
                spin_until([&]{ return bg_may_finish.load(); });
            });
            co_return;
        };

        auto future = coro();

        // wait for bg task to start
        spin_until([&]{ return bg_started.load(); });

        AssertGreater(loop->background_tasks(), 0);

        // an awaiter posts the resume before start_in_background() drops the counter.
        // a pending completion alone does not prove the counter reached 0 yet
        bg_may_finish.store(true);
        spin_until([&]{ return loop->pending_completions() != 0 && loop->background_tasks() == 0; });

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

    // ─── run_async_void: launch a one-shot coroutine from ANY thread ──────────
    // Off the owner thread the launch is marshalled (post) before the coroutine starts, so
    // the caller need not be on the loop thread; the coroutine still resumes on the loop thread.
    TestCase(run_async_void_from_background_thread)
    {
        std::atomic_bool done { false };
        std::atomic<uint64> launch_tid { 0 };
        std::atomic<uint64> resume_tid { 0 };

        std::thread launcher([&]
        {
            launch_tid = rpp::get_thread_id();
            loop->run_async_void([&]() -> rpp::event_task
            {
                co_await loop->run_async([] { rpp::sleep_ms(1); });
                resume_tid = rpp::get_thread_id();
                done = true;
            });
        });
        launcher.join();

        AssertNotEqual(launch_tid.load(), main_tid); // launched off the owner thread

        // drive the loop on the owner thread; run_until_idle waits for the background work
        idle();

        AssertThat(done.load(), true);
        AssertThat(resume_tid.load(), main_tid); // coroutine resumed on the owner thread
    }

    // run_async_void called on the owner thread forks immediately (no marshalling).
    TestCase(run_async_void_on_owner_thread)
    {
        std::atomic_bool done { false };
        std::atomic<uint64> resume_tid { 0 };
        loop->run_async_void([&]() -> rpp::event_task
        {
            co_await loop->run_async([] { rpp::sleep_ms(1); });
            resume_tid = rpp::get_thread_id();
            done = true;
        });
        idle();
        AssertThat(done.load(), true);
        AssertThat(resume_tid.load(), main_tid);
    }

    // loops untile predicate returns true or wall timeout expires,
    // returns elapsed wall time
    template<typename Predicate>
    rpp::Duration loop_until(rpp::Duration timeout, const Predicate& pred)
    {
        rpp::TimePoint start = rpp::TimePoint::monotonic_now();
        while (!pred() && (rpp::TimePoint::monotonic_now() - start) < timeout)
            loop->run_once(rpp::millis(5));
        return rpp::TimePoint::monotonic_now() - start;
    }

    // ─── warpable clock: delay() tracks an AtomicTimeSource so warp_forward releases it ──
    // A 10-virtual-second delay can only complete within a 2s wall budget if warp_forward()
    // advanced the loop's clock past the deadline.
    TestCase(delay_is_released_by_time_warp)
    {
        std::atomic_bool done { false };
        loop->fork([&]() -> rpp::event_task
        {
            co_await loop->delay(rpp::seconds(10)); // 10 *virtual* seconds
            done = true;
        });

        clock.warp_forward(rpp::seconds(10)); // advance virtual time past the deadline

        loop_until(rpp::seconds(1), [&]{ return done.load(); });
        AssertThat(done.load(), true);
    }

    // ─── wait_on_all honors its timeout on a warped clock ───────
    // wait_on_all builds its deadline from the loop clock it polls with. A deadline
    // from a different clock expires at once and gives a background task no grace.
    TestCase(wait_on_all_waits_for_background_task_on_warped_clock)
    {
        std::atomic_bool bg_started { false };
        std::atomic_bool bg_finished { false };
        loop->fork([&]() -> rpp::event_task
        {
            co_await loop->run_async([&]
            {
                bg_started = true;
                rpp::sleep_ms(10);
                bg_finished = true;
            });
        });

        // the task must own the counter before we wait
        spin_until([&]{ return bg_started.load(); });
        clock.warp_forward(rpp::seconds(10));

        AssertTrue(loop->wait_on_all(rpp::seconds(1)));
        AssertThat(bg_finished.load(), true);
    }

    // ─── wait_on_all outlives a callback which frees the clock ──────────────────
    // Its first drain runs owner-thread callbacks, and one of them can detach and free
    // the clock. A wait which kept the raw source then reads freed memory.
    TestCase(wait_on_all_survives_a_callback_which_frees_the_clock)
    {
        auto owned = std::make_unique<rpp::AtomicTimeSource>();
        owned->warp_forward(rpp::seconds(2)); // the deadline below is built in this frame
        loop->set_time_source(owned.get());

        rpp::semaphore gate; // holds the worker, so the wait below reaches its timeout
        loop->fork([&]() -> rpp::event_task
        {
            co_await loop->run_async([&]{ gate.wait(); });
        });
        // the task must own the counter before the wait
        spin_until([&]{ return loop->has_background_tasks(); });

        // the drain runs this before the wait, so the wait must keep no raw source
        loop->post([&]{ loop->set_time_source(nullptr); owned.reset(); });

        rpp::Timer wall;
        AssertThat(loop->wait_on_all(rpp::millis(20)), false); // the gated worker cannot finish
        double wait_ms = wall.elapsed_millis();
        print_info("wait_on_all: %.1fms\n", wait_ms);
        AssertLess(wait_ms, 1000.0); // a freed offset would hold the wait far past its budget

        gate.notify(); // release the worker, so the drain below does not wait on it
        loop->run_until_idle();
        AssertThat(loop->has_background_tasks(), false);
    }

    // ─── a delay() which loses its clock mid-wait ───────────────
    // The waiter keeps the last offset it read. A waiter which re-reads a detached source
    // drops the warp below and then waits for real time to reach the deadline.
    TestCase(delay_survives_a_detached_time_source)
    {
        clock.warp_forward(rpp::millis(400)); // the deadline below is built in this frame
        std::atomic_bool done { false };
        loop->fork([&]() -> rpp::event_task
        {
            co_await loop->delay(rpp::millis(20));
            done = true;
        });

        // the timer holds its frame before the detach
        AssertThat(loop->pending_waiters(), 1);
        loop->set_time_source(nullptr);

        loop_until(rpp::millis(150), [&]{ return done.load(); });
        AssertThat(done.load(), true);
    }

    // ─── the drain notices a count which drops after the last event ─────
    // A worker posts its resume, then drops the count when it returns. A drain which waits
    // on the queue alone waits for the whole timeout, because no event follows that drop.
    TestCase(stop_and_wait_all_ready_returns_when_the_last_task_returns)
    {
        rpp::event_loop_test::run_background(*loop, [this]
        {
            loop->post_resume({}); // what every awaiter does last
            rpp::sleep_us(200); // the drain reaches its wait before the count drops
        });
        rpp::Timer wall;
        AssertThat(loop->stop_and_wait_all_ready(rpp::seconds(1)), true);
        double drain_ms = wall.elapsed_millis();
        print_info("stop_and_wait_all_ready: %.1fms\n", drain_ms);
        AssertLess(drain_ms, 100.0); // a drain which waits on the queue alone reaches the timeout
    }

    // ─── a delay() whose clock a swap replaces mid-wait ─────────
    // A deadline belongs to the clock which built it. A waiter which takes the offset of the
    // new clock measures that deadline on a timeline it never saw. See BUGS.md B26.
    TestCase(delay_keeps_its_own_clock_when_a_swap_replaces_it)
    {
        std::atomic_bool done { false };
        loop->fork([&]() -> rpp::event_task
        {
            co_await loop->delay(rpp::millis(20));
            done = true;
        });
        AssertThat(loop->pending_waiters(), 1); // the timer holds its frame before the swap

        auto other = std::make_unique<rpp::AtomicTimeSource>();
        other->warp_forward(rpp::seconds(3600)); // far past the deadline the old clock built
        loop->set_time_source(other.get());

        rpp::Duration waited = loop_until(rpp::seconds(1), [&]{ return done.load(); });
        AssertThat(done.load(), true);
        AssertGreater(waited.millis(), 15); // the new clock must not move a deadline the old one built

        loop->set_time_source(nullptr); // the scope frees `other` next
    }

    // ─── a swap which frees the clock it replaced ───────────────
    // Every attach drains the readers, so an owner may free the clock a swap replaced.
    // Without that drain a reader holds the old pointer and ASAN reports the free. See B26.
    TestCase(a_swap_drains_the_readers_before_the_owner_frees_the_old_clock)
    {
        std::atomic_bool stop { false };
        std::atomic_int reads { 0 };
        constexpr int NUM_READERS = 2; // more readers cost the drain far more time, see BUGS.md B26
        std::vector<std::thread> readers;
        readers.reserve(NUM_READERS);
        for (int t = 0; t < NUM_READERS; ++t)
            readers.emplace_back([&]{ while (!stop.load()) { (void)loop->current_time(); reads.fetch_add(1); } });
        spin_until([&]{ return reads.load() != 0; }); // a reader must be live before the first swap

        std::unique_ptr<rpp::AtomicTimeSource> live;
        for (int i = 0; i < 20000; ++i)
        {
            auto next = std::make_unique<rpp::AtomicTimeSource>();
            loop->set_time_source(next.get());
            live = std::move(next); // frees the clock the swap above replaced, never the live one
        }
        loop->set_time_source(nullptr);
        live.reset();

        stop = true;
        for (std::thread& r : readers) r.join();
        AssertGreater(reads.load(), 0);
    }

    // ─── a frame which mixes one clock's offset with another's generation ───
    // The offset and the generation come from two atomics. A publish which leaves the old
    // clock live beside the new generation lets a reader take one from each. See BUGS.md B26.
    TestCase(a_frame_never_pairs_one_clocks_offset_with_another_generation)
    {
        constexpr int NUM_CLOCKS = 8; // clock i warps i+1 seconds, so its offset names it
        std::vector<std::unique_ptr<rpp::AtomicTimeSource>> clocks;
        for (int i = 0; i < NUM_CLOCKS; ++i)
        {
            clocks.push_back(std::make_unique<rpp::AtomicTimeSource>());
            clocks.back()->warp_forward(rpp::seconds(i + 1));
        }

        std::atomic_bool stop { false };
        std::atomic_int frames { 0 };
        std::atomic_int skewed { 0 };
        auto base = loop->get_time_source_frame().generation; // the clock the fixture attached
        std::thread reader { [&]
        {
            while (!stop.load())
            {
                auto f = loop->get_time_source_frame();
                if (!f.warpable || f.generation == base)
                    continue;
                int index = int((f.generation - base - 1) % NUM_CLOCKS);
                frames.fetch_add(1);
                if (f.offset_ns != rpp::seconds(index + 1).nsec)
                    skewed.fetch_add(1);
            }
        }};

        for (int i = 0; i < 20000; ++i)
            loop->set_time_source(clocks[i % NUM_CLOCKS].get());
        stop = true;
        reader.join();
        loop->set_time_source(nullptr); // the scope frees `clocks` next

        AssertGreater(frames.load(), 0);
        AssertThat(skewed.load(), 0);
    }

    // ─── a join_forks() which loses its clock mid-wait ──────────
    // The join timer keeps the last offset it read. A timer which re-reads a detached source
    // drops the warp below and then waits for real time to reach the deadline.
    TestCase(join_forks_survives_a_detached_time_source)
    {
        clock.warp_forward(rpp::millis(400)); // the join deadline below is built in this frame
        rpp::semaphore gate; // holds one fork open, so the join reaches its timeout
        loop->fork([&]() -> rpp::event_task
        {
            co_await loop->run_async([&]{ gate.wait(); });
        });

        std::atomic_bool joined { false };
        auto joiner = [&]() -> rpp::event_task
        {
            co_await loop->join_forks(rpp::millis(20));
            joined = true;
        };
        rpp::event_task task = joiner();

        // the gated fork must own the counter and the join timer must hold its frame before the detach
        spin_until([&]{ return loop->background_tasks() >= 1; });
        AssertThat(loop->pending_waiters(), 1);
        loop->set_time_source(nullptr);

        loop_until(rpp::millis(150), [&]{ return joined.load(); });
        AssertTrue(joined.load()); // a dropped offset would hold the join past its budget

        gate.notify(); // release the fork, so the cleanup below does not wait on it
        loop->run_until_idle();
    }

    // ─── the shutdown retires the clock before the owner frees it ───────────────
    // A shutdown which does not wait out the readers leaves one reading freed memory.
    // Stress reproducer: ASAN catches that 4 runs in 10, see BUGS.md B26.
    TestCase(stop_and_wait_all_ready_retires_the_clock_before_the_owner_frees_it)
    {
        std::atomic_bool stop { false };
        std::atomic_int reads { 0 };
        constexpr int NUM_READERS = 2; // more readers cost the retire far more time, see BUGS.md B26
        std::vector<std::thread> readers;
        readers.reserve(NUM_READERS);
        for (int t = 0; t < NUM_READERS; ++t)
            readers.emplace_back([&]{ while (!stop.load()) { (void)loop->current_time(); reads.fetch_add(1); } });
        // a reader must be live before the first detach
        spin_until([&]{ return reads.load() != 0; });

        const int before = reads.load();
        for (int i = 0; i < 20000; ++i)
        {
            auto warpable = std::make_unique<rpp::AtomicTimeSource>();
            loop->set_time_source(warpable.get());
            loop->stop_and_wait_all_ready(rpp::millis(20)); // retires before the scope frees it
        }

        stop = true;
        for (std::thread& reader : readers) reader.join();
        AssertGreater(reads.load(), before); // the readers ran across the detach cycles
    }

    // posts a callback from the final resume, which lands as the background count hits zero
    void fork_with_trailing_post(std::atomic_bool& trailing_ran)
    {
        loop->fork([this, &trailing_ran]() -> rpp::event_task
        {
            co_await loop->run_async([]{ rpp::sleep_ms(5); });
            // the worker pushes the resume before it decrements, so the post must wait for
            // the count, or wait_on_all drains it in the same pass and the drain proves nothing
            spin_until([&]{ return !loop->has_background_tasks(); });
            loop->post([&trailing_ran]{ trailing_ran = true; });
        });
    }

    // the trailing post starts a blocked task, so run_all_ready() leaves the loop busy
    void fork_with_trailing_post_starting(rpp::semaphore& release)
    {
        loop->fork([this, &release]() -> rpp::event_task
        {
            co_await loop->run_async([]{ rpp::sleep_ms(5); });
            // the fork must start inside run_all_ready()
            spin_until([&]{ return !loop->has_background_tasks(); });
            loop->post([this, &release]
            {
                loop->fork([this, &release]() -> rpp::event_task
                {
                    co_await loop->run_async([&release]{ release.wait(); });
                });
            });
        });
    }

    // parks the coroutine handle instead of starting background work, so neither counter sees it
    struct parking_awaiter
    {
        rpp::coro_handle<>& slot;
        // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
        bool await_ready() const noexcept { return false; }
        void await_suspend(rpp::coro_handle<> h) noexcept { slot = h; }
        void await_resume() const noexcept {}
    };

    // ─── shutdown: the one call that drains and detaches ────────
    TestCase(stop_and_wait_all_ready_drains_and_detaches)
    {
        std::atomic_bool trailing_ran { false };
        fork_with_trailing_post(trailing_ran);
        clock.warp_forward(rpp::seconds(10000));

        AssertTrue(loop->stop_and_wait_all_ready(rpp::seconds(1)));
        AssertThat(trailing_ran.load(), true);

        // detached: current_time() reads the wall clock, not the warped source
        AssertLess(loop->current_time(), clock.time_now() - rpp::seconds(9000));
    }

    // ─── shutdown: a timeout keeps the time source attached ─────
    // a live delay() worker polls it against a virtual deadline and would never reach a wall-clock one
    TestCase(stop_and_wait_all_ready_keeps_the_clock_on_timeout)
    {
        rpp::semaphore release;
        loop->fork([&]() -> rpp::event_task
        {
            co_await loop->run_async([&]{ release.wait(); });
        });

        clock.warp_forward(rpp::seconds(10000));
        AssertFalse(loop->stop_and_wait_all_ready(rpp::millis(20)));
        // still attached: current_time() carries the warp, a detached loop would read wall time
        AssertGreater(loop->current_time(), rpp::TimePoint::monotonic_now() + rpp::seconds(9000));

        release.notify();
        AssertTrue(loop->stop_and_wait_all_ready(rpp::seconds(1)));
    }

    // ─── shutdown: a suspended fork blocks completion ───────────
    // it runs no background task and queues no resume, so num_forks() is the only signal
    TestCase(stop_and_wait_all_ready_counts_a_suspended_fork)
    {
        rpp::coro_handle<> parked {};
        loop->fork([&]() -> rpp::event_task { co_await parking_awaiter{ parked }; });
        clock.warp_forward(rpp::seconds(10000));

        AssertFalse(loop->stop_and_wait_all_ready(rpp::millis(20)));
        AssertGreater(loop->current_time(), rpp::TimePoint::monotonic_now() + rpp::seconds(9000));

        loop->post_resume(parked);
        AssertTrue(loop->stop_and_wait_all_ready(rpp::seconds(1)));
    }

    // ─── shutdown: work started inside the drain keeps the clock ─
    // run_all_ready() resumes a coroutine which starts a new task, so the pre-drain count is stale
    TestCase(stop_and_wait_all_ready_keeps_the_clock_for_work_started_in_the_drain)
    {
        rpp::semaphore release;
        fork_with_trailing_post_starting(release);
        clock.warp_forward(rpp::seconds(10000));

        // the blocked task never finishes, so a short wait costs 20ms instead of a full second
        AssertFalse(loop->stop_and_wait_all_ready(rpp::millis(20)));
        AssertGreater(loop->current_time(), rpp::TimePoint::monotonic_now() + rpp::seconds(9000));

        release.notify();
        AssertTrue(loop->stop_and_wait_all_ready(rpp::seconds(1)));
    }

    // ─── loop hook: fires on every run_once() ───────────────────
    // run_once() always invokes the hook, whether or not an event was processed.
    TestCase(loop_hook_invoked_by_run_once)
    {
        int hook_calls = 0;
        loop->set_loop_hook_handler([&]{ ++hook_calls; });
        loop->run_once(rpp::Duration::zero()); // empty queue, non-blocking
        loop->run_once(rpp::Duration::zero());
        loop->run_once(rpp::Duration::zero());
        AssertThat(hook_calls, 3);
    }

    // ─── loop hook: NOT fired by run_all_ready() ────────────────
    // run_all_ready() drains ready events without ever calling the hook (documented).
    TestCase(loop_hook_not_invoked_by_run_all_ready)
    {
        int hook_calls = 0;
        loop->set_loop_hook_handler([&]{ ++hook_calls; });
        loop->post([]{}); // something to drain
        loop->post([]{});
        loop->run_all_ready();
        AssertThat(hook_calls, 0);
    }

    // ─── loop hook drives time warp: long delay ends quickly ────
    // The hook fires on every run_once(); a test uses it to warp virtual time forward
    // so a 10-virtual-second delay() resolves in a tiny wall budget instead of blocking
    // for the full duration. Without the warp this loop would run ~10 wall seconds.
    TestCase(loop_hook_warps_time_so_delay_ends_quickly)
    {
        loop->set_loop_hook_handler([&]{ clock.warp_forward(rpp::millis(500)); });

        std::atomic_bool done { false };
        loop->fork([&]() -> rpp::event_task
        {
            co_await loop->delay(rpp::seconds(10)); // 10 *virtual* seconds
            done = true;
        });

        // NOTE: hook warps +500ms each call
        rpp::Duration spent = loop_until(rpp::seconds(1), [&]{ return done.load(); });
        AssertThat(done.load(), true);
        AssertLess(spent, rpp::seconds(1)); // without the hook this spends the whole virtual delay
    }

    // ─── loop hook drives time warp under run_until_idle() ──────
    // run_until_idle() invokes the hook on its idle iterations (between resume events).
    // A single large warp on the first idle tick pushes virtual time past the deadline,
    // so the delay resolves immediately and the loop drains.
    TestCase(loop_hook_warps_time_in_run_until_idle)
    {
        loop->set_loop_hook_handler([&]{ clock.warp_forward(rpp::seconds(3600)); });

        std::atomic_bool done { false };
        loop->fork([&]() -> rpp::event_task
        {
            co_await loop->delay(rpp::seconds(10)); // 10 *virtual* seconds
            done = true;
        });

        rpp::Timer wall;
        loop->run_until_idle(); // idle-tick hook warps time so the delay resolves fast
        AssertThat(done.load(), true);
        AssertLess(wall.elapsed_ms(), 2000);
    }

    // ─── Helper: launch a coroutine OFF the loop thread ─────────
    // Runs `make_coro` (a factory returning rpp::cfuture<void>) on a fresh
    // background thread, so the coroutine's synchronous prefix (up to its first
    // co_await) executes OFF the loop thread. Then drives the loop on this
    // (owner) thread until the coroutine completes, bounded by a 2s wall budget
    // so a regression fails instead of hanging. Returns the launcher thread id.
    template <class MakeCoro>
    uint64 start_coro_on_background_thread(MakeCoro make_coro)
    {
        std::atomic<uint64> launcher_tid{0};
        rpp::cfuture<void> fut;
        // The task returns void and assigns the coroutine future instead of returning it.
        // Returning it would instantiate std::future<cfuture<void>>::get(), a non-coroutine
        // returning a coro type, which no RPP_CORO_WRAPPER can annotate inside libstdc++.
        rpp::async_task([&]
        {
            launcher_tid = rpp::get_thread_id();
            fut = make_coro(); // start the coroutine here, off the loop thread
        }).get(); // fut is set before anything below reads it

        loop_until(rpp::seconds(1), [&]{ return fut.await_ready(); });

        fut.get(); // ready in the success path; rethrows any coroutine exception
        return launcher_tid.load();
    }

    // awaiting an ALREADY-READY future must still
    // hop onto the loop thread, never resume inline on the caller.
    TestCase(ready_future_await_resumes_on_loop_not_caller)
    {
        std::atomic<uint64> resume_tid{0};
        uint64 launcher = start_coro_on_background_thread(
            [&]() -> rpp::cfuture<void>
            {
                // make_ready_future is resolved before the await even begins
                co_await loop->run_async(rpp::make_ready_future(42));
                resume_tid = rpp::get_thread_id();
            });
        AssertNotEqual(launcher, main_tid); // sanity: coroutine really started off the loop
        AssertThat(resume_tid.load(), main_tid); // resumed on the loop thread, not the caller
    }

    // resume_on_loop() unconditionally reschedules onto the loop thread,
    // even when the coroutine is currently running off the loop.
    TestCase(resume_on_loop_hops_to_loop_thread)
    {
        std::atomic<uint64> resume_tid{0};
        uint64 launcher = start_coro_on_background_thread(
            [&]() -> rpp::cfuture<void>
            {
                co_await loop->resume_on_loop();
                resume_tid = rpp::get_thread_id();
            });
        AssertNotEqual(launcher, main_tid);
        AssertThat(resume_tid.load(), main_tid);
    }

    // resume_on_loop() while ALREADY on the loop thread: still valid, no
    // deadlock, and stays on the loop thread across repeated hops.
    TestCaseCoro(resume_on_loop_when_already_on_loop)
    {
        assert_on_main_thread();
        co_await loop->resume_on_loop();
        assert_on_main_thread();
        co_await loop->resume_on_loop();
        assert_on_main_thread();
    }

    // ─── run_until_ready: drive a future to completion FROM the owner thread ─────
    // without a blocking get() — the continuation resumes on this thread, so we pump.
    TestCase(run_until_ready_pumps_future_on_owner_thread)
    {
        // the closure must outlive the coroutine, which reads its captures after the suspend
        auto coro = [&]() -> rpp::cfuture<int>
        {
            int v = co_await loop->run_async([] { rpp::sleep_ms(5); return 7; });
            co_return v * 6;
        };
        rpp::cfuture<int> fut = coro();
        int result = loop->run_until_ready(fut);
        AssertThat(result, 42);
    }

    // no pump makes a deferred future ready, so reporting a timeout would throw on a future
    // whose get() resolves it at once
    TestCase(run_until_ready_resolves_a_deferred_future)
    {
        bool ran = false;
        rpp::cfuture<int> fut = std::async(std::launch::deferred, [&] { ran = true; return 42; });

        AssertThat(loop->pump_until_ready(fut, rpp::millis(20)), true);
        AssertThat(ran, false); // the pump must not have run it
        AssertThat(fut.get(), 42);
        AssertThat(ran, true);
    }

    // pump_until_ready returns false on timeout instead of blocking on get().
    TestCase(pump_until_ready_times_out_without_blocking)
    {
        // the closure must outlive the coroutine, which reads its captures after the suspend
        auto coro = [&]() -> rpp::cfuture<int>
        {
            co_await loop->run_async([] { rpp::sleep_ms(200); return 1; });
            co_return 1;
        };
        rpp::cfuture<int> fut = coro();
        rpp::Timer wall;
        bool ready = loop->pump_until_ready(fut, rpp::millis(20));
        double pump_ms = wall.elapsed_millis();
        // print before asserting: this test has aborted on Windows with no other output
        print_info("pump_until_ready: ready=%d after %.1fms\n", (int)ready, pump_ms);

        AssertThat(ready, false);    // 200ms work can't finish in a 20ms budget
        AssertLess(pump_ms, 2000.0); // returned promptly — did NOT block on get()

        // Drain without throwing. run_until_ready throws on timeout, and unwinding here would
        // leave the pool task mid-sleep with a continuation into a loop the next test destroys.
        AssertThat(loop->pump_until_ready(fut, rpp::seconds(15)), true);
        AssertThat(fut.get(), 1);

        // the loop must own nothing in flight before the fixture replaces it
        AssertThat(loop->wait_on_all(rpp::millis(1000)), true);
        AssertThat(loop->has_background_tasks(), false);
    }

    // ─── pump_until_ready keeps its deadline when the clock detaches ────────────
    // The pump builds `end` from the loop clock. A pump which re-reads a detached source
    // drops the warp below and then overruns its own budget.
    TestCase(pump_until_ready_survives_a_detached_time_source)
    {
        clock.warp_forward(rpp::millis(400)); // the pump deadline below is built in this frame
        rpp::semaphore gate; // holds the worker until the pump budget is measured
        // the closure must outlive the coroutine, which reads its captures after the suspend
        auto coro = [&]() -> rpp::cfuture<int>
        {
            co_await loop->run_async([&]{ gate.wait(); return 1; });
            co_return 1;
        };
        rpp::cfuture<int> fut = coro();

        // the pump runs this on its first run_once(), so the detach lands after `end` is built
        loop->post([this]{ loop->set_time_source(nullptr); });

        rpp::Timer wall;
        bool ready = loop->pump_until_ready(fut, rpp::millis(20));
        double pump_ms = wall.elapsed_millis();
        print_info("pump_until_ready: ready=%d after %.1fms\n", (int)ready, pump_ms);

        AssertThat(ready, false);   // the gated worker cannot finish inside the budget
        AssertLess(pump_ms, 150.0); // a dropped offset would hold the pump past its budget

        gate.notify(); // release the worker, so the drain below does not wait on the clock
        AssertThat(loop->pump_until_ready(fut, rpp::seconds(1)), true); // drain without throwing
        AssertThat(fut.get(), 1);
    }

    // the owner frees the loop as soon as the background count reaches zero, see BUGS.md B26
    TestCase(the_background_count_is_a_workers_last_touch_of_the_loop)
    {
        constexpr int CYCLES = 200; // the shutdown path is what this exercises, not a narrow window
        int completed = 0;
        int drained = 0;
        for (int i = 0; i < CYCLES; ++i)
        {
            auto scoped = std::make_unique<rpp::event_loop>();
            rpp::event_loop* ev = scoped.get();
            std::atomic_bool ran { false };
            ev->fork([ev, &ran]() -> rpp::event_task
            {
                co_await ev->run_async([&ran]{ ran = true; });
            });
            const bool drain_ok = ev->stop_and_wait_all_ready(rpp::seconds(1));
            if (drain_ok) ++drained;
            scoped.reset(); // frees the loop while a worker may still be inside run()
            if (ran.load()) ++completed;
            if (!drain_ok) break; // a stuck drain burns the whole timeout each cycle
        }
        AssertEqual(drained, CYCLES); // a shutdown which gave up proves nothing about the window
        AssertEqual(completed, CYCLES);
    }

    // a worker reads the loop after it posts the resume, see BUGS.md B26 probe B
    TestCase(the_count_drops_after_a_background_task_returns)
    {
        constexpr int CYCLES = 200;
        int touched = 0;
        for (int i = 0; i < CYCLES; ++i)
        {
            auto scoped = std::make_unique<rpp::event_loop>();
            rpp::event_loop* ev = scoped.get();
            std::atomic_int seen { 0 };
            rpp::event_loop_test::run_background(*ev, [ev, &seen]
            {
                ev->post_resume({}); // what every awaiter does last
                rpp::sleep_us(200); // widens the window the owner races
                seen += ev->background_tasks();
            });
            spin_until([ev]{ return ev->background_tasks() == 0; });
            scoped.reset(); // the owner frees the loop the moment the count reaches zero
            if (spin_timed_out.load()) break;
            touched += seen.load();
        }
        AssertEqual(touched, CYCLES); // every worker read a live loop after its own resume
    }

    // ensure_on_owner_thread: true on the owner thread, false off it (logs an error — expected).
    TestCase(ensure_on_owner_thread_detects_off_thread)
    {
        AssertThat(loop->ensure_on_owner_thread(RPP_SOURCE_LOC_CURRENT), true); // we are the owner thread

        bool off_thread = true;
        std::thread t{[&] { off_thread = loop->ensure_on_owner_thread(RPP_SOURCE_LOC_CURRENT); }};
        t.join();
        AssertThat(off_thread, false); // ran on a different thread (one expected error log above)
    }

    // ─── loop timers: delay() parks on the loop thread ──────────
    // the timer queue holds the deadline, so no pool worker sleeps on its behalf
    TestCase(delay_needs_no_pool_worker)
    {
        custom_pool = std::make_unique<rpp::thread_pool>();
        loop = std::make_unique<rpp::event_loop>(0, custom_pool.get(), &clock);
        std::atomic_bool done { false };
        loop->fork([&]() -> rpp::event_task
        {
            co_await loop->delay(rpp::millis(5));
            assert_on_main_thread();
            done = true;
        });
        AssertThat(loop->pending_waiters(), 1);
        AssertThat(loop->background_tasks(), 0);
        loop_until(rpp::seconds(1), [&]{ return done.load(); });
        AssertThat(done.load(), true);
        AssertThat(loop->pending_waiters(), 0);
        AssertThat(custom_pool->total_tasks(), 0); // the pool never started a worker
    }

    // run_once() wakes at the timer deadline and resumes the coroutine inside the same call
    TestCase(run_once_wakes_at_the_earliest_timer)
    {
        std::atomic_bool done { false };
        loop->fork([&]() -> rpp::event_task
        {
            co_await loop->delay(rpp::millis(10));
            done = true;
        });
        rpp::Timer wall;
        const bool processed = loop->run_once(rpp::seconds(1));
        AssertThat(processed, true);
        AssertThat(done.load(), true);
        AssertLess(wall.elapsed_millis(), 500.0);
    }

    // an await from another thread registers through the resume queue, so the loop thread owns the timer list
    TestCase(delay_from_another_thread_registers_through_the_queue)
    {
        std::atomic<uint64> resume_tid{0};
        // the closure must outlive the coroutine, which reads its captures after the suspend
        auto coro = [&]() -> rpp::cfuture<void>
        {
            co_await loop->delay(rpp::millis(5));
            resume_tid = rpp::get_thread_id();
        };
        rpp::cfuture<void> fut;
        rpp::async_task([&]{ fut = coro(); }).get(); // the coroutine suspended on the worker before this returns
        AssertThat(loop->pending_completions(), 1); // the registration waits in the queue
        AssertThat(loop->pending_waiters(), 0);
        loop_until(rpp::seconds(1), [&]{ return fut.await_ready(); });
        fut.get(); // ready in the success path, and a cfuture must be consumed before it dies
        AssertThat(resume_tid.load(), main_tid);
    }

    // ─── socket waits: the loop thread polls the descriptor ─────
    TestCase(wait_readable_from_another_thread_registers_through_the_queue)
    {
        connect_pair();
        std::atomic<uint64> resume_tid{0};
        bool ready = true;
        // the closure must outlive the coroutine, which reads its captures after the suspend
        auto coro = [&]() -> rpp::cfuture<void>
        {
            ready = co_await loop->wait_readable(client, rpp::millis(5));
            resume_tid = rpp::get_thread_id();
        };
        rpp::cfuture<void> fut;
        rpp::async_task([&]{ fut = coro(); }).get(); // the coroutine suspended on the worker before this returns
        AssertThat(loop->pending_completions(), 1); // the registration waits in the queue
        AssertThat(loop->pending_waiters(), 0);
        loop_until(rpp::seconds(1), [&]{ return fut.await_ready(); });
        fut.get(); // ready in the success path, and a cfuture must be consumed before it dies
        AssertThat(ready, false);
        AssertThat(resume_tid.load(), main_tid);
    }

    TestCase(wait_readable_resumes_when_data_arrives)
    {
        connect_pair();
        fork_wait_readable(rpp::seconds(1));
        AssertThat(loop->pending_waiters(), 1);
        AssertThat(loop->background_tasks(), 0);
        rpp::cfuture<void> sender = rpp::async_task([&]{ rpp::sleep_ms(5); peer.send("hi", 2); });
        rpp::Duration spent = loop_until(rpp::seconds(1), [&]{ return wait_done.load(); });
        sender.get();
        AssertThat(wait_ready, true);
        AssertLess(spent, rpp::millis(500));
        AssertThat(client.available(), 2);
    }

    TestCase(wait_readable_times_out_without_data)
    {
        connect_pair();
        fork_wait_readable(rpp::millis(10));
        rpp::Duration spent = loop_until(rpp::seconds(1), [&]{ return wait_done.load(); });
        AssertThat(wait_ready, false);
        AssertLess(spent, rpp::millis(500));
    }

    TestCase(wait_readable_timeout_is_released_by_time_warp)
    {
        connect_pair();
        fork_wait_readable(rpp::seconds(10)); // 10 *virtual* seconds
        clock.warp_forward(rpp::seconds(10));
        rpp::Duration spent = loop_until(rpp::seconds(1), [&]{ return wait_done.load(); });
        AssertThat(wait_ready, false);
        AssertLess(spent, rpp::millis(500));
    }

    // a close is how the owner of a coroutine cancels its socket wait
    TestCase(closing_the_socket_releases_wait_readable)
    {
        connect_pair();
        fork_wait_readable(rpp::seconds(1)); // the close releases it, never the timeout
        client.close();
        rpp::Duration spent = loop_until(rpp::seconds(1), [&]{ return wait_done.load(); });
        AssertThat(wait_ready, true); // the socket needs attention, and recv() then reports the close
        AssertLess(spent, rpp::millis(500));
    }

    // a post() from a worker must not wait for the socket poll to time out
    TestCase(post_wakes_a_socket_poll)
    {
        loop = std::make_unique<rpp::event_loop>(); // a warpable clock slices the poll, and that would hide a lost wake
        connect_pair();
        fork_wait_readable(rpp::seconds(1)); // nothing arrives, so the poll sits here
        std::atomic_bool posted { false };
        rpp::cfuture<void> poster = rpp::async_task([&]{ rpp::sleep_ms(5); loop->post([&]{ posted = true; }); });
        rpp::Timer wall;
        loop->run_once(rpp::seconds(1));
        const double waited_ms = wall.elapsed_millis();
        poster.get();
        AssertThat(posted.load(), true);
        AssertLess(waited_ms, 500.0); // without the wake socket the poll holds the post for its whole timeout
        client.close(); // releases the waiter, so the fixture drains without a timeout
        idle();
    }

    TestCaseCoro(wait_writable_is_ready_on_a_connected_socket)
    {
        connect_pair();
        const bool ready = co_await loop->wait_writable(client, rpp::millis(10));
        AssertThat(ready, true);
        assert_on_main_thread();
    }

    // ─── connect: the loop completes a non-blocking connect ─────
    TestCaseCoro(connect_completes_on_the_loop)
    {
        server = rpp::make_tcp_randomport(rpp::SO_NonBlock);
        AssertTrue(server.good());
        rpp::socket sock;
        const bool connected = co_await sock.connect(*loop, rpp::ipaddress4{"127.0.0.1", server.port()}, rpp::millis(10));
        assert_on_main_thread();
        AssertThat(connected, true);
        AssertThat(sock.connected(), true);
        AssertThat(loop->background_tasks(), 0);
        peer = co_await server.accept(*loop, rpp::millis(10));
        assert_on_main_thread();
        AssertTrue(peer.good());
    }

    TestCaseCoro(connect_to_a_closed_port_fails_on_the_loop)
    {
        rpp::socket closed = rpp::make_tcp_randomport();
        const int port = closed.port();
        closed.close();
        rpp::socket sock;
        rpp::Timer wall;
        const bool connected = co_await loop->connect(sock, rpp::ipaddress4{"127.0.0.1", port}, rpp::millis(10));
        AssertThat(connected, false);
        // Windows reports a refused loopback connect late, so the bound is the timeout and not the refusal
        AssertLess(wall.elapsed_millis(), 100.0);
        AssertNotEqual(sock.last_err_type(), rpp::socket::SE_NONE);
    }

    // NOLINTEND(cppcoreguidelines-avoid-capturing-lambda-coroutines)

};
