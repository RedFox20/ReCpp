#include <rpp/scope_guard.h>
#include <rpp/tests.h>
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