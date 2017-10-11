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

    static int parallelism_count(int numIterations)
    {
        mutex m;
        unordered_set<thread::id> ids;
        parallel_for(0, numIterations, [&](int start, int end)
        {
            lock_guard<mutex> lock{m};
            ids.insert(this_thread::get_id());
        });
        lock_guard<mutex> lock{m};
        return (int)ids.size();
    }

    TestCase(threadpool_concurrency)
    {
        printf("physical_cores: %d\n", thread_pool::physical_cores());
        AssertThat(parallelism_count(1), 1);
        AssertThat(parallelism_count(128), thread_pool::physical_cores());
    }

    TestCase(generic_task)
    {
        semaphore sync;
        string s = "Data";
        delegate<void()> fun = [x=s, &s, &sync]()
        {
            printf("generic_task: %s\n", x.c_str());
            s = "completed";
            sync.notify();
        };
        parallel_task([f=move(fun)]()
        {
            f();
        });

        sync.wait();
        AssertThat(s, "completed");
    }

    TestCase(parallel_for_performance)
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

    TestCase(repeat_tests)
    {
        for (int i = 0; i < 2; ++i)
        {
            test_threadpool_concurrency();
            test_generic_task();
            test_parallel_for_performance();
        }
    }


    TestCase(parallel_task_unhandled_exception)
    {
        int times_launched = 0; // this makes sure the threadpool loop doesn't retrigger our task
        auto* task = rpp::parallel_task([&]() {
            AssertThat(times_launched, 0);
            ++times_launched;
            throw "aaargh!";
        });
        task->wait();
    }

    TestCase(parallel_task_reentrance)
    {
        int times_launched = 0;
        auto* task = rpp::parallel_task([&] {
            ++times_launched;
            this_thread::sleep_for(100ms);
        });
        task->wait();
        AssertThat(times_launched, 1);

        task = rpp::parallel_task([&] {
            ++times_launched;
            this_thread::sleep_for(100ms);
        });
        task->wait();
        AssertThat(times_launched, 2);
    }

} Impl;