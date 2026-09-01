#include <rpp/scope_guard.h>
#include <rpp/tests.h>
#if RPP_BUILD_WITH_MODULES
import rpp.scope_guard; // includes come first, the import goes last
#endif
using namespace rpp;


TestImpl(test_scope_guard)
{
    TestInit(test_scope_guard)
    {
    }

    TestCase(simple_scope_exit)
    {
        int valueToDecrement = 1;
        {
            scope_guard([&]{
                --valueToDecrement;
            });
        }
        AssertThat(valueToDecrement, 0);
    }

    TestCase(nested_scopes)
    {
        int valueToDecrement = 2;
        {
            scope_guard([&]{ --valueToDecrement; });
            {
                scope_guard([&]{ --valueToDecrement; });
            }
        }
        AssertThat(valueToDecrement, 0);
    }

};