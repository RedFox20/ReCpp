#include <rpp/stack_trace.h>
#include <rpp/thread_pool.h>
#include <rpp/timer.h>
#include <rpp/debugging.h>
#include <future>
#include <rpp/tests.h>
using namespace rpp;

static std::string stack_tracer() { return stack_trace(); }

TestImpl(test_stack_trace)
{
    TestInit(test_stack_trace)
    {
        thread_pool::global().set_task_tracer(&stack_tracer);
    }
    TestCleanup()
    {
        thread_pool::global().set_task_tracer(nullptr);
    }
    TestCase(second_trace_faster_than_first)
    {
        Timer t;
        stack_trace();
        double first_elapsed = t.elapsed();
        print_info("first stack_trace elapsed: %.2fs\n", first_elapsed);

        Timer t2;
        stack_trace();
        double second_elapsed = t2.elapsed();
        print_info("second stack_trace elapsed: %.2fs\n", second_elapsed);

        AssertLessOrEqual(second_elapsed, first_elapsed);
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
        auto task = rpp::parallel_task([] {
            throw traced_exception{ "TracedException from parallel_task" };
        });
        task.wait(); // @note this will throw the exception from parallel task
    }
};
