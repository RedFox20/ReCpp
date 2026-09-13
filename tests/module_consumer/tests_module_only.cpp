// Writes a test suite from the macro header and the module alone, with no <rpp/tests.h>.
// Each macro here needs a different include of tests.macros.h, so a dropped one fails the build.
#ifdef MAMA_HAS_MODULES
#include <rpp/tests.macros.h> // TestImpl, TestCase, AssertThat
#include <stdexcept>          // std::runtime_error, which this suite throws

import rpp.testing; // includes come first, the imports go last
import rpp.text;    // TestImpl expands to a constructor taking rpp::strview

// tests.macros.h re-exports future_types.h for <coroutine>, and the coroutine case below is
// what pins it. Drop that include and this file stops finding std::coroutine_traits.

TestImpl(module_only_suite)
{
    TestInitNoAutorun(module_only_suite) {} // std::unique_ptr, from <memory>

    TestCase(the_macros_reach_the_module_types)
    {
        AssertThat(rpp::Compare::eq(1, 1), true);
        AssertThat(rpp::Compare::lt(1, 2), true);
        AssertGreater(2, 1); // rpp::source_loc, from source_loc.h
    }

    TestCase(the_exception_macros_compile)
    {
        AssertThrows(throw std::runtime_error{"x"}, std::runtime_error);
        AssertNoThrowAny(int{0}); // std::exception, from <exception>
    }

    // typeid, from <typeinfo>
    TestCaseExpectedEx(an_expected_exception_names_its_type, std::runtime_error)
    {
        throw std::runtime_error{"expected"};
    }

    TestCaseCoro(a_coroutine_case_compiles)
    {
        AssertThat(1, 1);
        co_return;
    }
};

int main()
{
    // the suite registers itself and does not run, so a link of the framework is enough
    return 0;
}
#else
int main() { return 0; } // the header build does not exercise the module
#endif
