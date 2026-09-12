// Imports rpp.std and includes no std type, so a name the module drops fails to build here.
// gcc-14 ships no libstdc++ std module, which is why ReCpp carries this one.
#ifdef MAMA_HAS_MODULES
#include <new> // the placement operator new a std::vector needs, and no std type

import rpp.std; // includes come first, the import goes last

int main()
{
    // strings, and the free operators the module has to export beside them
    std::string s = "abcd";
    std::string_view sv = s;
    if (s != "abcd" || s + "e" != "abcde" || sv.size() != 4) return 1;

    // containers. std::span reaches rpp::socket::send and rpp::socket::poll
    std::vector<int> v { 1, 2, 3 };
    std::array<int, 3> a { 1, 2, 3 };
    std::span<const int> sp { a };
    std::unordered_map<int, int> m;
    m[1] = 2;
    std::optional<int> o = 5;
    if (v.size() != 3 || sp.size() != 3 || m.size() != 1 || o.value() != 5) return 2;

    // tuples, which rpp::function_traits names
    std::tuple<int, char> t { 1, 'c' };
    std::pair<int, int> p = std::make_pair(1, 2);
    static_assert(std::tuple_size<decltype(t)>::value == 2);
    if (p.first != 1) return 3;

    // memory and atomics. std::shared_ptr does not link here, see BUGS.md B22
    std::unique_ptr<int> up = std::make_unique<int>(9);
    std::atomic_int n { 0 };
    n += 7;
    if (*up != 9 || n.load() != 7) return 4;

    // traits the public signatures name
    static_assert(std::is_same_v<std::decay_t<const int&>, int>);
    static_assert(std::is_same_v<std::conditional_t<true, int, char>, int>);

    // exceptions. std::exception_ptr is absent on purpose, see BUGS.md B19
    try { throw std::runtime_error{"x"}; }
    catch (const std::exception& e) { if (std::string{e.what()} != "x") return 5; }
    return 0;
}
#else
int main() { return 0; } // the header build does not exercise the module
#endif
