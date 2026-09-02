// Imports rpp.minmax with no header, so the build fails if the module drops an export.
// tests/test_minmax.cpp cannot prove this, because <rpp/tests.h> includes minmax.h.
#ifdef MAMA_HAS_MODULES
import rpp.minmax;

int main()
{
    bool ok = rpp::min(2, 5) == 2
           && rpp::max(2, 5) == 5
           && rpp::abs(-3.0f) == 3.0f
           && rpp::sqrt(9.0) == 3.0
           && rpp::min3(7, 2, 5) == 2
           && rpp::max3(7, 2, 5) == 7;
    return ok ? 0 : 1;
}
#else
int main() { return 0; } // the header build does not exercise the module
#endif
