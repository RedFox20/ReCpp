#include <rpp/tests.h>
#include <rpp/mutex.h>
#include <rpp/condition_variable.h>
#include <rpp/threads.h> // rpp::yield
#include <rpp/timepoint.h> // rpp::TimePoint, rpp::seconds, rpp::millis
#include <atomic>
#include <deque>
#include <memory> // std::shared_ptr
#include <mutex> // std::lock_guard, std::unique_lock
#include <thread>
#include <type_traits> // std::is_trivially_destructible_v, std::is_same_v, std::conditional_t
#include <vector>

TestImpl(test_mutex)
{
    TestInit(test_mutex)
    {
    }

    class SimpleValue : public rpp::synchronizable<SimpleValue>
    {
        std::string value;
        rpp::mutex mutex;
    public:
        auto& get_mutex() { return mutex; }
        auto& get_ref() { return value; }
    };

    // a derived type which forgot the two accessors, so the constraint has something to reject
    class MissingAccessors : public rpp::synchronizable<MissingAccessors>
    {
        std::string value;
    };

    // accessors of the wrong type: SyncableType must reject these, because synchronize_guard
    // hard-errors on a unique_lock<int> and on a reference to void
    class WrongAccessorTypes : public rpp::synchronizable<WrongAccessorTypes>
    {
    public:
        static int get_mutex() noexcept { return 0; } // static, because it touches no member
        static void get_ref() noexcept {}
    };

    // a get_ref() which returns by value, so the guard would hand out a reference to a temporary
    class ReturnsByValue : public rpp::synchronizable<ReturnsByValue>
    {
        rpp::mutex mutex;
    public:
        auto& get_mutex() noexcept { return mutex; }
        static std::string get_ref() noexcept { return {}; } // static, because it touches no member
    };

    // a mutex returned by value, which spin_lock cannot bind to its reference parameter
    struct CopiedMutex { void lock() {} void unlock() {} static bool try_lock() { return true; } };
    class MutexByValue : public rpp::synchronizable<MutexByValue>
    {
        std::string value;
    public:
        static CopiedMutex get_mutex() noexcept { return {}; }
        auto& get_ref() noexcept { return value; }
    };

    // a try_lock() result whose boolean operators need an lvalue, which spin_lock has not
    struct LvalueOnlyBool
    {
        explicit operator bool() & noexcept { return true; }
        bool operator!() & noexcept { return false; }
    };
    struct LvalueBoolTryLock
    {
        void lock() {}
        void unlock() {}
        static LvalueOnlyBool try_lock() { return {}; }
    };
    class MutexTryLockLvalueOnly : public rpp::synchronizable<MutexTryLockLvalueOnly>
    {
        std::string value;
        LvalueBoolTryLock mutex;
    public:
        auto& get_mutex() noexcept { return mutex; }
        auto& get_ref() noexcept { return value; }
    };

    // a value type whose operator& returns something else, which the guard must not call
    struct OddAddress
    {
        int value = 0;
        int* operator&() noexcept { return &value; }
    };
    class OverloadedAddressOf : public rpp::synchronizable<OverloadedAddressOf>
    {
        rpp::mutex mutex;
        OddAddress value;
    public:
        auto& get_mutex() noexcept { return mutex; }
        auto& get_ref() noexcept { return value; }
    };

    // a try_lock() whose result reads as a bool but cannot be negated, which spin_lock does
    struct NoNegate
    {
        explicit operator bool() const noexcept { return true; }
        bool operator!() const = delete;
    };
    struct ProxyTryLock { void lock() {} void unlock() {} static NoNegate try_lock() { return {}; } };
    class MutexTryLockNoNegate : public rpp::synchronizable<MutexTryLockNoNegate>
    {
        std::string value;
        ProxyTryLock mutex;
    public:
        auto& get_mutex() noexcept { return mutex; }
        auto& get_ref() noexcept { return value; }
    };

    // a get_ref() returning a reference to an array, which decays to a pointer in value_type
    class ArrayRef : public rpp::synchronizable<ArrayRef>
    {
        rpp::mutex mutex;
        int values[3] {};
    public:
        auto& get_mutex() noexcept { return mutex; }
        auto& get_ref() noexcept { return values; }
    };

    // an lvalue ref-qualified get_ref(), which the guard only ever calls on an lvalue
    class RefQualified : public rpp::synchronizable<RefQualified>
    {
        rpp::mutex mutex;
        std::string value;
    public:
        auto& get_mutex() & noexcept { return mutex; }
        auto& get_ref() & noexcept { return value; }
    };

    // a const get_ref(), which the guard cannot hand out as a plain value_type&
    class ConstRef : public rpp::synchronizable<ConstRef>
    {
        rpp::mutex mutex;
        int value = 0;
    public:
        auto& get_mutex() noexcept { return mutex; }
        const int& get_ref() const noexcept { return value; }
    };

    // a volatile get_ref(), which the guard cannot hand out as a plain value_type&
    class VolatileRef : public rpp::synchronizable<VolatileRef>
    {
        rpp::mutex mutex;
        volatile int value = 0;
    public:
        auto& get_mutex() noexcept { return mutex; }
        volatile int& get_ref() noexcept { return value; }
    };

    // a try_lock() which returns nothing, so spin_lock would hard-error on `!m.try_lock()`
    struct VoidTryLock { void lock() {} void unlock() {} static void try_lock() {} };
    class MutexTryLockReturnsVoid : public rpp::synchronizable<MutexTryLockReturnsVoid>
    {
        std::string value;
        VoidTryLock mutex;
    public:
        auto& get_mutex() noexcept { return mutex; }
        auto& get_ref() noexcept { return value; }
    };

    // a mutex with no try_lock(), which spin_lock calls before it suspends the thread
    struct NoTryLock { void lock() {} void unlock() {} };
    class MutexWithoutTryLock : public rpp::synchronizable<MutexWithoutTryLock>
    {
        std::string value;
        NoTryLock mutex;
    public:
        auto& get_mutex() noexcept { return mutex; }
        auto& get_ref() noexcept { return value; }
    };

    // whether guard() is callable, which is what SyncableType decides
    template<class T> static constexpr bool has_guard = requires(T t) { t.guard(); };

    TestCase(syncable_type_answers_for_a_complete_type)
    {
        // the concept is public API, so a consumer can ask it about its own type
        static_assert(rpp::SyncableType<SimpleValue>);
        static_assert(rpp::SyncableType<rpp::synchronized<std::string>>);
        static_assert(!rpp::SyncableType<int>);
        static_assert(!rpp::SyncableType<MissingAccessors>);

        // an accessor of the wrong type fails the concept, not synchronize_guard
        static_assert(!rpp::SyncableType<WrongAccessorTypes>);
        static_assert(!rpp::SyncableType<ReturnsByValue>);
        static_assert(!rpp::SyncableType<MutexByValue>);
        static_assert(!rpp::SyncableType<MutexWithoutTryLock>);
        static_assert(!rpp::SyncableType<MutexTryLockReturnsVoid>);
        static_assert(!rpp::SyncableType<MutexTryLockNoNegate>);
        static_assert(!rpp::SyncableType<MutexTryLockLvalueOnly>);
        static_assert(!rpp::SyncableType<VolatileRef>);
        static_assert(!rpp::SyncableType<ConstRef>);
        static_assert(!rpp::SyncableType<ArrayRef>);
        // an lvalue ref-qualified accessor is the shape the guard uses, so it works
        static_assert(rpp::SyncableType<RefQualified>);

        // SyncableType constrains every synchronizable member, so a derived type which
        // forgot the accessors still compiles and only loses guard()
        static_assert(has_guard<SimpleValue>);
        static_assert(has_guard<rpp::synchronized<std::string>>);
        static_assert(!has_guard<MissingAccessors>);
        static_assert(!has_guard<WrongAccessorTypes>);
        static_assert(!has_guard<ReturnsByValue>);
        static_assert(!has_guard<MutexByValue>);
        static_assert(!has_guard<MutexWithoutTryLock>);
        static_assert(!has_guard<MutexTryLockReturnsVoid>);
        static_assert(!has_guard<MutexTryLockNoNegate>);
        static_assert(!has_guard<MutexTryLockLvalueOnly>);
        static_assert(!has_guard<VolatileRef>);
        static_assert(!has_guard<ConstRef>);
        static_assert(!has_guard<ArrayRef>);
        static_assert(has_guard<RefQualified>);

        SimpleValue value;
        auto guard = value.guard();
        AssertThat(guard.owns_lock(), true);
    }

    TestCase(sync_guard_ignores_an_overloaded_address_of)
    {
        // operator-> hands out the real address, so a hijacking operator& never runs
        OverloadedAddressOf odd;
        auto guard = odd.guard();
        guard->value = 7;
        AssertThat(guard->value, 7);
    }

    TestCase(sync_guard_can_lock_simple_value)
    {
        SimpleValue simple;

        *simple = "Testing operator*";
        AssertEqual(*simple, "Testing operator*");

        simple->assign("Testing operator->");
        AssertEqual(*simple, "Testing operator->");

        *simple = "Testing get()";
        std::string value = (*simple).get();
        AssertEqual(value, "Testing get()");

        // 1. lock and set First value
        // 2. spawn a thread which sets Second value
        // 3. check in a loop that it is not modified
        // 4. unlock the value and join the thread
        // 5. ensure it has the Second value
        auto guard = simple.guard(); // 1
        guard = "First value";
        auto t = std::thread([&] { // 2
            *simple = "Second value";
        });
        for (int i = 0; i < 10; ++i) { // 3 
            AssertEqual(*guard, "First value");
            sleep(1);
        }
        guard.unlock(); // 4
        t.join();
        AssertEqual(*simple, "Second value"); // 5
    }

    template<class T, class Mutex = rpp::mutex>
    class SafeVector : public rpp::synchronizable<SafeVector<T, Mutex>>
    {
    public:
        std::vector<T> value;
        Mutex mutex;
        auto& get_mutex() noexcept { return mutex; }
        auto& get_ref() noexcept { return value; }
    };

    TestCase(sync_guard_can_lock_vector)
    {
        SafeVector<int> vec;
        *vec = {1,2,3};

        AssertEqual(vec->size(), 3);
        AssertEqual(vec->at(0), 1);
        AssertEqual(vec->at(1), 2);
        AssertEqual(vec->at(2), 3);
        AssertEqual(*vec, std::vector<int>({1,2,3}));

        std::vector<int> iterated_values;
        for (auto& v : *vec)
            iterated_values.push_back(v);
        AssertEqual(iterated_values.size(), 3u);
        AssertEqual(iterated_values[0], 1);
        AssertEqual(iterated_values[1], 2);
        AssertEqual(iterated_values[2], 3);

        // the guard will prevent modification in background thread
        auto guard = vec.guard();
        auto t = std::thread([&]
        {
            vec->push_back(4);
            vec->push_back(5);
            vec->push_back(6);
        });

        for (int i = 0; i < 10; ++i)
        {
            AssertEqual(*guard, std::vector<int>({1,2,3}));
            sleep(1);
        }

        guard.unlock();
        t.join();

        AssertEqual(*vec, std::vector<int>({1,2,3,4,5,6}));
    };

    TestCase(sync_guard_holds_lock_during_iteration)
    {
        SafeVector<int, rpp::recursive_mutex> vec;
        *vec = {1,2,3};

        // try to acquire lock and modify the vector
        auto task = std::thread([&]
        {
            sleep(5);
            auto guard = vec.guard();
            guard->push_back(4);
            guard->push_back(5);
            guard->push_back(6);
        });

        // immediately acquire lock and slowly iterate over the vector
        for (auto& v : *vec)
        {
            sleep(10);
            AssertTrue((v == 1 || v == 2 || v == 3));
            AssertEqual(vec->size(), 3u);
        }

        // now lock should be released and vec will be modified
        task.join();
        AssertEqual(*vec, std::vector<int>({1,2,3,4,5,6}));
    }

    class WithSetMethod : public rpp::synchronizable<WithSetMethod>
    {
    public:
        std::string value;
        rpp::mutex mutex;
        bool called_set = false;

        WithSetMethod(std::string&& value) : value(std::move(value)) {}

        auto& get_mutex() { return mutex; }
        auto& get_ref() { return value; }

        template<class U> void set(U&& new_value) { 
            value = std::forward<U>(new_value);
            called_set = true;
        }
    };

    TestCase(sync_guard_uses_set_method_on_synced_type)
    {
        WithSetMethod var { "Initial value" };
        AssertEqual(*var, "Initial value");
        AssertFalse(var.called_set);

        *var = "Testing operator set()";
        AssertEqual(*var, "Testing operator set()");
        AssertTrue(var.called_set);
        var.called_set = false;
    }

    TestCase(sync_guard_locks_during_function_call)
    {
        WithSetMethod var { "Initial value" };

        auto t = std::thread([&]{
            sleep(5);
            *var = "Setting new value";
        });

        // will hold the lock for a long time
        auto fun = [&](const std::string& s)
        {
            sleep(10);
            AssertEqual(s, "Initial value");
        };
        fun(*var);

        t.join();
        AssertEqual(*var, "Setting new value");
    }

    class WithLongFunction : public rpp::synchronizable<WithLongFunction>
    {
    public:
        struct ValueType : public std::string
        {
            std::atomic_int associated_value = 0;
            int set_value_slow(int value, int sleep_for)
            {
                associated_value = value;
                sleep(sleep_for);
                return associated_value;
            }
        };
        ValueType value;
        rpp::mutex mutex;
        auto& get_mutex() { return mutex; }
        auto& get_ref() { return value; }
    };

    TestCase(sync_guard_locks_during_long_function_call)
    {
        WithLongFunction var;
        var->assign("Initial value");

        auto task = std::thread([&]
        {
            sleep(5);
            var->associated_value = 2;
        });

        // sets the value and holds the lock for a long time
        // the task will then enter the mutex and will be blocked,
        // unable to overwrite the value
        AssertEqual(var->set_value_slow(/*value*/1, /*sleep*/20), 1);

        task.join();
        AssertEqual(var->associated_value, 2);
    }

    TestCase(synchronized_var)
    {
        rpp::synchronized<std::string> str { "Initial value" };
        AssertEqual(*str, "Initial value");

        *str = "Testing operator*";
        AssertEqual(*str, "Testing operator*");

        str->assign("Testing operator->");
        AssertEqual(*str, "Testing operator->");

        *str = "Testing get()";
        std::string value = (*str).get();
        AssertEqual(value, "Testing get()");

        auto guard = str.guard();
        guard = "First value";
        auto t = std::thread([&]
        {
            *str = "Second value";
        });
        for (int i = 0; i < 10; ++i)
        {
            AssertEqual(*guard, "First value");
            sleep(1);
        }
        guard.unlock();
        t.join();
        AssertEqual(*str, "Second value");
    }

#if !RPP_HAS_CRITICAL_SECTION_MUTEX // FreeRTOS constructs it at runtime, and Cortex-M try_lock() always succeeds
    // a static mutex runs no constructor and no destructor, so a static initializer or an atexit handler can lock it
    TestCase(mutex_is_one_constant_initialized_word)
    {
        static constinit rpp::mutex m;
        std::lock_guard guard { m };
        bool taken = true;
        std::thread([&] { taken = m.try_lock(); }).join(); // a std::mutex owner which calls try_lock() is undefined
        AssertThat(taken, false);
    #if __linux__ || _MSC_VER
        AssertThat(std::is_trivially_destructible_v<rpp::mutex>, true);
        AssertThat(sizeof(m), sizeof(std::conditional_t<std::is_same_v<rpp::mutex, rpp::futex_mutex>, rpp::uint32, void*>));
    #endif
    }

    TestCase(try_lock_fails_while_another_thread_holds_the_mutex)
    {
        rpp::mutex m;
        auto try_lock_on_another_thread = [&] {
            bool taken = false;
            std::thread([&] { if ((taken = m.try_lock())) m.unlock(); }).join();
            return taken;
        };
        m.lock();
        AssertThat(try_lock_on_another_thread(), false);
        m.unlock();
        AssertThat(try_lock_on_another_thread(), true);
    }

    TestCase(spin_lock_for_gives_up_on_a_held_mutex)
    {
        rpp::mutex m;
        std::unique_lock held { m };
        bool owned = true;
        std::thread([&] { owned = rpp::spin_lock_for(m, rpp::millis(2)).owns_lock(); }).join();
        AssertThat(owned, false);
        held.unlock();
        AssertThat(rpp::spin_lock_for(m, rpp::millis(2)).owns_lock(), true);
    }

    // counts the lockers inside the mutex, so an overlap shows even when the count comes out right
    struct guarded_counter
    {
        rpp::mutex m;
        std::atomic_int inside { 0 };
        std::atomic_int overlaps { 0 };
        int count = 0;
        void enter() noexcept
        {
            if (inside.fetch_add(1) != 0) overlaps.fetch_add(1);
            ++count;
            inside.fetch_sub(1);
        }
    };

    // the guard sleeps on its own word, so a broken mutex cannot stall it. An abandoned thread keeps `state` alive
    struct thread_group
    {
        std::shared_ptr<std::atomic_uint32_t> finished = std::make_shared<std::atomic_uint32_t>(0u);
        std::vector<std::thread> threads;

        template<class State, class Body>
        thread_group(int n, const std::shared_ptr<State>& state, Body body)
        {
            for (int i = 0; i < n; ++i)
            {
                threads.emplace_back([state, body, i, finished = finished] {
                    body(*state, i);
                    finished->fetch_add(1);
                    rpp::cvar::wake_all(finished.get());
                });
            }
        }

        /// @returns false when a thread misses the one second hang guard
        bool join()
        {
            const rpp::TimePoint deadline = rpp::TimePoint::monotonic_now() + rpp::seconds(1);
            for (rpp::uint32 done; (done = finished->load()) != threads.size();)
            {
                const rpp::Duration left = deadline - rpp::TimePoint::monotonic_now();
                if (left.nsec <= 0)
                {
                    for (std::thread& t : threads) t.detach();
                    return false;
                }
                (void)rpp::cvar::wait_for(finished.get(), done, left);
            }
            for (std::thread& t : threads) t.join();
            return true;
        }
    };

    // a futex_mutex word reads contended once a locker marked it to sleep. Another mutex shows no state, so it gets a few yields
    /// @returns false when no locker marked the mutex within the hang guard
    static bool wait_until_contended(const rpp::mutex& m)
    {
        if constexpr (std::is_same_v<rpp::mutex, rpp::futex_mutex>)
        {
            const auto* word = reinterpret_cast<const std::atomic_uint32_t*>(&m);
            const rpp::TimePoint deadline = rpp::TimePoint::monotonic_now() + rpp::seconds(1); // a hang guard, a locker ends it
            while (word->load() != 2u && rpp::TimePoint::monotonic_now() < deadline)
                rpp::yield();
            return word->load() == 2u;
        }
        else
        {
            for (int i = 0; i < 10; ++i) rpp::yield();
            return true;
        }
    }

    static void lock_and_enter(guarded_counter& s, int) noexcept
    {
        std::lock_guard guard { s.m };
        s.enter();
    }

    TestCase(contended_lockers_never_overlap)
    {
        auto s = std::make_shared<guarded_counter>();
        thread_group lockers { 8, s, [](guarded_counter& s, int) {
            for (int i = 0; i < 5'000; ++i) lock_and_enter(s, i);
        }};
        AssertThat(lockers.join(), true);
        AssertThat(s->overlaps.load(), 0);
        AssertThat(s->count, 40'000);
    }

    // many lockers, so most of them sleep and wake many times
    TestCase(oversubscribed_lockers_all_finish)
    {
        auto s = std::make_shared<guarded_counter>();
        thread_group lockers { 32, s, [](guarded_counter& s, int) {
            for (int i = 0; i < 500; ++i) lock_and_enter(s, i);
        }};
        AssertThat(lockers.join(), true);
        AssertThat(s->overlaps.load(), 0);
        AssertThat(s->count, 16'000);
    }

    // several lockers sleep on the held mutex, and each unlock must wake the next one
    TestCase(every_sleeping_locker_wakes)
    {
        bool woke = true;
        for (int round = 0; woke && round < 20; ++round)
        {
            auto s = std::make_shared<guarded_counter>();
            s->m.lock();
            thread_group lockers { 6, s, &lock_and_enter };
            const bool contended = wait_until_contended(s->m);
            for (int i = 0; i < 10; ++i) rpp::yield(); // more lockers reach the sleep
            s->m.unlock();
            woke = lockers.join() && contended && s->count == 6 && s->overlaps == 0;
        }
        AssertThat(woke, true);
    }

    // a holder which yields inside the lock makes the others sleep, so every handoff goes through a wake
    TestCase(a_slow_holder_hands_the_mutex_to_sleeping_lockers)
    {
        auto s = std::make_shared<guarded_counter>();
        thread_group lockers { 6, s, [](guarded_counter& s, int) {
            for (int i = 0; i < 300; ++i)
            {
                std::lock_guard guard { s.m };
                s.enter();
                rpp::yield();
            }
        }};
        AssertThat(lockers.join(), true);
        AssertThat(s->overlaps.load(), 0);
        AssertThat(s->count, 1'800);
    }

    // a failed try_lock() keeps the mark which makes the next unlock wake a sleeping locker
    TestCase(a_failed_try_lock_keeps_the_sleeping_locker_wakeable)
    {
        auto s = std::make_shared<guarded_counter>();
        s->m.lock();
        thread_group locker { 1, s, &lock_and_enter };
        AssertThat(wait_until_contended(s->m), true);
        bool taken = true;
        std::thread([&] { taken = s->m.try_lock(); }).join();
        AssertThat(taken, false);
        s->m.unlock();
        AssertThat(locker.join(), true);
        AssertThat(s->count, 1);
    }

    TestCase(lock_try_lock_and_spin_lock_for_exclude_each_other)
    {
        auto s = std::make_shared<guarded_counter>();
        thread_group lockers { 6, s, [](guarded_counter& s, int t) {
            for (int i = 0; i < 2'000; ++i)
            {
                if (t % 3 == 0)
                {
                    lock_and_enter(s, i);
                }
                else if (t % 3 == 1)
                {
                    while (!s.m.try_lock()) rpp::yield();
                    s.enter();
                    s.m.unlock();
                }
                else if (std::unique_lock lock = rpp::spin_lock_for(s.m, rpp::millis(2)); lock.owns_lock())
                {
                    s.enter();
                }
                else
                {
                    lock_and_enter(s, i);
                }
            }
        }};
        AssertThat(lockers.join(), true);
        AssertThat(s->overlaps.load(), 0);
        AssertThat(s->count, 12'000);
    }

    // the next owner may free the mutex while the last unlock still runs, so unlock() never touches it after the release
    TestCase(a_locker_can_free_the_mutex_right_after_the_unlock)
    {
        bool freed = true;
        for (int round = 0; freed && round < 100; ++round)
        {
            auto s = std::make_shared<rpp::mutex*>(new rpp::mutex);
            rpp::mutex* m = *s;
            m->lock();
            thread_group freer { 1, s, [](rpp::mutex*& m, int) {
                m->lock();
                m->unlock();
                delete m;
            }};
            const bool contended = wait_until_contended(*m);
            m->unlock();
            freed = freer.join() && contended;
        }
        AssertThat(freed, true);
    }

    // packed mutexes share a cache line and a kernel wait bucket, so a wake must reach the word it names
    TestCase(adjacent_mutexes_wake_their_own_lockers)
    {
        struct packed { rpp::mutex m[8]; int count[8] = {}; };
        auto s = std::make_shared<packed>();
        thread_group lockers { 8, s, [](packed& s, int t) {
            const int next = (t + 1) % 8;
            const int a = next < t ? next : t, b = next < t ? t : next; // one lock order, so no deadlock
            for (int i = 0; i < 2'000; ++i)
            {
                std::lock_guard first { s.m[a] };
                std::lock_guard second { s.m[b] };
                ++s.count[a];
                ++s.count[b];
            }
        }};
        AssertThat(lockers.join(), true);
        for (int count : s->count) AssertThat(count, 4'000);
    }

    // a bounded queue sleeps on both sides, and every wait relocks the mutex under contention
    TestCase(a_bounded_queue_hands_every_item_to_one_consumer)
    {
        struct queue
        {
            rpp::mutex m;
            rpp::condition_variable not_empty, not_full;
            std::deque<int> items;
            long long sum = 0;
        };
        auto s = std::make_shared<queue>();
        thread_group workers { 8, s, [](queue& q, int t) {
            for (int i = 0; i < 1'000; ++i)
            {
                std::unique_lock lock { q.m };
                if (t < 4)
                {
                    q.not_full.wait(lock, [&] { return q.items.size() < 8; });
                    q.items.push_back(i);
                    q.not_empty.notify_one();
                }
                else
                {
                    q.not_empty.wait(lock, [&] { return !q.items.empty(); });
                    q.sum += q.items.front();
                    q.items.pop_front();
                    q.not_full.notify_one();
                }
            }
        }};
        AssertThat(workers.join(), true);
        AssertThat(s->sum, 4LL * 499'500); // each producer pushes 0 to 999
        AssertThat(s->items.empty(), true);
    }
#endif
};