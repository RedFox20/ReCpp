/**
 * Runs the module-only consumer checks through the normal test framework.
 * Each check lives in tests/modulecheck/ and includes no rpp header except a macro
 * header, so a missing export list entry fails the build. This file cannot do that
 * job itself: <rpp/tests.h> includes the very headers the modules wrap.
 */
#include <rpp/tests.h>

#if RPP_BUILD_WITH_MODULES

int check_debugging(); // tests/modulecheck/check_debugging.cpp

TestImpl(test_modules)
{
    TestInit(test_modules)
    {
    }

    TestCase(debugging_module_exports_everything_the_macros_need)
    {
        AssertThat(check_debugging(), 0);
    }
};

#endif // RPP_BUILD_WITH_MODULES
