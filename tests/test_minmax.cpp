#include <rpp/minmax.h>
#include <cfloat>  // FLT_MIN
#include <climits> // INT_MIN
#include <rpp/tests.h>

TestImpl(test_minmax)
{
    TestInit(test_minmax)
    {
    }

    TestCase(min)
    {
        AssertThat(rpp::min(5, 9), 5);
        AssertThat(rpp::min(INT_MIN, INT_MAX), INT_MIN);
        AssertThat(rpp::min(-10, -5), -10);

        AssertThat(rpp::min(5.0f, 9.0f), 5.0f);
        AssertThat(rpp::min(-FLT_MAX, FLT_MAX), -FLT_MAX);
        AssertThat(rpp::min(-10.0f, -5.0f), -10.0f);

        AssertThat(rpp::min(5.0, 9.0), 5.0);
        AssertThat(rpp::min(-DBL_MAX, DBL_MAX), -DBL_MAX);
        AssertThat(rpp::min(-10.0, -5.0), -10.0);
    }

    TestCase(max)
    {
        AssertThat(rpp::max(5, 9), 9);
        AssertThat(rpp::max(INT_MIN, INT_MAX), INT_MAX);
        AssertThat(rpp::max(-10, -5), -5);

        AssertThat(rpp::max(5.0f, 9.0f), 9.0f);
        AssertThat(rpp::max(-FLT_MAX, FLT_MAX), FLT_MAX);
        AssertThat(rpp::max(-10.0f, -5.0f), -5.0f);

        AssertThat(rpp::max(5.0, 9.0), 9.0);
        AssertThat(rpp::max(-DBL_MAX, DBL_MAX), DBL_MAX);
        AssertThat(rpp::max(-10.0, -5.0), -5.0);
    }

    TestCase(abs)
    {
        AssertThat(rpp::abs(+5), 5);
        AssertThat(rpp::abs(-5), 5);
        AssertThat(rpp::abs(INT_MIN), -INT_MIN);

        AssertThat(rpp::abs(+5.66f), 5.66f);
        AssertThat(rpp::abs(-5.66f), 5.66f);
        AssertThat(rpp::abs(-FLT_MAX), std::abs(FLT_MAX));

        AssertThat(rpp::abs(+5.66), 5.66);
        AssertThat(rpp::abs(-5.66), 5.66);
        AssertThat(rpp::abs(-DBL_MAX), std::abs(DBL_MAX));
    }

    TestCase(sqrt)
    {
        AssertThat(rpp::sqrt(16.0f),  4.0f);
        AssertThat(rpp::sqrt(100.0f), 10.0f);

        AssertThat(rpp::sqrt(16.0), 4.0);
        AssertThat(rpp::sqrt(100.0), 10.0);
    }

    TestCase(min3)
    {
        AssertThat(rpp::min3(-5, +5, +9), -5);
        AssertThat(rpp::min3(+5, -5, +9), -5);
        AssertThat(rpp::min3(+9, +5, -5), -5);
        AssertThat(rpp::min3(INT_MIN, 0, INT_MAX), INT_MIN);

        AssertThat(rpp::min3(-5.0f, +5.0f, +9.0f), -5.0f);
        AssertThat(rpp::min3(+5.0f, -5.0f, +9.0f), -5.0f);
        AssertThat(rpp::min3(+9.0f, +5.0f, -5.0f), -5.0f);
        AssertThat(rpp::min3(-FLT_MAX, 0.0f, FLT_MAX), -FLT_MAX);

        AssertThat(rpp::min3(-5.0, +5.0, +9.0), -5.0);
        AssertThat(rpp::min3(+5.0, -5.0, +9.0), -5.0);
        AssertThat(rpp::min3(+9.0, +5.0, -5.0), -5.0);
        AssertThat(rpp::min3(-DBL_MAX, 0.0, DBL_MAX), -DBL_MAX);
    }

    TestCase(max3)
    {
        AssertThat(rpp::max3(-5, +5, +9), +9);
        AssertThat(rpp::max3(+5, -5, +9), +9);
        AssertThat(rpp::max3(+9, +5, -5), +9);
        AssertThat(rpp::max3(INT_MIN, 0, INT_MAX), INT_MAX);

        AssertThat(rpp::max3(-5.0f, +5.0f, +9.0f), +9.0f);
        AssertThat(rpp::max3(+5.0f, -5.0f, +9.0f), +9.0f);
        AssertThat(rpp::max3(+9.0f, +5.0f, -5.0f), +9.0f);
        AssertThat(rpp::max3(-FLT_MAX, 0.0f, FLT_MAX), FLT_MAX);

        AssertThat(rpp::max3(-5.0, +5.0, +9.0), +9.0);
        AssertThat(rpp::max3(+5.0, -5.0, +9.0), +9.0);
        AssertThat(rpp::max3(+9.0, +5.0, -5.0), +9.0);
        AssertThat(rpp::max3(-DBL_MAX, 0.0, DBL_MAX), DBL_MAX);
    }

};
