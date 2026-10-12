/**
 * Imports rpp.future, which test_modules.cpp cannot. That unit already imports every other
 * group, and one more exhausts the imported source locations of gcc-14. See BUGS.md B8 shape 3.
 */
#include <rpp/tests.h>

#if RPP_BUILD_WITH_MODULES

#include <rpp/tests.macros.h> // TestImpl, TestCase, AssertThat
#include <future>             // std::future, which std_future_awaiter awaits
#include <type_traits>        // std::is_same_v, which pins an exported signature

// Keep this import list as short as it is. rpp.future carries the whole threading stack in
// its fragment, so another group here exceeds the source locations gcc-14 can represent
import rpp.core; // includes come first, the imports go last
import rpp.time;
import rpp.future;

TestImpl(test_modules_future)
{
    TestInit(test_modules_future) {}

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
        // rpp.threading holds the coroutine return types, so the probe drives await_ready() directly
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
        static_assert(sizeof(rpp::std_future_awaiter<int>) > 0, "the module must export std_future_awaiter");
    }
};

#endif // RPP_BUILD_WITH_MODULES
