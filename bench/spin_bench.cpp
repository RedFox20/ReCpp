// Wall and CPU ns per lock of rpp::mutex against std::mutex, to choose the futex_mutex spin length
#include <rpp/mutex.h>
#include <rpp/timepoint.h>
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <thread>
#include <vector>

static double cpu_sec()
{
    timespec ts {};
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// `cs` and `out` are LCG steps inside the lock and between two locks
template<class M> void bench(const char* name, int threads, int cs, int out)
{
    std::vector<double> wall, cpu;
    for (int rep = 0; rep < 5; ++rep)
    {
        M m;
        unsigned shared = 1;
        const int iters = 1'000'000 / threads;
        std::vector<std::thread> ts;
        double c0 = cpu_sec();
        rpp::TimePoint t0 = rpp::TimePoint::monotonic_now();
        for (int t = 0; t < threads; ++t)
            ts.emplace_back([&, t] {
                unsigned x = t + 1;
                for (int i = 0; i < iters; ++i)
                {
                    {
                        std::lock_guard g { m };
                        unsigned y = shared;
                        for (int w = 0; w < cs; ++w) y = y * 1664525u + 1013904223u;
                        shared = y + 1;
                    }
                    for (int w = 0; w < out; ++w) x = x * 1664525u + 1013904223u;
                }
                if (x == 42) std::printf(" "); // the compiler cannot drop the work outside the lock
            });
        for (std::thread& t : ts) t.join();
        double ops = double(threads) * iters;
        wall.push_back((rpp::TimePoint::monotonic_now() - t0).nsec / ops);
        cpu.push_back((cpu_sec() - c0) * 1e9 / ops);
    }
    std::sort(wall.begin(), wall.end());
    std::sort(cpu.begin(), cpu.end());
    std::printf("%s mutex threads=%2d cs=%4d out=%4d: wall %7.1f  cpu %7.1f ns/lock (wall min %.1f max %.1f)\n",
                name, threads, cs, out, wall[2], cpu[2], wall.front(), wall.back());
    std::fflush(stdout);
}

int main()
{
    const int cases[][2] = { {0, 0}, {20, 100}, {0, 200}, {1000, 1000} };
    for (auto [cs, out] : cases)
        for (int threads : {1, 2, 4, 8})
        {
            bench<rpp::mutex>("rpp", threads, cs, out);
            bench<std::mutex>("std", threads, cs, out);
        }
}
