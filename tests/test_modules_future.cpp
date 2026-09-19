/**
 * Imports rpp.future, which test_modules.cpp cannot. That unit already imports every other
 * group, and one more exhausts the imported source locations of gcc-14. See BUGS.md B8 shape 3.
 */
#include <rpp/tests.h>

#if RPP_BUILD_WITH_MODULES

#include <rpp/tests.macros.h> // TestImpl, TestCase, AssertThat
#include <future>             // std::promise, which rpp::cpromise aliases
#include <type_traits>        // std::is_same_v, which pins an exported signature
#include <vector>

// Keep this import list as short as it is. rpp.future carries the whole threading stack in
// its fragment, so another group here exceeds the source locations gcc-14 can represent
import rpp.core; // includes come first, the imports go last
import rpp.time;
import rpp.future;

TestImpl(test_modules_future)
{
    TestInit(test_modules_future) {}

    // clang rejects a plain function which returns a cfuture, so the launcher takes the wrapper
    RPP_CORO_WRAPPER static rpp::cfuture<void> module_launch(int&) { return {}; }

    TestCase(future_module_carries_the_whole_surface)
    {
        // gcc-14 crashes when an importer instantiates std::promise, so name these unevaluated, see BUGS.md B16
        static_assert(std::is_same_v<rpp::cpromise<int>, std::promise<int>>);
        static_assert(std::is_same_v<decltype(rpp::async_task(+[]{ return 1; })), rpp::cfuture<int>>);
        static_assert(std::is_same_v<decltype(rpp::make_ready_future(1)), rpp::cfuture<int>>);
        static_assert(std::is_same_v<decltype(rpp::make_ready_future()), rpp::cfuture<void>>);
        static_assert(std::is_same_v<decltype(rpp::make_exceptional_future<int>(1)), rpp::cfuture<int>>);

        // a filled vector needs a real cfuture, and every way to build one runs a promise
        std::vector<rpp::cfuture<int>> no_ints;
        rpp::wait_all(no_ints);
        AssertThat(int(rpp::get_all(no_ints).size()), 0);

        std::vector<rpp::cfuture<void>> no_voids;
        rpp::get_all(no_voids);

        std::vector<int> no_items;
        rpp::run_tasks(no_items, &module_launch);
    }

    // event_task starts eagerly, so one with no co_await ends before the caller sees it
    static rpp::event_task module_event_task(int* out) { *out = 3; co_return; }

    TestCase(event_loop_module_carries_the_whole_surface)
    {
        rpp::event_loop loop;
        AssertThat(loop.main_thread_id() != 0, true); // get_thread_id needs rpp.threading
        AssertThat(loop.has_pending_work(), false);

        int posted = 0;
        loop.post([&] { ++posted; });
        loop.run_until_idle();
        AssertThat(posted, 1);

        int value = 0;
        rpp::event_task task = module_event_task(&value);
        AssertThat(value, 3);
        AssertThat(task.done(), true);
    }

    TestCase(coroutines_module_carries_the_whole_surface)
    {
        // a cfuture coroutine instantiates std::promise, so the probe drives await_ready() directly, see BUGS.md B16
        rpp::time_awaiter elapsed { rpp::TimePoint::monotonic_now() };
        AssertThat(elapsed.await_ready(), true);

        using namespace rpp::coro_operators; // the module exports the inline namespace too
        rpp::time_awaiter pending = operator co_await(rpp::seconds(1));
        AssertThat(pending.await_ready(), false);

        rpp::functor_awaiter<int> functor { rpp::delegate<int()>{ +[] { return 7; } } };
        AssertThat(functor.await_ready(), false);

        // the constrained overloads pick a different awaiter, so each one needs its own probe
        static_assert(std::is_same_v<decltype(operator co_await(rpp::delegate<int()>{})), rpp::functor_awaiter<int>>);
        static_assert(std::is_same_v<decltype(operator co_await(std::future<int>{})), rpp::std_future_awaiter<int>>);

        static_assert(sizeof(rpp::functor_awaiter<void>) > 0, "the module must export functor_awaiter<void>");
        static_assert(sizeof(rpp::functor_awaiter_fut<rpp::cfuture<int>>) > 0, "the module must export functor_awaiter_fut");
        static_assert(sizeof(rpp::std_future_awaiter<int>) > 0, "the module must export std_future_awaiter");
    }
};

#endif // RPP_BUILD_WITH_MODULES
