#include "thread_pool.h"
#include <chrono>
#include <cassert>
#include <csignal>

namespace rpp
{
    ///////////////////////////////////////////////////////////////////////////////

    pool_task::~pool_task() noexcept
    {
        unique_lock<mutex> lock(mtx);
        killed = true;
        cv.notify_one();
        th.detach();
    }

    void pool_task::run_task(int start, int end, const action<int, int>& newTask) noexcept
    {
        unique_lock<mutex> lock(mtx);
        task        = newTask;
        loopStart   = start;
        loopEnd     = end;
        taskRunning = true;
        cv.notify_one();
    }

    void pool_task::wait(int timeoutMillis) noexcept
    {
        //printf("wait for (%d, %d)\n", loopStart, loopEnd);
        while (taskRunning) // duplicate check is intentional (to avoid lock)
        {
            unique_lock<mutex> lock(mtx);
            if (!taskRunning)
                return;
            if (timeoutMillis)
            {
                if (cv.wait_for(lock, chrono::milliseconds(timeoutMillis)) == cv_status::timeout)
                    return;
            }
            else
            {
                cv.wait(lock);
            }
        }
    }

    static void segfault(int) { throw exception("SIGSEGV"); }

    void pool_task::run() noexcept
    {
        signal(SIGSEGV, segfault); // set SIGSEGV handler, so we can catch it

        while (!killed)
        {
            if (wait_for_task())
            {
                try
                {
                    taskRunning = true;
                    task(loopStart, loopEnd);
                }
                catch (const exception& e)
                {
                    fprintf(stderr, "pool_task::run unhandled exception: %s\n", e.what());
                }
                catch (...) // prevent failure that would terminate the thread
                {
                    fprintf(stderr, "pool_task::run unhandled exception!\n");
                }
                if (killed)
                    return;
            }
            taskRunning = false;
            cv.notify_one();
        }
    }

    bool pool_task::wait_for_task() noexcept
    {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock);
        return (bool)task; // got task?
    }


    ///////////////////////////////////////////////////////////////////////////////

    thread_pool thread_pool::global;
    static int core_count;

    thread_pool::thread_pool()
    {
        if (!core_count)
            core_count = (int)thread::hardware_concurrency();
    }

    thread_pool::~thread_pool() noexcept
    {
        // defined destructor to prevent agressive inlining
    }

    int thread_pool::active_tasks() noexcept
    {
        int active = 0;
        for (auto& task : tasks) 
            if (task->running()) ++active;
        return active;
    }

    int thread_pool::idle_tasks() noexcept
    {
        int idle = 0;
        for (auto& task : tasks)
            if (!task->running()) ++idle;
        return idle;
    }

    int thread_pool::total_tasks() noexcept
    {
        return (int)tasks.size();
    }

    int thread_pool::clear_idle_tasks() noexcept
    {
        int cleared = 0;
        for (int i = 0; i < (int)tasks.size(); ++i)
        {
            if (!tasks[i]->running())
            {
                swap(tasks[i], tasks.back()); // erase_swap_last pattern
                tasks.pop_back();
                ++cleared;
            }
        }
        return cleared;
    }

    pool_task* thread_pool::new_task() noexcept
    {
        tasks.emplace_back(make_unique<pool_task>());
        return tasks.back().get();
    }

    pool_task* thread_pool::next_task(size_t& poolIndex) noexcept
    {
        for (; poolIndex < tasks.size(); ++poolIndex)
        {
            pool_task* task = tasks[poolIndex].get();
            if (!task->running())
                return task;
        }
        return new_task();
    }

    void thread_pool::parallel_for(int rangeStart, int rangeEnd, 
                                   const action<int, int>& rangeTask) noexcept
    {
        assert(loop_running == false && "Fatal error: nested parallel loops are forbidden!");
        assert(core_count != 0);

        const int range = rangeEnd - rangeStart;
        if (range <= 0)
            return;

        const int cores = range < core_count ? range : core_count;
        const int len = range / cores;

        pool_task** active = (pool_task**)_alloca(sizeof(pool_task*) * cores);
        {
            lock_guard<mutex> lock(poolmutex);
            loop_running = true;

            size_t poolIndex = 0;
            for (int i = 0; i < cores; ++i)
            {
                const int start = i * len;
                const int end   = i == cores - 1 ? rangeEnd : start + len;

                pool_task* task = next_task(poolIndex);
                active[i] = task;
                task->run_task(start, end, rangeTask);
            }
        }

        for (int i = 0; i < cores; ++i)
            active[i]->wait();

        loop_running = false;
    }
}
