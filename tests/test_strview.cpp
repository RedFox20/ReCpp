#include <rpp/tests.h>
#include <rpp/file_io.h>
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

    TestCase(string_buf)
    {
        string_buffer buf; buf.writeln("str", 10, 20.1, "2132"_sv, "abcd"s);
        AssertThat(buf.view(), "str 10 20.1 2132 abcd\n");

        string bigs(4096, 'z');
        string_buffer buf2 { bigs };
        AssertEqual(buf2.view(), bigs);
    }

    TestCase(string_buf_loop)
    {
        // ensure it won't crash with random input loop
        string_buffer buf;
        for (int i = 0; i < 100; ++i)
        {
            buf.writeln("str", 10, 20.1, "2132"_sv, "abcd"s);
        }

        string str;
        for (int i = 0; i < 100; ++i)
            str += "str 10 20.1 2132 abcd\n";
        AssertEqual(buf.view(), str);

        // ensure it won't crash with gradual growth:
        string_buffer buf2;
        for (int i = 0; i < 4096; ++i)
            buf2.write('z');

        string bigs(4096, 'z');
        AssertEqual(buf2.view(), bigs);
    }

    TestCase(println)
    {
        println("hello", 10, "println", 20);
    }

} Impl;