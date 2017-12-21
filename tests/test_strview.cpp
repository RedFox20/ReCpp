#include <rpp/tests.h>
using namespace rpp;

TestImpl(test_strview)
{
    TestInit(test_strview)
    {
    }

    TestCase(basic_init)
    {
        strview str = "hello";
        AssertThat(str.length(), 5);
        AssertThat(str, "hello");
        AssertNotEqual(str, "heihi");
        AssertThat(str[0], 'h');
        AssertThat(str[4], 'o');
        AssertNotEqual(str[3], 'x');

        string stdstr  = "hello"s;
        string stdstr2 = "heihi"s;
        AssertThat(stdstr, str);
        AssertThat(str, stdstr);
        AssertNotEqual(stdstr2, str);
        AssertNotEqual(str, stdstr2);

        strview str2 = stdstr;
        AssertThat(str2, stdstr);
        AssertThat(str2.length(), stdstr.length());

        string stdstr3 = str2;
        AssertThat(stdstr3, str2);
        AssertThat(str2, stdstr3);
    }

} Impl;
