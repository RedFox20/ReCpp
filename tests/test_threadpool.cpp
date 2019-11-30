#include <rpp/tests.h>
#include <rpp/thread_pool.h>
#include <rpp/timer.h> // performance measurement
#include <atomic>
#include <unordered_set>
using namespace rpp;
using std::mutex;
using std::atomic;
using std::atomic_int;
using std::unordered_set;
using namespace std::this_thread;
using namespace std::chrono_literals;


TestImpl(test_threadpool)
{
    TestInit(test_threadpool)
    {
        printf("physical_cores: %d\n", thread_pool::physical_cores());
    }

    static int parallelism_count(int numIterations)
    {
        mutex m;
        unordered_set<std::thread::id> ids;
        parallel_for(0, numIterations, [&](int start, int end)
        {
            ::sleep_for(1ms);
            { std::lock_guard<mutex> lock{m};
                ids.insert(::get_id());
            }
        });
        std::lock_guard<mutex> lock{m};
        return (int)ids.size();
    }

    TestCase(threadpool_concurrency)
    {
        AssertThat(parallelism_count(1), 1);
        AssertThat(parallelism_count(128), thread_pool::physical_cores());
    }

    TestCase(generic_task)
    {
        semaphore sync;
        string s = "Data";
        rpp::task_delegate<void()> fun = [x=s, &s, &sync]()
        {
            //printf("generic_task: %s\n", x.c_str());
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
        auto numbers = vector<int>(13333337);
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

    TestCase(parallel_foreach)
    {
        auto numbers = vector<int>(1337);
        parallel_foreach(numbers, [](int& n) {
            n = 1;
        });

        int checksum = 0;
        for (int i : numbers) checksum += i;
        AssertThat(checksum, 1337);
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

    TestCaseExpectedEx(parallel_task_exception, std::logic_error)
    {
        int times_launched = 0; // this makes sure the threadpool loop doesn't retrigger our task
        auto* task = rpp::parallel_task([&]() {
            AssertThat(times_launched, 0);
            ++times_launched;
            throw std::logic_error("aaargh!");
        });
        task->wait(); // @note this should rethrow
    }

    TestCase(parallel_task_reentrance)
    {
        int times_launched = 0;
        auto* task = rpp::parallel_task([&] {
            ++times_launched;
            ::sleep_for(10ms);
        });
        task->wait();
        AssertThat(times_launched, 1);

        task = rpp::parallel_task([&] {
            ++times_launched;
            ::sleep_for(10ms);
        });
        task->wait();
        AssertThat(times_launched, 2);
    }

    TestCase(parallel_task_resurrection)
    {
        thread_pool::global().max_task_idle_time(0.3f);
        thread_pool::global().clear_idle_tasks();
        AssertThat(thread_pool::global().active_tasks(), 0);

        atomic_int times_launched {0};
        rpp::parallel_task([&] { times_launched += 1; })->wait(1000);
        AssertThat((int)times_launched, 1);

        printf("Waiting for pool tasks to die naturally...\n");
        ::sleep_for(0.5s);
        printf("Attempting pool task resurrection\n");
        rpp::parallel_task([&] { 
            times_launched += 1; 
        })->wait(1000);
        AssertThat((int)times_launched, 2);

        thread_pool::global().max_task_idle_time(2);
    }

    TestCase(parallel_task_nested_nodeadlocks)
    {
        atomic_int times_launched {0};
        auto func = [&]() {
            times_launched += 1;
            pool_task* subtasks[5] = {
                parallel_task([&]() { times_launched += 1; }),
                parallel_task([&]() { times_launched += 1; }),
                parallel_task([&]() { times_launched += 1; }),
                parallel_task([&]() { times_launched += 1; }),
                parallel_task([&]() { times_launched += 1; }),
            };

            for (auto* task : subtasks)
                AssertThat(task->wait(1000), pool_task::finished);
        };
        pool_task* tasks[4] = {
            parallel_task(func),
            parallel_task(func),
            parallel_task(func),
            parallel_task(func),
        };

        for (auto* task : tasks)
            AssertThat(task->wait(1000), pool_task::finished);

        int expected = 4 * 6;
        AssertThat((int)times_launched, expected);
    }

};