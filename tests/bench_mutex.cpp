#include <rpp/tests.h>
#include <rpp/mutex.h>
#include <rpp/timepoint.h>
#include <rpp/threads.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>
#if _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <Windows.h>
    #include <intrin.h>
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
    unsigned long release_unheld_srw_lock() noexcept
    {
        SRWLOCK unheld = SRWLOCK_INIT;
        __try { ReleaseSRWLockExclusive(&unheld); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
        return 0;
    }
    struct win_critical_section
    {
        CRITICAL_SECTION cs;
        win_critical_section() noexcept { InitializeCriticalSection(&cs); }
        ~win_critical_section() noexcept { DeleteCriticalSection(&cs); }
        void lock() noexcept { EnterCriticalSection(&cs); }
        void unlock() noexcept { LeaveCriticalSection(&cs); }
    };
#elif __linux__ && !__ANDROID__
    struct adaptive_mutex
    {
        pthread_mutex_t m = PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP;
        void lock() noexcept { pthread_mutex_lock(&m); }
        void unlock() noexcept { pthread_mutex_unlock(&m); }
    };
#endif

    inline void cpu_pause() noexcept
    {
    #if _MSC_VER && (_M_X64 || _M_IX86)
        _mm_pause();
    #elif _MSC_VER && _M_ARM64
        __yield();
    #elif __x86_64__ || __i386__
        __builtin_ia32_pause();
    #elif __aarch64__
        asm volatile("yield");
    #endif
    }

    // futex_mutex with a bounded spin before the first sleep
    template<int SPINS>
    struct spin_futex_mutex
    {
        std::atomic_uint32_t word { 0 };
        bool try_lock() noexcept
        {
            rpp::uint32 e = 0;
            return word.compare_exchange_strong(e, 1, std::memory_order_acquire, std::memory_order_relaxed);
        }
        void lock() noexcept
        {
            if (try_lock()) return;
            for (int i = 0; i < SPINS; ++i)
            {
                cpu_pause();
                if (word.load(std::memory_order_relaxed) == 0 && try_lock()) return;
            }
            while (word.exchange(2, std::memory_order_acquire) != 0)
                rpp::cvar::wait(&word, 2);
        }
        void unlock() noexcept
        {
            if (word.exchange(0, std::memory_order_release) == 2)
                rpp::cvar::wake_one(&word);
        }
    };

    // the Rust std futex mutex spin: load only, stop once a locker sleeps, and spin again after a wake
    template<int SPINS, bool RESPIN>
    struct polite_spin_mutex
    {
        std::atomic_uint32_t word { 0 };
        bool try_lock() noexcept
        {
            rpp::uint32 e = 0;
            return word.compare_exchange_strong(e, 1, std::memory_order_acquire, std::memory_order_relaxed);
        }
        rpp::uint32 spin() noexcept
        {
            rpp::uint32 s;
            for (int i = 0; (s = word.load(std::memory_order_relaxed)) == 1 && i < SPINS; ++i) cpu_pause();
            return s;
        }
        void lock() noexcept
        {
            if (try_lock()) return;
            rpp::uint32 s = spin();
            if (s == 0)
            {
                if (word.compare_exchange_strong(s, 1, std::memory_order_acquire, std::memory_order_relaxed)) return;
            }
            for (;;)
            {
                if (s != 2 && word.exchange(2, std::memory_order_acquire) == 0) return;
                rpp::cvar::wait(&word, 2);
                s = RESPIN ? spin() : word.load(std::memory_order_relaxed);
            }
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
    sample run_once(scenario sc, int duration_ms)
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

    struct contender
    {
        const char* name;
        sample (*run)(scenario, int);
        std::vector<sample> samples;
    };

    void run_scenario(std::vector<contender>& all, scenario sc)
    {
        for (contender& c : all) c.samples.clear();
        for (int rep = 0; rep < 7; ++rep)
            for (contender& c : all) c.samples.push_back(c.run(sc, 200));
        for (contender& c : all)
        {
            std::vector<sample>& s = c.samples;
            std::sort(s.begin(), s.end(), [](const sample& a, const sample& b) { return a.wall_ns < b.wall_ns; });
            const sample& med = s[s.size() / 2];
            std::printf("BENCH %-14s threads %2d cs %4d out %4d  wall %8.1f ns/op  cpu %8.1f ns/op  fair %.2f  (min %.1f max %.1f)\n",
                        c.name, sc.threads, sc.cs_work, sc.out_work, med.wall_ns, med.cpu_ns, med.fairness,
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
    #if _WIN32
        std::printf("BENCH release of an unheld SRWLOCK raises 0x%08lX\n", release_unheld_srw_lock());
    #endif
        std::vector<contender> all {
            { "futex_mutex", &run_once<rpp::futex_mutex>, {} },
            { "std::mutex", &run_once<std::mutex>, {} },
        #if _WIN32
            { "SRWLOCK", &run_once<srw_lock>, {} },
        #elif __linux__ && !__ANDROID__
            { "pthread_adapt", &run_once<adaptive_mutex>, {} },
        #endif
            { "spin40", &run_once<spin_futex_mutex<40>>, {} },
            { "polite100", &run_once<polite_spin_mutex<100, true>>, {} },
            { "polite1000", &run_once<polite_spin_mutex<1000, true>>, {} },
            { "polite100_1x", &run_once<polite_spin_mutex<100, false>>, {} },
        };
        run_scenario(all, { 1, 0, 0 });
        for (int threads : { 2, 4, 8, 16 }) run_scenario(all, { threads, 0, 0 });
        for (int threads : { 2, 4, 8, 16 }) run_scenario(all, { threads, 20, 100 });
        for (int threads : { 4, 16 }) run_scenario(all, { threads, 1000, 1000 });
    }
};
