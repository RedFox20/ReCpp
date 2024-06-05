#include <rpp/tests.h>
using namespace rpp;
#include <limits>
#include <unordered_map>

constexpr auto MAX_DOUBLE = std::numeric_limits<double>::max();
constexpr auto MIN_DOUBLE = std::numeric_limits<double>::min();

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

        std::string stdstr  = "hello"s;
        std::string stdstr2 = "heihi"s;
        AssertThat(stdstr, str);
        AssertThat(str, stdstr);
        AssertNotEqual(stdstr2, str);
        AssertNotEqual(str, stdstr2);

        strview str2 = stdstr;
        AssertThat(str2, stdstr);
        AssertThat(str2.length(), (int)stdstr.length());

        std::string stdstr3 = str2;
        AssertThat(stdstr3, str2);
        AssertThat(str2, stdstr3);
    }

    TestCase(thread_local_buffer)
    {
        std::string buffer(1023, 'a'); // this should be the same length as strview to_cstr thread_local buffer
        std::vector<char> input(2048, 'a'); // this should be anything bigger and NOT null terminated
        strview view = { input.data(), input.size()-1 };
        const char* cview = view.to_cstr();

        AssertThat(strlen(cview), buffer.size());
        AssertThat(cview, buffer);
    }


    TestCase(next_token_single_element)
    {
        strview input = "first_token";
        strview a = input.next(',');
        AssertThat(a, "first_token");
    }
    TestCase(next_token_three_elements)
    {
        strview input = "first,second,third";
        strview a = input.next(',');
        strview b = input.next(',');
        strview c = input.next(',');
        AssertThat(a, "first");
        AssertThat(b, "second");
        AssertThat(c, "third");
    }
    TestCase(next_token_empty_input)
    {
        strview input = "";
        strview a = input.next(',');
        AssertThat(a, "");
    }
    TestCase(next_token_preserve_empty_tokens)
    {
        strview input = ",second";
        strview a = input.next(',');
        strview b = input.next(',');
        AssertThat(a, "");
        AssertThat(b, "second");
    }
    TestCase(next_token_empty_tokens_inbetween)
    {
        strview input = "first_token,,after_empty,";
        strview a = input.next(',');
        strview b = input.next(',');
        strview c = input.next(',');
        strview d = input.next(',');
        AssertThat(a, "first_token");
        AssertThat(b, "");
        AssertThat(c, "after_empty");
        AssertThat(d, "");
    }


    TestCase(parse_int_empty)
    {
        AssertThat(strview{""}.to_int(), 0);
    }
    TestCase(parse_int_normal)
    {
        AssertThat(strview{"1234"}.to_int(), 1234);
    }
    TestCase(parse_int_negative)
    {
        AssertThat(strview{"-12345"}.to_int(), -12345);
    }
    TestCase(parse_int_invalid_prefix)
    {
        AssertThat(strview{"abcdefgh-12345"}.to_int(), 0);
    }
    TestCase(parse_int_ascii_suffix)
    {
        AssertThat(strview{"12345abcdefgh"}.to_int(), 12345);
    }


    TestCase(parse_float_empty)
    {
        AssertThat(strview{""}.to_float(), 0.0f);
    }
    TestCase(parse_float_integer)
    {
        AssertThat(strview{"12345"}.to_float(), 12345.0f);
    }
    TestCase(parse_float_normal)
    {
        AssertThat(strview{"120.55"}.to_float(), 120.55f);
        AssertThat(strview{"-120.55"}.to_float(), -120.55f);
    }
    TestCase(parse_float_invalid_prefix)
    {
        AssertThat(strview{"    -120.55"}.to_float(), 0.0f);
        AssertThat(strview{"asda-120.55"}.to_float(), 0.0f);
    }
    TestCase(parse_float_ascii_suffix)
    {
        AssertThat(strview{"120.55abcdefgh"}.to_float(), 120.55f);
        AssertThat(strview{"-120.55abcdefgh"}.to_float(), -120.55f);
    }


    TestCase(parse_bool_empty)
    {
        AssertThat(strview{""}.to_bool(), false);
    }
    TestCase(parse_bool_normal_case_insensitive)
    {
        AssertThat(strview{"True"}.to_bool(), true);
        AssertThat(strview{"yEs"}.to_bool(), true);
        AssertThat(strview{"oN"}.to_bool(), true);
        AssertThat(strview{"1"}.to_bool(), true);
    }
    TestCase(parse_bool_invalid_ascii)
    {
        AssertThat(strview{"supardupah"}.to_bool(), false);
        AssertThat(strview{"aok"}.to_bool(), false);
        AssertThat(strview{"0"}.to_bool(), false);
    }


    TestCase(decompose)
    {
        strview input = "hello,,strview,1556,true\n";
        strview a, b, c;
        int x = 0;
        bool y = false;
        input.decompose(',', a, b, c, x, y);
        AssertThat(a, "hello");
        AssertThat(b, "");
        AssertThat(c, "strview");
        AssertThat(x, 1556);
        AssertThat(y, true);
    }

    TestCase(hashmap_of_strview)
    {
        std::unordered_map<strview, int> map;
        map["hello"] = 1;
        map["world"] = 2;
        map["strview"] = 3;
        AssertThat(map["hello"], 1);
        AssertThat(map["world"], 2);
        AssertThat(map["strview"], 3);
    }

    std::string toString(double f, int maxDecimals) {
        char buf[32];
        return std::string{buf, buf + _tostring(buf, f, maxDecimals)};
    }

    TestCase(tostring_float)
    {
        AssertThat(toString(+0.199999, 6), "0.2");
        AssertThat(toString(-0.199999, 6), "-0.2");
        AssertThat(toString(+0.099999, 6), "0.1");
        AssertThat(toString(-0.099999, 6), "-0.1");
        AssertThat(toString(+100.1, 6), "100.1");
        AssertThat(toString(+0.05, 6), "0.05");
        AssertThat(toString(-0.05, 6), "-0.05");
        AssertThat(toString(-0.17080, 6), "-0.1708");
        AssertThat(toString(-2.00120, 6), "-2.0012");
        AssertThat(toString(+0.99590, 6), "0.9959");
        AssertThat(toString(+0.16, 6), "0.16");

    #if OCLEA || __ANDROID__
        // ARMv8 rounds stuff differently
        AssertThat(toString(+4.8418443193907041e+30, 6), "9223372036854775807");
        AssertThat(toString(-4.8418443193907041e+30, 6), "-9223372036854775807");
        AssertThat(toString(+MAX_DOUBLE, 6), "9223372036854775807");
        AssertThat(toString(-MAX_DOUBLE, 6), "-9223372036854775807");
    #else
        AssertThat(toString(+4.8418443193907041e+30, 6), "9223372036854775808");
        AssertThat(toString(-4.8418443193907041e+30, 6), "-9223372036854775808");
        AssertThat(toString(+MAX_DOUBLE, 6), "9223372036854775808");
        AssertThat(toString(-MAX_DOUBLE, 6), "-9223372036854775808");
    #endif
        AssertThat(toString(+MIN_DOUBLE, 6), "0.0");
        AssertThat(toString(-MIN_DOUBLE, 6), "-0.0");
    }

    TestCase(tostring_float_precision)
    {
        // NOTE: if decimals=0 the float is rounded
        AssertThat(toString(+100.999999, 0), "101");
        AssertThat(toString(-100.999999, 0), "-101");
        AssertThat(toString(+100.123456, 0), "100");
        AssertThat(toString(-100.123456, 0), "-100");

        AssertThat(toString(+100.123456, 1), "100.1");
        AssertThat(toString(-100.123456, 1), "-100.1");
        AssertThat(toString(+100.123456, 2), "100.12");
        AssertThat(toString(-100.123456, 2), "-100.12");
        AssertThat(toString(+100.123456, 3), "100.123");
        AssertThat(toString(-100.123456, 3), "-100.123");
    }

    TestCase(equals_with_identical_strings)
    {
        AssertEqual(strview{"hello"}, "hello");
        AssertEqual("hello", strview{"hello"});
        AssertTrue(strview{"hello"} == strview{"hello"});
        AssertTrue(strview{"hello"} == std::string{"hello"});
        AssertTrue(std::string{"hello"} == strview{"hello"});
        AssertFalse(strview{"hello"} == "");
        AssertFalse("" == strview{"hello"});
    }

    TestCase(empty_string_equals_empty_string)
    {
        AssertEqual(strview{""}, "");
        AssertEqual("", strview{""});
        AssertEqual(strview{""}, strview{""});
        AssertEqual(strview{""}, std::string{""});
        AssertEqual(std::string{""}, strview{""});
    }

    TestCase(empty_string_must_not_equal_nonempty)
    {
        // regression test: this was a surprising regression in equality operator
        AssertNotEqual(strview{""}, "--help");
        AssertNotEqual("--help", strview{""});
        AssertFalse(strview{""} == "--help");
        AssertFalse("--help" == strview{""});
        AssertTrue(strview{""} != "--help");
        AssertTrue("--help" != strview{""});
    }

    TestCase(string_compare_less)
    {
        AssertLess(strview{"aaaa"}, "bbbbbbbb");
        AssertLess(strview{"aaaa"}, strview{"bbbbbbbb"});
        AssertLess(strview{"aaaa"}, std::string{"bbbbbbbb"});

        AssertLess("aaaa", strview{"bbbbbbbb"});
        AssertLess(strview{"aaaa"}, strview{"bbbbbbbb"});
        AssertLess(std::string{"aaaa"}, strview{"bbbbbbbb"});
    }

    TestCase(string_compare_greater)
    {
        AssertGreater(strview{"bbbb"}, "aaaaaaaa");
        AssertGreater(strview{"bbbb"}, strview{"aaaaaaaa"});
        AssertGreater(strview{"bbbb"}, std::string{"aaaaaaaa"});

        AssertGreater("bbbb", strview{"aaaaaaaa"});
        AssertGreater(strview{"bbbb"}, strview{"aaaaaaaa"});
        AssertGreater(std::string{"bbbb"}, strview{"aaaaaaaa"});
    }
};
