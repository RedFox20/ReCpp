/**
 * Chainable and coroutine compatible futures, which own their shared state
 * Copyright (c) 2026, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "async.h"
#include "thread_pool.h" // rpp::parallel_task_detached
#include <utility> // std::exchange

namespace rpp::detail
{
    // each result a step publishes nests the next step one level deeper, so a longer chain continues after the stack unwinds
    static constexpr int MAX_NESTED_STEPS = 32;

    // the steps this thread runs inside each other. A thread outside the pool runs none
    static thread_local int nested_steps = 0;

    bool in_pool_step() noexcept
    {
        return nested_steps > 0;
    }

    static void run_step(continuation* c) noexcept
    {
        ++nested_steps;
        c->run();
        delete c;
        --nested_steps;
    }

    // a step which the depth cap postponed, so the pool task runs it after the stack unwinds
    static thread_local continuation* deferred = nullptr;

    void continuation::run_pool_task() noexcept
    {
        for (continuation* c = this; c; c = std::exchange(deferred, nullptr))
            run_step(c);
    }

    void start_step(continuation* c, bool may_run_here) noexcept
    {
        if (may_run_here && nested_steps < MAX_NESTED_STEPS)
            run_step(c);
        else if (may_run_here && !deferred)
            deferred = c;
        else
            rpp::parallel_task_detached(rpp::delegate<void()>{c, &continuation::run_pool_task});
    }

    void run_outside_pool_steps(continuation& c) noexcept
    {
        int depth = std::exchange(nested_steps, 0);
        c.run();
        nested_steps = depth;
    }
}
