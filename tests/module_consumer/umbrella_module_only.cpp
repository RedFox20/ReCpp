// Imports the umbrella with no rpp header, so `import rpp;` alone has to carry every group
// but rpp.testing. A module missing from rpp.cppm fails here, not just in the ReCpp build.
#ifdef MAMA_HAS_MODULES
#include <string> // std::string, which rpp::to_string returns

import rpp;

int main()
{
    // one name per group, so a dropped export import names the group which lost it
    bool ok = rpp::strview{"gp"}.length() == 2         // rpp.text
           && rpp::max(2, 5) == 5                      // rpp.numeric
           && rpp::radf(0.0f) == 0.0f                  // rpp.numeric
           && rpp::millis(1500) > rpp::seconds(1)      // rpp.time
           && rpp::delegate<int()>{ +[] { return 4; } }() == 4  // rpp.core
           && rpp::to_string(42) == "42"               // rpp.text
           && rpp::path_combine("a", "b") == "a/b";    // rpp.io

    rpp::semaphore sem; // rpp.threading
    sem.notify();
    ok = ok && sem.count() == 1;

    // gcc-14 crashes when an importer instantiates std::promise, so these two stay
    // unevaluated here, see BUGS.md B16
    static_assert(sizeof(rpp::cfuture<int>) > 0, "rpp.threading");
    static_assert(sizeof(rpp::time_awaiter) > 0, "rpp.threading coroutines");
    return ok ? 0 : 1;
}
#else
int main() { return 0; } // the header build does not exercise the module
#endif
