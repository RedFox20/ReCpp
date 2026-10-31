// Drives rpp::future through rpp.threading and rpp.future from an import alone. On gcc-14 the include
// list keeps clear of bits/exception_ptr.h, because no run measured rpp::future beside it. See BUGS.md B28.
#ifdef MAMA_HAS_MODULES
#include <typeinfo> // the include set of BUGS.md B28
#include <new>      // placement new, which a vector of futures runs
#include <vector>
#if _MSC_VER
// MSVC parses <thread> from the fragment here and needs the time_point operators. <chrono>
// reaches exception_ptr.h, which the gcc-14 include set leaves out. See BUGS.md B28
#include <chrono> // BUGS.md B28
#endif
#if defined(__clang__) || defined(_MSC_VER) || __GNUC__ >= 15
// B28 is a gcc-14 defect. gcc-15, clang and MSVC compile the lines below, which reach
// exception_ptr.h through <memory>
#define RPP_B28_FREE 1
#include <memory>
#endif

import rpp.threading;
import rpp.future;

int main()
{
    // future: a ready value, a pool task, and a continuation which reads the first result
    if (rpp::ready_future(11).get() != 11) return 1;
    if (rpp::async([] { return 7; }).get() != 7) return 2;
    if (rpp::async([] { return 7; }).then([](int v) { return v * 2; }).get() != 14) return 3;

    // an exception crosses the module boundary. A thrown int needs no <exception>
    bool threw = false;
    try { rpp::async([]() -> int { throw 42; }).get(); }
    catch (int v) { threw = (v == 42); }
    if (!threw) return 4;

    // a vector of futures also drives the container construction. get_all consumes them,
    // because ~future terminates on a future no caller awaited
    std::vector<rpp::future<int>> futures;
    futures.push_back(rpp::async([] { return 1; }));
    futures.push_back(rpp::async([] { return 2; }));
    rpp::wait_all(futures);
    const std::vector<int> values = rpp::get_all(futures);
    if (values.size() != 2 || values[0] + values[1] != 3) return 5;

#if RPP_B28_FREE
    // a shared_ptr beside a future call
    std::shared_ptr<int> owned = std::make_shared<int>(4);
    if (rpp::async([v = owned] { return *v; }).get() != 4) return 8;
#endif

    // the event loop pumps a future to its result on this thread
    rpp::event_loop loop;
    rpp::future<int> pending = rpp::async([] { return 5; });
    if (loop.run_until_ready(pending) != 5) return 6;
    return loop.stop_and_wait_all_ready() ? 0 : 7;
}
#else
int main() { return 0; } // the header build does not exercise the module
#endif
