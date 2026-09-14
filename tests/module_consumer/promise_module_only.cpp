// Instantiates std::promise beside an import, the shape which crashes gcc-14 at -O1 and
// above whenever the module fragment carries <future>, see BUGS.md B16.
#ifdef MAMA_HAS_MODULES
#include <future> // includes come first, the import goes last

import rpp.threading;

int main()
{
    std::promise<int> p;
    p.set_value(7);
    if (p.get_future().get() != 7) return 1;

    rpp::semaphore_once_flag flag; // a name from the module, so the import is load bearing
    flag.notify();
    return flag.try_wait() ? 0 : 2;
}
#else
int main() { return 0; } // the header build does not exercise the module
#endif
