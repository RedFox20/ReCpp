// Drives the rpp.future surface from an import alone, which no other target reaches.
// It includes no <exception>, because that segfaults gcc-14 here. See BUGS.md B27.
#ifdef MAMA_HAS_MODULES
#include <typeinfo> // libstdc++ names typeid inside <future>, and the fragment does not carry it
#include <new>      // placement new, which a vector of cfuture runs
#include <vector>
#if _MSC_VER
// MSVC parses <thread> from the fragment here and needs the time_point operators. gcc-14
// crashes on this same include beside a future call, so the guard carries both. See BUGS.md B27
#include <chrono>
#endif
#if defined(__clang__) || defined(_MSC_VER)
// B27 is a gcc-14 defect, and this proves the other two carry no such limit. gcc-14 crashes
// on <memory> beside a future call, so only a compiler which is not gcc reads these lines
#define RPP_B27_FREE 1
#include <memory>
#endif

import rpp.future;

int main()
{
    // cfuture: a ready value, a pool task, and a continuation which reads the first result
    if (rpp::make_ready_future(11).get() != 11) return 1;
    if (rpp::async_task([] { return 7; }).get() != 7) return 2;
    if (rpp::async_task([] { return 7; }).then([](int v) { return v * 2; }).get() != 14) return 3;

    // an exception crosses the module boundary. A thrown int needs no <exception>
    bool threw = false;
    try { rpp::async_task([]() -> int { throw 42; }).get(); }
    catch (int v) { threw = (v == 42); }
    if (!threw) return 4;

    // a vector of futures also drives the container construction. get_all consumes them,
    // because ~cfuture terminates on a future no caller awaited
    std::vector<rpp::cfuture<int>> futures;
    futures.push_back(rpp::async_task([] { return 1; }));
    futures.push_back(rpp::async_task([] { return 2; }));
    rpp::wait_all(futures);
    const std::vector<int> values = rpp::get_all(futures);
    if (values.size() != 2 || values[0] + values[1] != 3) return 5;

#if RPP_B27_FREE
    // the B27 shape, which gcc-14 cannot compile. A shared_ptr beside a future call
    std::shared_ptr<int> owned = std::make_shared<int>(4);
    if (rpp::async_task([v = owned] { return *v; }).get() != 4) return 8;
#endif

    // event_loop ships in this group too, so name its public surface
    rpp::event_loop loop;
    if (loop.has_pending_work()) return 6;
    return loop.stop_and_wait_all_ready() ? 0 : 7;
}
#else
int main() { return 0; } // the header build does not exercise the module
#endif
