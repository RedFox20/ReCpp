// Imports the modules that <rpp/tests.h> masks, with no header, so the build fails if any
// of them drops an export. tests/test_modules.cpp cannot prove these four.
#ifdef MAMA_HAS_MODULES
#include <string>
#include <vector>

import rpp.type_traits; // includes come first, the imports go last
import rpp.source_loc;
import rpp.future_types;
import rpp.math;
import rpp.sprint;

// is_detected takes the alias of whoever calls it, so this proves the idiom, not one alias
template<class T> using has_size = decltype(std::declval<T>().size());

int main()
{
    // type_traits: the concepts, and the detection idiom a consumer drives with its own alias
    bool detected = rpp::is_detected_v<has_size, std::string>
                 && rpp::is_stringlike<std::string>
                 && rpp::is_container<std::vector<int>>
                 && rpp::is_iterable<std::vector<int>>;

    // source_loc: the type carries the call site
    constexpr rpp::source_loc loc { "masked_module_only.cpp", "main", 22 };
    bool located = loc.line() == 22 && loc.function_name() != nullptr;

    // future_types: the concepts hold for a plain function
    auto plain = [] { return 1; };
    bool typed = rpp::IsFunction<decltype(plain)> && rpp::NotFuture<int>;

    // math: the constants and the functions both come from the module
    bool numeric = rpp::clamp(5, 0, 3) == 3 && rpp::lerp(0.5, 30.0, 60.0) == 45.0
                && rpp::nearlyZero(0.0001) && rpp::PI > 3.14;

    // sprint: the buffer, the free functions and the container formatting all come from the module
    rpp::string_buffer sb;
    sb << "v=" << std::vector<int>{ 1, 2 };
    bool printed = rpp::sprint(1, "and", 2.5) == "1 and 2.5" && sb.view() == "v={ 1, 2 }"
                && rpp::to_string('x') == "x" && rpp::format("%d", 7) == "7";

    return (detected && located && typed && numeric && printed) ? 0 : 1;
}
#else
int main() { return 0; } // the header build does not exercise the module
#endif
