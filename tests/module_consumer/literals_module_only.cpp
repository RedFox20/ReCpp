// Reaches a literal operator through `using namespace rpp;` with no rpp header, so the
// module has to reopen the inline namespace which carries it.
#ifdef MAMA_HAS_MODULES
import rpp.text;
import rpp.time;

using namespace rpp;

int main()
{
    // rpp::literals, from strview.h
    if ("xy"_sv.length() != 2) return 1;
    // rpp::duration_literals, from timepoint.h
    if (!(1500_ms > 1_s)) return 2;
    return 0;
}
#else
int main() { return 0; } // the header build does not exercise the module
#endif
