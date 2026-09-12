/**
 * Imports rpp.std and no std header, so a name the module drops fails to compile here.
 * gcc-14 ships no libstdc++ std module, which is why ReCpp carries this one.
 */
#include <rpp/tests.h>

#if RPP_BUILD_WITH_MODULES

#include <rpp/tests.macros.h> // TestImpl, TestCase, AssertThat

import rpp.std; // includes come first, the import goes last

TestImpl(test_modules_std)
{
    TestInit(test_modules_std) {}

    TestCase(the_std_module_carries_what_a_public_signature_names)
    {
        std::string s = "abcd";                 // <string>
        AssertThat(int(s.size()), 4);

        std::vector<int> v { 1, 2, 3 };         // <vector>
        AssertThat(int(v.size()), 3);

        std::atomic_int n { 0 };                // <atomic>
        n += 7;
        AssertThat(n.load(), 7);

        std::optional<int> o = 5;               // <optional>
        AssertThat(o.value(), 5);

        std::unique_ptr<int> p = std::make_unique<int>(9); // <memory>
        AssertThat(*p, 9);

        // <type_traits>, which the public signatures name
        static_assert(std::is_same_v<std::decay_t<const int&>, int>);
        static_assert(std::is_same_v<std::conditional_t<true, int, char>, int>);
    }

    TestCase(the_std_module_carries_the_exception_types)
    {
        // std::exception_ptr is absent on purpose, see BUGS.md B19
        AssertThrows(throw std::runtime_error{"x"}, std::runtime_error);
        try { throw std::logic_error{"y"}; }
        catch (const std::exception& e) { AssertThat(std::string{e.what()}, std::string{"y"}); }
    }
};
#endif
