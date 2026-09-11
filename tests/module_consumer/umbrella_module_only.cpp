// Imports the umbrella with no rpp header, so `import rpp;` alone has to carry every layer.
// A module missing from rpp.cppm fails to compile here, not just in the ReCpp build.
#ifdef MAMA_HAS_MODULES
#include <string> // std::string, which rpp::to_string returns

import rpp;

int main()
{
    // one name per layer, so a dropped export import names the layer which lost it
    bool ok = rpp::strview{"L0"}.length() == 2         // L0 rpp.strview
           && rpp::max(2, 5) == 5                      // L0 rpp.minmax
           && rpp::radf(0.0f) == 0.0f                  // L1 rpp.math
           && rpp::millis(1500) > rpp::seconds(1)      // L1 rpp.timepoint
           && rpp::delegate<int()>{ +[] { return 4; } }() == 4  // L1 rpp.delegate
           && rpp::to_string(42) == "42"               // L2 rpp.sprint
           && rpp::path_combine("a", "b") == "a/b"     // L3 rpp.paths
           && rpp::Compare::eq(1, 1);                  // L3 rpp.tests

    rpp::semaphore sem; // L5 rpp.semaphore
    sem.notify();
    ok = ok && sem.count() == 1;

    // gcc-14 crashes when an importer instantiates std::promise, so L7 and L8 stay
    // unevaluated here, see BUGS.md B16
    static_assert(sizeof(rpp::cfuture<int>) > 0, "L7 rpp.future");
    static_assert(sizeof(rpp::time_awaiter) > 0, "L8 rpp.coroutines");
    return ok ? 0 : 1;
}
#else
int main() { return 0; } // the header build does not exercise the module
#endif
