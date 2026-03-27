#include <rpp/tests.h>
#include <rpp/thread_pool.h>
#include <rpp/future.h>
#include <rpp/semaphore.h>
#include <rpp/timer.h> // performance measurement
#include <rpp/threads.h> // thread naming
#include <atomic>
#include <latch>
#include <unordered_set>
using namespace rpp;

namespace rpp
{
    inline rpp::string_buffer& operator<<(rpp::string_buffer& sb, rpp::wait_result wr)
    {
        sb << (wr == wait_result::finished ? "finished" : "timeout");
        return sb;
    }
}


TestImpl(test_threadpool)
{
    TestInit(test_threadpool)
    {
        print_info("global_max_parallelism: %d\n", thread_pool::global_max_parallelism());
    }

    TestCleanup()
    {
        thread_pool::global().clear_idle_tasks();
        if (thread_pool::global().active_tasks() > 0)
            print_error("Dangling tasks detected: %d\n", thread_pool::global().active_tasks());
    }

    TestCaseSetup()
    {
        // reset to default
        thread_pool::set_global_max_parallelism(num_physical_cores());
    }

    static int count_parallel_for_thread_ids(int numIterations)
    {
        rpp::mutex m;
        std::unordered_set<rpp::uint64> ids;
        parallel_for(0, numIterations, 0, [&](int start, int end)
        {
            (void)start; (void)end;
            rpp::sleep_ms(1);
            { std::lock_guard lock{m};
                ids.insert(rpp::get_thread_id());
            }
        });
        std::lock_guard lock{m};
        return (int)ids.size();
    }

    TestCase(basic_thread_utils)
    {
        rpp::set_this_thread_name("TestThread");
        print_info("Current thread name: '%s' (expected: 'TestThread')\n", rpp::get_this_thread_name().c_str());
        AssertThat(rpp::get_this_thread_name(), "TestThread");

        rpp::set_this_thread_name("AnotherName");
        print_info("Current thread name: '%s' (expected: 'AnotherName')\n", rpp::get_this_thread_name().c_str());
        AssertThat(rpp::get_this_thread_name(), "AnotherName");
    }

    TestCase(parallel_for_should_not_exceed_max_parallelism)
    {
        AssertThat(count_parallel_for_thread_ids(1), 1);
        AssertThat(count_parallel_for_thread_ids(128),
                   thread_pool::global_max_parallelism());
    }

    TestCase(generic_task)
    {
        semaphore sync;
        std::string s = "Data";
        rpp::task_delegate<void()> fun = [x=s, &s, &sync]()
        {
            //printf("generic_task: %s\n", x.c_str());
            s = "completed";
            sync.notify();
        };
        parallel_task([f=std::move(fun)]()
        {
            f();
        });

        sync.wait();
        AssertThat(s, "completed");
    }

    TestCase(parallel_for_max_range_size)
    {
        auto numbers = std::vector<int>(32);
        int* ptr = numbers.data();
        int  len = (int)numbers.size();
        for (int i = 0; i < len; ++i)
            ptr[i] = i;

        rpp::Timer t;

        thread_pool pool{/*max_parallelism*/4};
        pool.parallel_for(0, len, 8, [&](int start, int end) {
            AssertThat(end-start, 8);
            for (int i = start; i < end; ++i) AssertThat(ptr[i], i);
        });
        
        pool.parallel_for(0, len, 2, [&](int start, int end) {
            AssertThat(end-start, 2);
            for (int i = start; i < end; ++i) AssertThat(ptr[i], i);
        });

        pool.parallel_for(0, len, 1, [&](int start, int end) {
            AssertThat(end-start, 1);
            for (int i = start; i < end; ++i) AssertThat(ptr[i], i);
        });

        double elapsed = t.elapsed();
        AssertLessOrEqual(elapsed, 0.2);
    }

    TestCase(parallel_for_max_range_size_unaligned)
    {
        auto numbers = std::vector<int>(17);
        int* ptr = numbers.data();
        int  len = (int)numbers.size();
        for (int i = 0; i < len; ++i)
            ptr[i] = i;
        
        thread_pool pool{/*max_parallelism*/4};
        rpp::Timer t;

        // [0,8); [8,16); [16,17)
        pool.parallel_for(0, len, 8, [&](int start, int end) {
            if      (start == 0)  { AssertThat(end-start, 8); AssertThat(end, 8); }
            else if (start == 8)  { AssertThat(end-start, 8); AssertThat(end, 16); }
            else if (start == 16) { AssertThat(end-start, 1); AssertThat(end, 17); }
            else AssertMsg(false, "invalid parallel_for range: [%d, %d)", start, end);
        });

        double elapsed = t.elapsed();
        AssertLessOrEqual(elapsed, 0.06);

        AssertEqual(pool.idle_tasks(), 3);
    }

    TestCase(parallel_for_performance)
    {
    #if YOCTO_LINUX || MIPS || __ANDROID__
        constexpr int COUNT = 81'234'567 / 10;
        constexpr int64 EXPECTED_SUM = 32995264630240LL;
    #else
        constexpr int COUNT = 81'234'567 / 10;
        constexpr int64 EXPECTED_SUM = 32995264630240LL;
    #endif

        auto numbers = std::vector<int>(COUNT);
        int* ptr = numbers.data();
        int len = (int)numbers.size();

        // Continuous Integration machines are virtualized,
        // so the parallelism are shared between VM's which can lead to invalid test results
        // Attempt to detect this and limit the number of tasks
        const int default_parallelism = rpp::num_physical_cores();
        thread_pool::set_global_max_parallelism(default_parallelism);
        if (default_parallelism > 8)
        {
            print_info("Limiting Max Parallelism to 8\n");
            thread_pool::set_global_max_parallelism(8);
        }

        print_info("PFOR pre-validation loop\n");
        parallel_for(0, len, 0, [&](int start, int end) {
            for (int i = start; i < end; ++i)
                ptr[i] = i;
        });

        print_info("PFOR validation loop\n");
        rpp::Timer t0;
        for (int i = 0; i < len; ++i) {
            AssertThat(ptr[i], i);
        }
        print_info("PFOR validation elapsed: %.3fms\n", t0.elapsed_millis());
        //#pragma loop(hint_parallel(0))
        //for (int i = 0; i < len; ++i)
        //    ptr[i] = rand() % 32768;
        //concurrency::parallel_for(0, len, [&](int index) {
        //    ptr[index] = index;
        //});

        print_info("PFOR perf loop\n");
        Timer timer1;

        std::atomic_int64_t sum { 0 };
        parallel_for(0, len, 0, [&](int start, int end) {
            int64 isum = 0;
            for (int i = start; i < end; ++i)
                isum += ptr[i];
            sum += isum; // only touch atomic int once
        });
        //#pragma loop(hint_parallel(0))
        //for (int i = 0; i < len; ++i)
        //    sum += ptr[i];
        //concurrency::parallel_for(size_t(0), numbers.size(), [&](int index) {
        //    sum += ptr[index];
        //});
        double parallel_elapsed = timer1.elapsed();
        print_info("ParallelFor  elapsed: %.4fs  result: %lld\n", parallel_elapsed, (int64)sum);
        AssertThat((int64)sum, EXPECTED_SUM);

        Timer timer2;
        int64 sum2 = 0;
        for (int i = 0; i < len; ++i)
            sum2 += ptr[i];
        double serial_elapsed = timer2.elapsed();

        print_info("Singlethread elapsed: %.4fs  result: %lld\n", serial_elapsed, sum2);
        AssertThat(sum2, EXPECTED_SUM);

        const int parallelism = thread_pool::global_max_parallelism();
        print_info("Test System # Max Parallelism: %d\n", parallelism);
        if (parallelism == 1)
        {
            // System has no parallelism at all, so there is going to be significant overhead!
            AssertLessOrEqual(parallel_elapsed, serial_elapsed+0.06);
        }
        else if (parallelism <= 2)
        {
            // if the system doesn't have enough parallelism, the overhead should be minimal
            AssertLessOrEqual(parallel_elapsed, serial_elapsed+0.005);
        }
        else
        {
            // no point running this under ASAN/TSAN/UBSAN, it will most likely always fail
            #if !RPP_SANITIZERS
                AssertLessOrEqual(parallel_elapsed, serial_elapsed + 0.001);
            #endif
        }

        print_info("Global Thread Pool idle tasks: %d\n", thread_pool::global().idle_tasks());
        print_info("Global Thread Pool active tasks: %d\n", thread_pool::global().active_tasks());
        thread_pool::set_global_max_parallelism(default_parallelism);
    }

    TestCase(parallel_foreach)
    {
        auto numbers = std::vector<int>(1337);
        parallel_foreach(numbers, [](int& n) {
            n = 1;
        });

        int checksum = 0;
        for (int i : numbers) checksum += i;
        AssertThat(checksum, 1337);
    }

    TestCaseExpectedEx(parallel_task_exception, std::logic_error)
    {
        int times_launched = 0; // this makes sure the threadpool loop doesn't retrigger our task
        auto task = rpp::parallel_task([&]() {
            AssertThat(times_launched, 0);
            ++times_launched;
            throw std::logic_error("aaargh!");
        });
        task.wait(); // @note this should rethrow
    }

    TestCase(parallel_task_reentrance)
    {
        int times_launched = 0;
        auto task = rpp::parallel_task([&] {
            ++times_launched;
            rpp::sleep_ms(5);
        });
        task.wait();
        AssertThat(times_launched, 1);

        task = rpp::parallel_task([&] {
            ++times_launched;
            rpp::sleep_ms(5);
        });
        task.wait();
        AssertThat(times_launched, 2);
    }

    TestCase(parallel_task_resurrection)
    {
        thread_pool::global().max_task_idle_time(0.05f);
        thread_pool::global().clear_idle_tasks();
        AssertThat(thread_pool::global().active_tasks(), 0);

        std::atomic_int times_launched {0};
        rpp::parallel_task([&] {
            times_launched += 1; 
        }).wait(rpp::Duration::from_millis(1000));
        AssertThat((int)times_launched, 1);

        print_info("Waiting for pool tasks to die naturally...\n");

        rpp::sleep_ms(100);

        print_info("Attempting pool task resurrection\n");
        rpp::parallel_task([&] { 
            times_launched += 1; 
        }).wait(rpp::Duration::from_millis(1000));
        AssertThat((int)times_launched, 2);

        // restore default
        thread_pool::global().max_task_idle_time();
    }

    TestCase(parallel_task_nested_nodeadlocks)
    {
        rpp::Timer t;
        std::atomic_int times_launched {0};

        auto func = [&]() {
            times_launched += 1;
            std::vector<pool_task_handle> subtasks {
                parallel_task([&]() { times_launched += 1; }),
                parallel_task([&]() { times_launched += 1; }),
                parallel_task([&]() { times_launched += 1; }),
                parallel_task([&]() { times_launched += 1; }),
                parallel_task([&]() { times_launched += 1; }),
            };

            for (auto& task : subtasks)
                AssertThat(task.wait(rpp::Duration::from_millis(5000)), wait_result::finished);
        };
        std::vector<pool_task_handle> main_tasks = {
            parallel_task(func),
            parallel_task(func),
            parallel_task(func),
            parallel_task(func),
        };

        // TODO: this test is failing, run some stress tests
        for (auto& task : main_tasks)
            AssertThat(task.wait(rpp::Duration::from_millis(5000)), wait_result::finished);

        int64 elapsed_ms = t.elapsed_ms();
        if (elapsed_ms >= 5000)
        {
            AssertFailed("a deadlock occurred - wait took: %lldms\n", elapsed_ms);
        }

        int expected = 4 * 6;
        AssertThat((int)times_launched, expected);
    }

    TestCase(pool_task_handle_copy_race_stress)
    {
        // Stress test for pool_task_handle concurrent copy + cleanup.
        //
        // Simulates a realistic scenario:
        //   Main thread:   Creates new task handles, "works" briefly, then calls
        //                  signal_finish_and_cleanup() — mimicking the worker thread
        //                  finishing a task.
        //   Waiter thread: Repeatedly copies the shared handle and calls wait()
        //                  on the copy — mimicking client code waiting for completion.
        //
        // The race window:
        //   Waiter:  pool_task_handle copy{shared}       // load ptr + inc_ref
        //   Main:    shared.signal_finish_and_cleanup()  // exchange→null, signal, dec_ref
        //   Main:    shared = new handle                 // old state now only held by copy
        //
        // Without proper strong-ref in wait(), the state could be freed
        // while the waiter is still accessing it.

        constexpr int ITERATIONS = 1'000;
        std::atomic_bool stop{false};
        std::atomic_int waits_finished{0};
        std::atomic_int waits_timeout{0};

        pool_task_handle shared{nullptr};

        // Waiter thread: copies shared handle and waits for completion
        std::thread waiter([&]() {
            while (!stop.load(std::memory_order_relaxed)) {
                // The wait here does a strong ref copy
                // which simulates the copy constructor's load + inc_ref sequence
                // use a lower wait period here to increase contention
                auto wait_result = shared.wait(rpp::Duration::from_micros(20), std::nothrow);
                if (wait_result == wait_result::finished) {
                    waits_finished.fetch_add(1, std::memory_order_relaxed);
                } else if (wait_result == wait_result::timeout) {
                    waits_timeout.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });

        for (int i = 0; i < ITERATIONS; ++i) {
            // Create a fresh task handle (refcount=1)
            shared = pool_task_handle{static_cast<pool_worker*>(nullptr)};
            // Small delay to give waiter a chance to copy
            rpp::sleep_us(100);
            // Simulate worker finishing: signal + cleanup (exchange→null, dec_ref)
            // Race: waiter may be between get() and inc_ref() in copy constructor
            shared.signal_finish_and_cleanup();
        }

        stop.store(true, std::memory_order_release);
        waiter.join();

        print_info("copy_race_stress: waits finished=%d, timeouts=%d  (%d iterations)\n",
                   (int)waits_finished, (int)waits_timeout, ITERATIONS);
        // Test passes if no crash, use-after-free, or sanitizer error occurred
    }

    TestCase(pool_task_handle_inc_ref_uaf_race)
    {
        // Reliably reproduces the load-then-increment UAF in pool_task_handle.
        //
        // === The Race ===
        //
        //   Copier (N threads):   p = other.ptr.load()         // (1) atomic load of raw ptr
        //                         --- race window ---
        //                         p->ref_count.fetch_add(1)    // (2) inc_ref: UB if p is freed
        //
        //   Destroyer (main):     ptr.exchange(nullptr)        // nulls shared.ptr atomically
        //                         ref_count.fetch_sub(1) → 0   // dec_ref: last reference gone
        //                         delete p                     // p freed ← copier still has it!
        //
        // (1) and (2) are individually atomic, but NOT atomic together.
        // Between them, the destroyer can run fully (exchange + dec_ref → 0 + delete).
        // When the copier resumes at (2), it reads/writes freed memory:
        //   - ASAN reports: heap-use-after-free (WRITE of size 4 on ref_count field)
        //   - Assertion fires: inc_ref sees old_refs==0
        //
        constexpr int ITERATIONS = 1'000;
        constexpr int N_COPIERS  = 4;

        std::atomic_bool stop{false};
        pool_task_handle shared{nullptr};

        // All threads start simultaneously for maximum initial contention
        std::latch ready{N_COPIERS + 1};

        std::vector<std::thread> copiers;
        copiers.reserve(N_COPIERS);
        for (int t = 0; t < N_COPIERS; ++t)
        {
            copiers.emplace_back([&]() {
                ready.arrive_and_wait(); // synchronize start with main + other copiers
                while (!stop.load(std::memory_order_relaxed))
                {
                    pool_task_handle copy{shared}; // THE RACING OPERATION: load + inc_ref // NOLINT(performance-unnecessary-copy-initialization)
                    (void)copy;                    // drop ref immediately → dec_ref
                }
            });
        }

        ready.arrive_and_wait(); // release all copiers at once

        for (int i = 0; i < ITERATIONS; ++i)
        {
            // Fresh handle: refcount=1 owned by shared
            shared = pool_task_handle{static_cast<pool_worker*>(nullptr)};
            // yield once — enough for a copier to load ptr but not yet inc_ref
            std::this_thread::yield();
            // Immediately destroy: exchange→null, dec_ref → 0, delete
            // Races with any copier that loaded ptr but hasn't called inc_ref yet
            shared.signal_finish_and_cleanup();
        }

        stop.store(true, std::memory_order_release);
        for (auto& t : copiers) t.join();

        print_info("inc_ref_uaf_race: completed %d iterations with %d copier threads\n",
                   ITERATIONS, N_COPIERS);
        // Test passes if no assertion, crash, or ASAN error occurs
    }

    // Rapid submit-wait cycles from 4 threads simultaneously.
    // Stresses the signal_finish_and_cleanup → set_current_task_and_unlock → is_running
    // path that caused the RadioMicrohard deadlock (pool worker stuck on finished.m mutex).
    // Timeout detection: if any worker gets stuck, the wait() calls will hang.
    TestCase(rapid_task_cycle_stress)
    {
        constexpr int TASKS_PER_THREAD = 500;
        std::atomic_int completed{0};
        std::latch go{4};

        auto submitter = [&] {
            go.arrive_and_wait();
            for (int i = 0; i < TASKS_PER_THREAD; ++i)
                rpp::parallel_task([&] { completed += 1; }).wait();
        };
        std::thread t1{submitter}, t2{submitter}, t3{submitter}, t4{submitter};
        t1.join(); t2.join(); t3.join(); t4.join();

        AssertThat((int)completed, 4 * TASKS_PER_THREAD);
    }

    // Nested async_task().get() — the exact pattern from the RadioMicrohard deadlock.
    // Outer pool worker blocks on inner future, requiring the pool to spawn/reuse
    // a different worker. Tests 2-deep and 3-deep nesting.
    TestCase(nested_async_task_get_no_deadlock)
    {
        rpp::Timer t;

        // 2-deep: outer task blocks waiting for inner task
        for (int i = 0; i < 100; ++i)
        {
            int r = rpp::async_task([]() -> int {
                return rpp::async_task([]() -> int { return 42; }).get();
            }).get();
            AssertThat(r, 42);
        }

        // 3-deep: requires 3 concurrent workers minimum
        for (int i = 0; i < 50; ++i)
        {
            int r = rpp::async_task([]() -> int {
                return rpp::async_task([]() -> int {
                    return rpp::async_task([]() -> int { return 99; }).get();
                }).get();
            }).get();
            AssertThat(r, 99);
        }

        if (t.elapsed_ms() >= 10'000)
            AssertFailed("nested async_task deadlock — took %lldms\n", t.elapsed_ms());
    }

    TestCase(pool_task_handle_fire_and_forget_race)
    {
        // Tests the async_task fire-and-forget pattern where
        // the pool_task_handle temporary is destroyed on the main thread
        // while the worker thread may still be in signal_finish_and_cleanup.
        //
        // The race window in dec_ref: main thread's fetch_sub vs
        // worker thread's operator delete at the same address (ref_count).
        // Fixed by __tsan_acquire annotation before delete.

        constexpr int ITERATIONS = 200;
        std::atomic_int completed{0};

        for (int i = 0; i < ITERATIONS; ++i)
        {
            auto future = rpp::async_task([&completed]() noexcept -> int {
                completed.fetch_add(1, std::memory_order_relaxed);
                return 0;
            });
            (void)future.get();
        }

        print_info("fire_and_forget_race: %d/%d iterations completed\n",
                   (int)completed, ITERATIONS);
        AssertThat((int)completed, ITERATIONS);
    }

};
