/**
 * Reaches every layer through `import rpp;` alone, with no other rpp import in this
 * translation unit, so a name which the umbrella misses fails to compile here.
 */
#include <rpp/tests.h>

#if RPP_BUILD_WITH_MODULES

#include <rpp/tests.macros.h> // TestImpl, TestCase, AssertThat
#include <string>
#include <vector>
#include <type_traits> // std::is_same_v, which pins an exported signature

import rpp; // includes come first, the import goes last

TestImpl(test_modules_umbrella)
{
    TestInit(test_modules_umbrella) {}

    TestCase(the_umbrella_reaches_every_layer)
    {
        // one name per layer, so a dropped export import names the layer which lost it
        AssertThat(rpp::strview{"L0"}.length(), 2);                  // L0 rpp.strview
        AssertThat(rpp::max(3, 7), 7);                               // L0 rpp.minmax
        AssertThat(rpp::radf(0.0f), 0.0f);                           // L1 rpp.math
        AssertThat(rpp::millis(1500) > rpp::seconds(1), true);       // L1 rpp.timepoint
        AssertThat(rpp::delegate<int()>{ +[] { return 4; } }(), 4);  // L1 rpp.delegate
        AssertThat(rpp::to_string(42), "42");                        // L2 rpp.sprint
        AssertThat(rpp::path_combine("a", "b"), "a/b");              // L3 rpp.paths
        AssertThat(rpp::file{"no_such_file_here"}.good(), false);    // L4 rpp.file_io
    }

    TestCase(the_umbrella_reaches_the_threading_layers)
    {
        rpp::semaphore sem; // L5 rpp.semaphore
        sem.notify();
        AssertThat(sem.count(), 1);

        rpp::concurrent_queue<int> queue; // L5 rpp.concurrent_queue
        queue.push(7);
        AssertThat(queue.size(), 1);

        // gcc-14 crashes when an importer instantiates std::promise, so name these
        // unevaluated, see BUGS.md B16
        static_assert(std::is_same_v<decltype(rpp::make_ready_future(1)), rpp::cfuture<int>>); // L7
        static_assert(sizeof(rpp::time_awaiter) > 0, "L8 rpp.coroutines");

        // L6 rpp.thread_pool, which runs the task on a pool thread and waits for it
        std::atomic_int ran { 0 };
        rpp::parallel_for(0, 4, 0, [&](int start, int end) { ran += (end - start); });
        AssertThat(ran.load(), 4);
    }

    TestCase(the_umbrella_reaches_the_test_framework_itself)
    {
        // rpp.tests is a module like any other, so `import rpp;` carries its declarations.
        // The macros above still come from the header, because a macro never crosses a module.
        AssertThat(rpp::Compare::eq(1, 1), true);
        AssertThat(rpp::Compare::lt(1, 2), true);
        static_assert(std::is_same_v<decltype(rpp::TestVerbosity::AllMessages), rpp::TestVerbosity>);
    }
};
#endif
