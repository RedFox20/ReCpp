#include <rpp/atomic_shared_ptr.h>
#include <rpp/tests.h>
#include <thread>
#include <vector>

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

        std::vector<std::thread> readers;
        readers.reserve(NUM_READERS);
        for (int i = 0; i < NUM_READERS; ++i)
        {
            readers.emplace_back([&]
            {
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
};
