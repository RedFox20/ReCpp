#include "tests.h"
#include <file_io.h>

using namespace rpp;

TestImpl(test_strview)
{
    TestInit(test_strview)
    {
    }

    TestCase(basic_init)
    {
        strview str = "hello";
        Assert(str.length() == 5);
        Assert(str == "hello");
        Assert(str != "heihi");
        Assert(str[0] == 'h');
        Assert(str[4] == 'o');
        Assert(str[3] != 'x');

        string stdstr  = "hello"s;
        string stdstr2 = "heihi"s;
        Assert(stdstr == str);
        Assert(str == stdstr);
        Assert(stdstr2 != str);
        Assert(str != stdstr2);

        strview str2 = stdstr;
        Assert(str2 == stdstr);
        Assert(str2.length() == stdstr.length());

        string stdstr3 = str2;
        Assert(stdstr3 == str2);
        Assert(str2 == stdstr3);
    }

} Impl;