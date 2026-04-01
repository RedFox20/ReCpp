// override to OUR internal implementation
#define RPP_USE_STD_ATOMIC_SHARED_PTR 0
#include <rpp/atomic_shared_ptr.h>
#include <rpp/semaphore.h>
#include <rpp/tests.h>
#include <thread>
#include <vector>
#include <latch>

TestImpl(test_atomic_shared_ptr)
{
    TestInit(test_atomic_shared_ptr)
    {
    }

    TestCase(default_construct_is_null)
    {
        rpp::atomic_shared_ptr<int> asp;
        auto sp = asp.load();
        AssertTrue(sp == nullptr);
    }

    TestCase(construct_from_shared_ptr)
    {
        auto orig = std::make_shared<int>(42);
        rpp::atomic_shared_ptr<int> asp{orig};
        auto loaded = asp.load();
        AssertTrue(loaded != nullptr);
        AssertEqual(*loaded, 42);
        // orig and loaded both hold references
        AssertEqual(orig.use_count(), 3); // orig + asp internal + loaded
    }

    TestCase(store_and_load)
    {
        rpp::atomic_shared_ptr<int> asp;
        AssertTrue(asp.load() == nullptr);

        asp.store(std::make_shared<int>(10));
        auto sp = asp.load();
        AssertTrue(sp != nullptr);
        AssertEqual(*sp, 10);

        asp.store(std::make_shared<int>(20));
        sp = asp.load();
        AssertEqual(*sp, 20);

        asp.store(nullptr);
        AssertTrue(asp.load() == nullptr);
    }

    TestCase(exchange_returns_old)
    {
        rpp::atomic_shared_ptr<int> asp{std::make_shared<int>(1)};

        auto old = asp.exchange(std::make_shared<int>(2));
        AssertTrue(old != nullptr);
        AssertEqual(*old, 1);

        auto current = asp.load();
        AssertEqual(*current, 2);
    }

    TestCase(exchange_with_nullptr)
    {
        rpp::atomic_shared_ptr<int> asp{std::make_shared<int>(99)};

        auto old = asp.exchange(nullptr);
        AssertTrue(old != nullptr);
        AssertEqual(*old, 99);

        AssertTrue(asp.load() == nullptr);
    }

    TestCase(refcount_correctness)
    {
        auto sp = std::make_shared<int>(7);
        AssertEqual(sp.use_count(), 1);

        rpp::atomic_shared_ptr<int> asp{sp};
        AssertEqual(sp.use_count(), 2); // sp + asp internal

        auto loaded = asp.load();
        AssertEqual(sp.use_count(), 3); // sp + asp internal + loaded

        asp.store(nullptr);
        AssertEqual(sp.use_count(), 2); // sp + loaded

        loaded.reset();
        AssertEqual(sp.use_count(), 1); // sp only
    }

    TestCase(destroyed_when_last_ref_drops)
    {
        std::weak_ptr<int> weak;
        {
            auto sp = std::make_shared<int>(123);
            weak = sp;
            {
                rpp::atomic_shared_ptr<int> asp{std::move(sp)};
                AssertFalse(weak.expired());
                // asp goes out of scope here
            }
            AssertTrue(weak.expired());
        }
    }

    TestCase(concurrent_load_store)
    {
        rpp::atomic_shared_ptr<int> asp{std::make_shared<int>(0)};
        constexpr int NUM_ITERATIONS = 100;
        constexpr int NUM_READERS = 4;

        std::atomic_bool stop{false};
        std::atomic_int valid_reads{0};
        std::atomic_int invalid_reads{0};
        std::atomic_int readers_spinning{0};
        std::latch readers_ready{NUM_READERS};

        std::vector<std::thread> readers;
        readers.reserve(NUM_READERS);
        for (int i = 0; i < NUM_READERS; ++i)
        {
            readers.emplace_back([&]
            {
                readers_ready.arrive_and_wait();
                readers_spinning.fetch_add(1, std::memory_order_release);
                while (!stop.load(std::memory_order_acquire))
                {
                    auto sp = asp.load(std::memory_order_acquire);
                    if (sp)
                    {
                        int val = *sp;
                        if (val >= 0 && val <= NUM_ITERATIONS)
                            valid_reads.fetch_add(1, std::memory_order_relaxed);
                        else
                            invalid_reads.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }

        // wait until all readers have entered the while-loop
        while (readers_spinning.load(std::memory_order_acquire) < NUM_READERS)
            std::this_thread::yield();

        for (int i = 1; i <= NUM_ITERATIONS; ++i)
            asp.store(std::make_shared<int>(i), std::memory_order_release);

        stop.store(true, std::memory_order_release);
        for (auto& t : readers)
            t.join();

        AssertEqual(invalid_reads.load(), 0);
        AssertGreater(valid_reads.load(), 0);
        AssertEqual(*asp.load(std::memory_order_acquire), NUM_ITERATIONS);
    }

    TestCase(concurrent_exchange)
    {
        rpp::atomic_shared_ptr<int> asp{std::make_shared<int>(0)};
        constexpr int NUM_THREADS = 4;
        constexpr int EXCHANGES_PER_THREAD = 100;

        std::atomic_int total_non_null{0};
        std::vector<std::thread> threads;
        threads.reserve(NUM_THREADS);
        for (int t = 0; t < NUM_THREADS; ++t)
        {
            threads.emplace_back([&, t]
            {
                for (int i = 0; i < EXCHANGES_PER_THREAD; ++i)
                {
                    int val = t * EXCHANGES_PER_THREAD + i + 1;
                    auto old = asp.exchange(
                        std::make_shared<int>(val), std::memory_order_acq_rel);
                    if (old)
                        total_non_null.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }

        for (auto& t : threads)
            t.join();

        // every exchange received a non-null old value (initial was non-null)
        AssertEqual(total_non_null.load(), NUM_THREADS * EXCHANGES_PER_THREAD);
        AssertTrue(asp.load(std::memory_order_acquire) != nullptr);
    }

    // Reproduces the pool_task_handle lifecycle that caused the RadioMicrohard deadlock:
    //   Writer: store(new state) → yield → exchange(null) + lock finished + notify (signal_finish_and_cleanup)
    //   Readers: load() → finished.is_set()  (is_running check)
    // The deadlock was: a reader got stuck on finished.m of a freshly allocated state.
    // If allocator reuses memory from the just-freed state, and the mutex isn't properly
    // initialized, finished.m can appear locked (futex=2) to the reader.
    TestCase(pool_task_lifecycle_stress)
    {
        struct task_state
        {
            rpp::semaphore_once_flag finished;
        };

        constexpr int N = 5'000;
        rpp::atomic_shared_ptr<task_state> asp;
        std::atomic_bool stop{false};
        std::atomic_int checks{0};

        // 4 reader threads hammering load() + finished.is_set() — same as pool_worker::is_running()
        auto reader = [&] {
            while (!stop.load(std::memory_order_relaxed))
            {
                if (auto s = asp.load())
                {
                    (void)s->finished.is_set(); // locks finished.m briefly
                    checks.fetch_add(1, std::memory_order_relaxed);
                }
            }
        };
        std::thread r1{reader}, r2{reader}, r3{reader}, r4{reader}; // NOLINT(readability-isolate-declaration)

        for (int i = 0; i < N; ++i)
        {
            asp.store(std::make_shared<task_state>()); // new state (may reuse freed memory)
            std::this_thread::yield();
            // signal_finish_and_cleanup pattern: take → lock finished → notify → destroy
            if (auto taken = asp.exchange(nullptr))
            {
                auto lock = taken->finished.spin_lock();
                taken->finished.notify_all(lock);
            }
        }

        stop.store(true, std::memory_order_release);
        r1.join(); r2.join(); r3.join(); r4.join();
        AssertGreater(checks.load(), 0);
    }

    TestCase(weak_ptr_default_construct_is_expired)
    {
        rpp::atomic_weak_ptr<int> awp;
        auto wp = awp.load();
        AssertTrue(wp.expired());
        AssertTrue(wp.lock() == nullptr);
    }

    TestCase(weak_ptr_construct_from_weak_ptr)
    {
        auto sp = std::make_shared<int>(42);
        std::weak_ptr<int> wp = sp;
        rpp::atomic_weak_ptr<int> awp{wp};
        auto loaded = awp.load();
        AssertFalse(loaded.expired());
        auto locked = loaded.lock();
        AssertTrue(locked != nullptr);
        AssertEqual(*locked, 42);
    }

    TestCase(weak_ptr_store_and_load)
    {
        rpp::atomic_weak_ptr<int> awp;
        AssertTrue(awp.load().expired());

        auto sp1 = std::make_shared<int>(10);
        awp.store(sp1);
        auto loaded = awp.load().lock();
        AssertTrue(loaded != nullptr);
        AssertEqual(*loaded, 10);

        auto sp2 = std::make_shared<int>(20);
        awp.store(sp2);
        loaded = awp.load().lock();
        AssertEqual(*loaded, 20);

        awp.store(std::weak_ptr<int>{});
        AssertTrue(awp.load().expired());
    }

    TestCase(weak_ptr_exchange_returns_old)
    {
        auto sp1 = std::make_shared<int>(1);
        rpp::atomic_weak_ptr<int> awp{std::weak_ptr<int>{sp1}};

        auto sp2 = std::make_shared<int>(2);
        auto old = awp.exchange(sp2);
        auto old_locked = old.lock();
        AssertTrue(old_locked != nullptr);
        AssertEqual(*old_locked, 1);

        auto current = awp.load().lock();
        AssertEqual(*current, 2);
    }

    TestCase(weak_ptr_expires_when_shared_ptr_dies)
    {
        rpp::atomic_weak_ptr<int> awp;
        {
            auto sp = std::make_shared<int>(99);
            awp.store(sp);
            AssertFalse(awp.load().expired());
        }
        // sp destroyed, weak_ptr should be expired
        AssertTrue(awp.load().expired());
        AssertTrue(awp.load().lock() == nullptr);
    }

    TestCase(weak_ptr_assign_nullptr_resets)
    {
        auto sp = std::make_shared<int>(5);
        rpp::atomic_weak_ptr<int> awp{std::weak_ptr<int>{sp}};
        AssertFalse(awp.load().expired());

        awp = nullptr;
        AssertTrue(awp.load().expired());
    }

    TestCase(weak_ptr_concurrent_load_store)
    {
        auto sp = std::make_shared<int>(0);
        rpp::atomic_weak_ptr<int> awp{std::weak_ptr<int>{sp}};
        constexpr int NUM_ITERATIONS = 100;
        constexpr int NUM_READERS = 4;

        std::atomic_bool stop{false};
        std::atomic_int valid_reads{0};
        std::atomic_int readers_spinning{0};
        std::latch readers_ready{NUM_READERS};

        // keep shared_ptrs alive for the duration of the test
        std::vector<std::shared_ptr<int>> kept_alive;
        kept_alive.reserve(NUM_ITERATIONS + 1);
        kept_alive.push_back(sp);

        std::vector<std::thread> readers;
        readers.reserve(NUM_READERS);
        for (int i = 0; i < NUM_READERS; ++i)
        {
            readers.emplace_back([&]
            {
                readers_ready.arrive_and_wait();
                readers_spinning.fetch_add(1, std::memory_order_release);
                while (!stop.load(std::memory_order_acquire))
                {
                    auto wp = awp.load(std::memory_order_acquire);
                    if (auto locked = wp.lock())
                    {
                        int val = *locked;
                        if (val >= 0 && val <= NUM_ITERATIONS)
                            valid_reads.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }

        // wait until all readers have entered the while-loop
        while (readers_spinning.load(std::memory_order_acquire) < NUM_READERS)
            std::this_thread::yield();

        for (int i = 1; i <= NUM_ITERATIONS; ++i)
        {
            auto new_sp = std::make_shared<int>(i);
            kept_alive.push_back(new_sp);
            awp.store(new_sp, std::memory_order_release);
        }

        stop.store(true, std::memory_order_release);
        for (auto& t : readers)
            t.join();

        AssertGreater(valid_reads.load(), 0);
        auto final_locked = awp.load(std::memory_order_acquire).lock();
        AssertTrue(final_locked != nullptr);
        AssertEqual(*final_locked, NUM_ITERATIONS);
    }
};
