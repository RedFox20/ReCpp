// Throws across a module boundary, the shape BUGS.md B19 broke when a module exported
// std::exception_ptr. The std headers come first, which is the only shape gcc-14 never breaks.
#ifdef MAMA_HAS_MODULES
#include <stdexcept>

import rpp.core; // includes come first, the import goes last

template<class T> using has_size = decltype(sizeof(T));

int main()
{
    try { throw std::runtime_error{"x"}; }
    catch (const std::exception& e) { if (e.what()[0] != 'x') return 1; }

    try { throw std::logic_error{"y"}; }
    catch (const std::exception& e) { if (e.what()[0] != 'y') return 2; }

    // an rpp name from the import, so the module really loads. The alias is local, because
    // libc++ does not declare a std trait through <stdexcept> and libstdc++ does
    return rpp::is_detected_v<has_size, int> ? 0 : 3;
}
#else
int main() { return 0; } // the header build does not exercise the module
#endif
