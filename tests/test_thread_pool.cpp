#include <rpp/tests.h>
#include <rpp/thread_pool.h>
#include <rpp/timer.h> // performance measurement
#include <atomic>
#include <unordered_set>
using namespace rpp;

TestImpl(test_threadpool)
{
    TestInit(test_threadpool)
    {
    }

    int parallelism_count(int numIterations)
    {
        unordered_set<thread::id> ids;
        parallel_for(0, numIterations, [&](int start, int end)
        {
            ids.insert(this_thread::get_id());
        });
        return (int)ids.size();
    }

    TestCase(threadpool_concurrency)
    {
        printf("physical_cores: %d\n", thread_pool::physical_cores());
        AssertThat(parallelism_count(1), 1);
        AssertThat(parallelism_count(128), thread_pool::physical_cores());
    }

    TestCase(test_performance)
    {
        auto numbers = vector<int>(133333337);
        int* ptr = numbers.data();
        int  len = (int)numbers.size();

        parallel_for(0, len, [&](int start, int end) {
            for (int i = start; i < end; ++i)
                ptr[i] = i;
        });
        //#pragma loop(hint_parallel(0))
        //for (int i = 0; i < len; ++i)
        //    ptr[i] = rand() % 32768;
        //concurrency::parallel_for(0, len, [&](int index) {
        //    ptr[index] = index;
        //});

        Timer timer;

        atomic<int64_t> sum { 0 };;
        parallel_for(0, (int)numbers.size(), [&](int start, int end) {
            int64_t isum = 0;
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
        printf("ParallelFor  elapsed: %.3fs  result: %lld\n", timer.elapsed(), (long long)sum);

        timer.start();
        int64_t sum2 = 0;
        for (int i = 0; i < len; ++i)
            sum2 += ptr[i];
        printf("Singlethread elapsed: %.3fs  result: %lld\n", timer.elapsed(), (long long)sum2);
    }

} Impl;