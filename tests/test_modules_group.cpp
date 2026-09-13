/**
 * Imports one group, so a group which drops a member fails to compile here.
 */
#include <rpp/tests.h>

#if RPP_BUILD_WITH_MODULES

#include <rpp/tests.macros.h> // TestImpl, TestCase, AssertThat
#include <vector>             // std::vector, which rpp::sort takes

import rpp.numeric; // includes come first, the import goes last

TestImpl(test_modules_group)
{
    TestInit(test_modules_group) {}

    TestCase(a_group_carries_every_member)
    {
        AssertThat(rpp::max(3, 7), 7);      // minmax.h
        AssertThat(rpp::radf(0.0f), 0.0f);  // math.h

        rpp::Vector2 v { 3.0f, 4.0f };      // vec.h
        AssertThat(v.length(), 5.0f);

        std::vector<int> items { 5, 1, 3 }; // sort.h
        rpp::sort(items);
        AssertThat(items[0], 1);
    }
};
#endif
