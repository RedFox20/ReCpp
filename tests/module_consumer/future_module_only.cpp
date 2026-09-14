// Drives the cfuture half of rpp.future from an import alone. The include list keeps clear of
// bits/exception_ptr.h, which gcc-14 cannot merge beside a cfuture. See BUGS.md B28.
#ifdef MAMA_HAS_MODULES
#include <typeinfo> // libstdc++ names typeid inside <future>, and the fragment does not carry it
#include <new>      // placement new, which a vector of cfuture runs
#include <vector>
#if _MSC_VER
// MSVC parses <thread> from the fragment here and needs the time_point operators. <chrono>
// reaches exception_ptr.h, which gcc-14 rejects beside a cfuture. See BUGS.md B28
#include <chrono>
#endif
#if defined(__clang__) || defined(_MSC_VER)
// B28 is a gcc-14 defect, and this proves the other two carry no such limit. Only a compiler
// which is not gcc reads these lines, because <memory> reaches exception_ptr.h
#define RPP_B28_FREE 1
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

#if RPP_B28_FREE
    // the B28 shape, which gcc-14 cannot compile. A shared_ptr beside a future call
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
