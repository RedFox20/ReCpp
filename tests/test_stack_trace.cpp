#include <rpp/stack_trace.h>
#include <rpp/thread_pool.h>
#include <rpp/timer.h>
#include <rpp/debugging.h>
#include <future>
#include <rpp/tests.h>
using namespace rpp;

static string stack_tracer() { return stack_trace(); }

TestImpl(test_stack_trace)
{
    TestInit(test_stack_trace)
    {
        thread_pool::global().set_task_tracer(&stack_tracer);
    }
    TestCase(first_trace_performance)
    {
        Timer t;
        stack_trace();
        double e = t.elapsed();
        LogInfo("First stack_trace elapsed: %.2fs", e);
    }
    TestCase(second_trace_performance)
    {
        Timer t;
        stack_trace();
        double e = t.elapsed();
        LogInfo("second stack_trace elapsed: %.2fs", e);
    }
    TestCaseExpectedEx(trace_ex, traced_exception)
    {
        struct inner_struct {
            void method() {
                throw traced_exception{ "TracedException message" };
            }
        };
        inner_struct{}.method();
    }
    TestCaseExpectedEx(trace_ex_from_function, traced_exception)
    {
        std::function<void()> f = [] {
            throw traced_exception{ "TracedException from std::function" };
        };
        f();
    }
    TestCaseExpectedEx(trace_ex_from_future, traced_exception)
    {
        std::async(std::launch::async, [] {
            throw traced_exception{ "TracedException from std::async" };
        }).get();
    }
    TestCaseExpectedEx(trace_ex_from_parallel_task, traced_exception)
    {
        auto* task = rpp::parallel_task([] {
            throw traced_exception{ "TracedException from parallel_task" };
        });
        task->wait(); // @note this will throw the exception from parallel task
    }
} Impl;
