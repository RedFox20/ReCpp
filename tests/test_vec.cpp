#include "tests.h"
#include <vec.h>
using namespace rpp;

TestImpl(test_vec)
{
    TestInit(test_vec)
    {
    }

    TestCase(test_initialization)
    {
        Vector2 point;

        point.print();
    }

} Impl;
