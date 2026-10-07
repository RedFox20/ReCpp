#include <rpp/tests.h>
#include <rpp/mutex.h>
#include <rpp/condition_variable.h>
#include <rpp/timepoint.h>
#include <rpp/threads.h>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>
#if _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <Windows.h>
#else
    #include <pthread.h>
    #include <sys/resource.h>
#endif

namespace
{
#if _WIN32
    struct srw_lock
    {
        SRWLOCK l = SRWLOCK_INIT;
        void lock() noexcept { AcquireSRWLockExclusive(&l); }
        void unlock() noexcept { ReleaseSRWLockExclusive(&l); }
    };
    using os_lock = srw_lock;
    constexpr const char* OS_LOCK = "SRWLOCK";
#elif __linux__ && !__ANDROID__
    struct adaptive_mutex
    {
        pthread_mutex_t m = PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP;
        void lock() noexcept { pthread_mutex_lock(&m); }
        void unlock() noexcept { pthread_mutex_unlock(&m); }
    };
    using os_lock = adaptive_mutex;
    constexpr const char* OS_LOCK = "pthread_adapt";
#endif

    // the futex_mutex before the spin: mark the word contended and sleep at once
    struct plain_futex_mutex
    {
        std::atomic_uint32_t word { 0 };
        void lock() noexcept
        {
            rpp::uint32 e = 0;
            if (word.compare_exchange_strong(e, 1, std::memory_order_acquire, std::memory_order_relaxed)) return;
            while (word.exchange(2, std::memory_order_acquire) != 0)
                rpp::cvar::wait(&word, 2);
        }
        void unlock() noexcept
        {
            if (word.exchange(0, std::memory_order_release) == 2)
                rpp::cvar::wake_one(&word);
        }
    };

    double cpu_seconds() noexcept
    {
    #if _WIN32
        FILETIME c, e, k, u;
        GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u);
        auto secs = [](FILETIME f) { return (double(f.dwHighDateTime) * 4294967296.0 + f.dwLowDateTime) * 1e-7; };
        return secs(k) + secs(u);
    #else
        rusage r {};
        getrusage(RUSAGE_SELF, &r);
        return double(r.ru_utime.tv_sec + r.ru_stime.tv_sec) + double(r.ru_utime.tv_usec + r.ru_stime.tv_usec) * 1e-6;
    #endif
    }

    struct sample { double wall_ns; double cpu_ns; double fairness; };
    struct scenario { int threads; int cs_work; int out_work; };

    unsigned spin_work(unsigned x, int n) noexcept
    {
        for (int i = 0; i < n; ++i) x = x * 1103515245u + 12345u;
        return x;
    }

    template<class Mutex>
    sample run_lock(scenario sc, int duration_ms)
    {
        alignas(64) Mutex m;
        alignas(64) unsigned shared[16] = {};
        std::atomic_int ready { 0 };
        std::atomic_bool go { false }, stop { false };
        std::vector<long long> ops(static_cast<size_t>(sc.threads), 0);
        std::vector<std::thread> pool;
        for (int t = 0; t < sc.threads; ++t)
        {
            pool.emplace_back([&, t] {
                unsigned x = unsigned(t) + 1;
                long long n = 0;
                ready.fetch_add(1);
                while (!go.load(std::memory_order_acquire)) rpp::yield();
                while (!stop.load(std::memory_order_relaxed))
                {
                    m.lock();
                    shared[n & 15] = spin_work(shared[n & 15] + x, sc.cs_work);
                    m.unlock();
                    x = spin_work(x, sc.out_work);
                    ++n;
                }
                ops[size_t(t)] = n + (x == 0xdeadbeef);
            });
        }
        while (ready.load() != sc.threads) rpp::yield();
        double c0 = cpu_seconds();
        rpp::TimePoint t0 = rpp::TimePoint::monotonic_now();
        go.store(true, std::memory_order_release);
        rpp::sleep_ms(duration_ms);
        stop.store(true);
        for (std::thread& th : pool) th.join();
        rpp::TimePoint t1 = rpp::TimePoint::monotonic_now();
        double c1 = cpu_seconds();
        long long total = 0, lo = ops[0], hi = ops[0];
        for (long long n : ops) { total += n; lo = std::min(lo, n); hi = std::max(hi, n); }
        return { double((t1 - t0).nsec) / double(total), (c1 - c0) * 1e9 / double(total), hi ? double(lo) / double(hi) : 0.0 };
    }

    // producers and consumers on one bounded queue: every item goes through a notify and most through a wake
    template<class Mutex, class CondVar>
    sample run_queue(scenario sc, int duration_ms)
    {
        struct alignas(64) queue
        {
            Mutex m;
            CondVar not_empty, not_full;
            int items[8] = {};
            int head = 0, size = 0;
            bool stop = false;
        } q;
        std::atomic_int ready { 0 };
        std::atomic_bool go { false };
        std::atomic<long long> consumed { 0 };
        std::vector<std::thread> pool;
        for (int t = 0; t < sc.threads; ++t)
        {
            pool.emplace_back([&] {
                ready.fetch_add(1);
                while (!go.load(std::memory_order_acquire)) rpp::yield();
                for (int i = 0;; ++i)
                {
                    std::unique_lock lock { q.m };
                    while (q.size == 8 && !q.stop) q.not_full.wait(lock);
                    if (q.stop) return;
                    q.items[(q.head + q.size++) & 7] = i;
                    q.not_empty.notify_one();
                }
            });
            pool.emplace_back([&] {
                long long n = 0;
                ready.fetch_add(1);
                while (!go.load(std::memory_order_acquire)) rpp::yield();
                for (;;)
                {
                    std::unique_lock lock { q.m };
                    while (q.size == 0 && !q.stop) q.not_empty.wait(lock);
                    if (q.stop) break;
                    q.head = (q.head + 1) & 7;
                    --q.size;
                    ++n;
                    q.not_full.notify_one();
                }
                consumed.fetch_add(n);
            });
        }
        while (ready.load() != 2 * sc.threads) rpp::yield();
        double c0 = cpu_seconds();
        rpp::TimePoint t0 = rpp::TimePoint::monotonic_now();
        go.store(true, std::memory_order_release);
        rpp::sleep_ms(duration_ms);
        { std::unique_lock lock { q.m }; q.stop = true; }
        q.not_empty.notify_all();
        q.not_full.notify_all();
        for (std::thread& th : pool) th.join();
        rpp::TimePoint t1 = rpp::TimePoint::monotonic_now();
        double c1 = cpu_seconds();
        double total = double(consumed.load());
        return { double((t1 - t0).nsec) / total, (c1 - c0) * 1e9 / total, 1.0 };
    }

    // two threads hand one turn back and forth, so every handoff is a notify, a wake and a relock
    template<class Mutex, class CondVar>
    sample run_pingpong(scenario, int)
    {
        constexpr int ROUNDS = 20'000;
        Mutex m;
        CondVar cv;
        int turn = 0;
        auto player = [&](int me) {
            for (int i = 0; i < ROUNDS; ++i)
            {
                std::unique_lock lock { m };
                while (turn != me) cv.wait(lock);
                turn = 1 - me;
                cv.notify_one();
            }
        };
        double c0 = cpu_seconds();
        rpp::TimePoint t0 = rpp::TimePoint::monotonic_now();
        std::thread other { player, 1 };
        player(0);
        other.join();
        rpp::TimePoint t1 = rpp::TimePoint::monotonic_now();
        double c1 = cpu_seconds();
        return { double((t1 - t0).nsec) / (2.0 * ROUNDS), (c1 - c0) * 1e9 / (2.0 * ROUNDS), 1.0 };
    }

    struct contender
    {
        const char* name;
        sample (*run)(scenario, int);
        std::vector<sample> samples;
    };

    void run_scenario(const char* kind, std::vector<contender>& all, scenario sc, int reps = 7)
    {
        for (contender& c : all) c.samples.clear();
        for (int rep = 0; rep < reps; ++rep)
            for (contender& c : all) c.samples.push_back(c.run(sc, 200));
        for (contender& c : all)
        {
            std::vector<sample>& s = c.samples;
            std::sort(s.begin(), s.end(), [](const sample& a, const sample& b) { return a.wall_ns < b.wall_ns; });
            const sample& med = s[s.size() / 2];
            std::printf("BENCH %-5s %-22s threads %2d cs %4d out %4d  wall %8.1f ns/op  cpu %8.1f ns/op  fair %.2f  (min %.1f max %.1f)\n",
                        kind, c.name, sc.threads, sc.cs_work, sc.out_work, med.wall_ns, med.cpu_ns, med.fairness,
                        s.front().wall_ns, s.back().wall_ns);
        }
        std::printf("BENCH\n");
        std::fflush(stdout);
    }
}

TestImpl(bench_mutex)
{
    TestInit(bench_mutex)
    {
    }

    TestCase(compare)
    {
        std::printf("BENCH cpus %u, sizeof rpp::mutex %zu, std::mutex %zu\n",
                    std::thread::hardware_concurrency(), sizeof(rpp::mutex), sizeof(std::mutex));
        std::vector<contender> locks {
            { "futex_plain", &run_lock<plain_futex_mutex>, {} },
            { "futex_polite", &run_lock<rpp::futex_mutex>, {} },
            { "std::mutex", &run_lock<std::mutex>, {} },
            { OS_LOCK, &run_lock<os_lock>, {} },
        };
        run_scenario("lock", locks, { 1, 0, 0 });
        for (int threads : { 2, 4, 8, 16 }) run_scenario("lock", locks, { threads, 0, 0 });
        for (int threads : { 2, 4, 8, 16 }) run_scenario("lock", locks, { threads, 20, 100 });
        for (int threads : { 4, 16 }) run_scenario("lock", locks, { threads, 1000, 1000 });

        using rpp_cv = rpp::condition_variable;
        std::vector<contender> queues {
            { "futex_plain+rpp_cv", &run_queue<plain_futex_mutex, rpp_cv>, {} },
            { "futex_polite+rpp_cv", &run_queue<rpp::futex_mutex, rpp_cv>, {} },
            { "std::mutex+std_cv", &run_queue<std::mutex, std::condition_variable>, {} },
            { "os_lock+rpp_cv", &run_queue<os_lock, rpp_cv>, {} },
        };
        for (int pairs : { 1, 2, 4 }) run_scenario("queue", queues, { pairs, 0, 0 });

        std::vector<contender> pingpongs {
            { "futex_plain+rpp_cv", &run_pingpong<plain_futex_mutex, rpp_cv>, {} },
            { "futex_polite+rpp_cv", &run_pingpong<rpp::futex_mutex, rpp_cv>, {} },
            { "std::mutex+std_cv", &run_pingpong<std::mutex, std::condition_variable>, {} },
            { "os_lock+rpp_cv", &run_pingpong<os_lock, rpp_cv>, {} },
        };
        run_scenario("pong", pingpongs, { 2, 0, 0 }, 5);
    }
};
