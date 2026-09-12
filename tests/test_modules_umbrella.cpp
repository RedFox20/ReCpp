/**
 * Reaches every layer through `import rpp;` alone, with no other rpp import in this
 * translation unit, so a name which the umbrella misses fails to compile here.
 */
#include <rpp/tests.h>

#if RPP_BUILD_WITH_MODULES

#include <rpp/tests.macros.h> // TestImpl, TestCase, AssertThat
#include <string>      // std::string, which rpp::to_string returns
#include <atomic>      // std::atomic_int, which the parallel_for case counts with
#include <type_traits> // std::is_same_v, which pins an exported signature

import rpp; // includes come first, the import goes last

TestImpl(test_modules_umbrella)
{
    TestInit(test_modules_umbrella) {}

    TestCase(the_umbrella_reaches_every_layer)
    {
        // one name per group, so a dropped export import names the group which lost it
        AssertThat(rpp::strview{"L0"}.length(), 2);                  // rpp.text
        AssertThat(rpp::max(3, 7), 7);                               // rpp.numeric
        AssertThat(rpp::radf(0.0f), 0.0f);                           // rpp.numeric
        AssertThat(rpp::millis(1500) > rpp::seconds(1), true);       // rpp.time
        AssertThat(rpp::delegate<int()>{ +[] { return 4; } }(), 4);  // rpp.core
        AssertThat(rpp::to_string(42), "42");                        // rpp.text
        AssertThat(rpp::path_combine("a", "b"), "a/b");              // rpp.io
        AssertThat(rpp::file{"no_such_file_here"}.good(), false);    // rpp.io
    }

    TestCase(the_umbrella_reaches_the_threading_layers)
    {
        rpp::semaphore sem; // rpp.threading
        sem.notify();
        AssertThat(sem.count(), 1);

        rpp::concurrent_queue<int> queue; // rpp.threading
        queue.push(7);
        AssertThat(queue.size(), 1);

        // gcc-14 crashes when an importer instantiates std::promise, so name these
        // unevaluated, see BUGS.md B16
        static_assert(std::is_same_v<decltype(rpp::make_ready_future(1)), rpp::cfuture<int>>); // rpp.threading
        static_assert(sizeof(rpp::time_awaiter) > 0, "rpp.threading coroutines");

        // rpp.threading, which runs the task on a pool thread and waits for it
        std::atomic_int ran { 0 };
        rpp::parallel_for(0, 4, 0, [&](int start, int end) { ran += (end - start); });
        AssertThat(ran.load(), 4);
    }
};
#endif
