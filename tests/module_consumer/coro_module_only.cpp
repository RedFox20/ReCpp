// Drives the event_loop and awaiter half of rpp.future beside <memory>, which B27 forbids
// next to a cfuture. This target holds no cfuture, so no get() reaches the merge.
#ifdef MAMA_HAS_MODULES
#include <typeinfo> // libstdc++ names typeid inside <future>, and the fragment does not carry it
#include <new>      // placement new, which the loop queue runs
#include <memory>   // the header B27 rejects beside a cfuture, see BUGS.md B27
#include <vector>

import rpp.core;
import rpp.time;
import rpp.future;

int main()
{
    // <memory> is live here, so the target fails if B27 ever widens past cfuture
    std::shared_ptr<int> owned = std::make_shared<int>(4);
    if (*owned != 4) return 1;

    // event_loop: post a task and drain it on the calling thread
    rpp::event_loop loop;
    if (loop.has_pending_work()) return 2;
    int posted = 0;
    loop.post([&] { ++posted; });
    loop.run_until_idle();
    if (posted != 1) return 3;

    // the awaiters, named through await_ready so no coroutine frame builds a cfuture
    rpp::time_awaiter elapsed { rpp::TimePoint::monotonic_now() };
    if (!elapsed.await_ready()) return 4;

    using namespace rpp::coro_operators;
    rpp::time_awaiter pending = operator co_await(rpp::seconds(1));
    if (pending.await_ready()) return 5;

    rpp::functor_awaiter<int> functor { rpp::delegate<int()>{ +[] { return 7; } } };
    if (functor.await_ready()) return 6;

    return loop.stop_and_wait_all_ready() ? 0 : 7;
}
#else
int main() { return 0; } // the header build does not exercise the module
#endif
