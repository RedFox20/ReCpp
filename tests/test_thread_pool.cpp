#include "tests.h"
#include <thread_pool.h>
#include <timer.h> // performance measurement
#include <atomic>
using namespace rpp;

TestImpl(test_threadpool)
{
    TestInit(test_threadpool)
    {
    }

    TestCase(threadpool_concurrency)
    {
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
        atomic_int64_t sum = 0;
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
        printf("ParallelFor  elapsed: %.3fs  result: %lld\n", timer.elapsed(), (int64_t)sum);

        timer.start();
        int64_t sum2 = 0;
        for (int i = 0; i < len; ++i)
            sum2 += ptr[i];
        printf("Singlethread elapsed: %.3fs  result: %lld\n", timer.elapsed(), sum2);
    }

} Impl;