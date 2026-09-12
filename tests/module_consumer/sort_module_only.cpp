// Imports rpp.numeric alone, with no header and no rpp.containers, so the build fails if
// rpp::sort stops reaching an importer of the group which carries sort.h.
#ifdef MAMA_HAS_MODULES
#include <vector>

import rpp.numeric; // includes come first, the import goes last

int main()
{
    std::vector<int> v { 4, 1, 3 };
    rpp::sort(v);
    bool ascending = v[0] == 1 && v[1] == 3 && v[2] == 4;

    rpp::sort(v, [](int a, int b) { return a > b; });
    bool descending = v[0] == 4 && v[2] == 1;

    // the pointer overload and the container overload both come from this module
    int raw[3] { 9, 7, 8 };
    rpp::insertion_sort(raw, 3, [](int a, int b) { return a < b; });
    bool sorted_raw = raw[0] == 7 && raw[2] == 9;

    return (ascending && descending && sorted_raw) ? 0 : 1;
}
#else
int main() { return 0; } // the header build does not exercise the module
#endif
