#if _MSC_VER
#pragma execution_character_set("utf-8")
#endif

// when building with modules, there shouldn't be any differences in the test code
// this validates our export modules are alright
#if RPP_BUILD_WITH_MODULES
import rpp.strview;
#endif

#include <rpp/tests.h>
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
        rpp::strview str = "hello";
        AssertThat(str.length(), 5);
        AssertThat(str, "hello");
        AssertNotEqual(str, "heihi");
        AssertThat(str[0], 'h');
        AssertThat(str[4], 'o');
        AssertNotEqual(str[3], 'x');

        using namespace std::string_literals;
        std::string stdstr  = "hello"s;
        std::string stdstr2 = "heihi"s;
        AssertThat(stdstr, str);
        AssertThat(str, stdstr);
        AssertNotEqual(stdstr2, str);
        AssertNotEqual(str, stdstr2);

        rpp::strview str2 = stdstr;
        AssertThat(str2, stdstr);
        AssertThat(str2.length(), (int)stdstr.length());

        std::string stdstr3 = str2;
        AssertThat(stdstr3, str2);
        AssertThat(str2, stdstr3);
    }

    TestCase(thread_local_buffer)
    {
        std::string buffer(1023, 'a'); // this should be the same length as rpp::strview to_cstr thread_local buffer
        std::vector<char> input(2048, 'a'); // this should be anything bigger and NOT null terminated
        rpp::strview view = { input.data(), input.size()-1 };
        const char* cview = view.to_cstr();

        AssertThat(strlen(cview), buffer.size());
        AssertThat(cview, buffer);
    }


    TestCase(next_token_single_element)
    {
        rpp::strview input = "first_token";
        rpp::strview a = input.next(',');
        AssertThat(a, "first_token");
    }
    TestCase(next_token_three_elements)
    {
        rpp::strview input = "first,second,third";
        rpp::strview a = input.next(',');
        rpp::strview b = input.next(',');
        rpp::strview c = input.next(',');
        AssertThat(a, "first");
        AssertThat(b, "second");
        AssertThat(c, "third");
    }
    TestCase(next_token_empty_input)
    {
        rpp::strview input = "";
        rpp::strview a = input.next(',');
        AssertThat(a, "");
    }
    TestCase(next_token_preserve_empty_tokens)
    {
        rpp::strview input = ",second";
        rpp::strview a = input.next(',');
        rpp::strview b = input.next(',');
        AssertThat(a, "");
        AssertThat(b, "second");
    }
    TestCase(next_token_empty_tokens_inbetween)
    {
        rpp::strview input = "first_token,,after_empty,";
        rpp::strview a = input.next(',');
        rpp::strview b = input.next(',');
        rpp::strview c = input.next(',');
        rpp::strview d = input.next(',');
        AssertThat(a, "first_token");
        AssertThat(b, "");
        AssertThat(c, "after_empty");
        AssertThat(d, "");
    }


    TestCase(parse_int_empty)
    {
        AssertThat(rpp::strview{""}.to_int(), 0);
    }
    TestCase(parse_int_normal)
    {
        AssertThat(rpp::strview{"1234"}.to_int(), 1234);
    }
    TestCase(parse_int_negative)
    {
        AssertThat(rpp::strview{"-12345"}.to_int(), -12345);
    }
    TestCase(parse_int_invalid_prefix)
    {
        AssertThat(rpp::strview{"abcdefgh-12345"}.to_int(), 0);
    }
    TestCase(parse_int_ascii_suffix)
    {
        AssertThat(rpp::strview{"12345abcdefgh"}.to_int(), 12345);
    }

    TestCase(parse_int_hex_empty)
    {
        AssertThat(rpp::strview{""}.to_int_hex(), 0);
    }
    TestCase(parse_int_hex_normal)
    {
        AssertThat(rpp::strview{"0x1234"}.to_int_hex(), 0x1234);
        AssertThat(rpp::strview{"01234"}.to_int_hex(), 0x1234);
        AssertThat(rpp::strview{"0X1234"}.to_int_hex(), 0x1234);
    }

    TestCase(parse_float_empty)
    {
        AssertThat(rpp::strview{""}.to_float(), 0.0f);
    }
    TestCase(parse_float_integer)
    {
        AssertThat(rpp::strview{"12345"}.to_float(), 12345.0f);
    }
    TestCase(parse_float_normal)
    {
        AssertThat(rpp::strview{"120.55"}.to_float(), 120.55f);
        AssertThat(rpp::strview{"-120.55"}.to_float(), -120.55f);
    }
    TestCase(parse_float_invalid_prefix)
    {
        AssertThat(rpp::strview{"    -120.55"}.to_float(), 0.0f);
        AssertThat(rpp::strview{"asda-120.55"}.to_float(), 0.0f);
    }
    TestCase(parse_float_ascii_suffix)
    {
        AssertThat(rpp::strview{"120.55abcdefgh"}.to_float(), 120.55f);
        AssertThat(rpp::strview{"-120.55abcdefgh"}.to_float(), -120.55f);
    }


    TestCase(parse_bool_empty)
    {
        AssertThat(rpp::strview{""}.to_bool(), false);
    }
    TestCase(parse_bool_normal_case_insensitive)
    {
        AssertThat(rpp::strview{"True"}.to_bool(), true);
        AssertThat(rpp::strview{"yEs"}.to_bool(), true);
        AssertThat(rpp::strview{"oN"}.to_bool(), true);
        AssertThat(rpp::strview{"1"}.to_bool(), true);
    }
    TestCase(parse_bool_invalid_ascii)
    {
        AssertThat(rpp::strview{"supardupah"}.to_bool(), false);
        AssertThat(rpp::strview{"aok"}.to_bool(), false);
        AssertThat(rpp::strview{"0"}.to_bool(), false);
    }


    TestCase(decompose)
    {
        rpp::strview input = "hello,,strview,1556,true\n";
        rpp::strview a, b, c; // NOLINT
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
        std::unordered_map<rpp::strview, int> map;
        map["hello"] = 1;
        map["world"] = 2;
        map["strview"] = 3;
        AssertThat(map["hello"], 1);
        AssertThat(map["world"], 2);
        AssertThat(map["strview"], 3);
    }

    std::string toString(double f, int maxDecimals) { // NOLINT(readability-convert-member-functions-to-static)
        char buf[32];
        return std::string{buf, buf + rpp::_tostring(buf, f, maxDecimals)};
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

    #if YOCTO_LINUX || __ANDROID__
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
        AssertEqual(rpp::strview{"hello"}, "hello");
        AssertEqual("hello", rpp::strview{"hello"});
        AssertTrue(rpp::strview{"hello"} == rpp::strview{"hello"});
        AssertTrue(rpp::strview{"hello"} == std::string{"hello"});
        AssertTrue(std::string{"hello"} == rpp::strview{"hello"});
        AssertFalse(rpp::strview{"hello"} == ""); // NOLINT(readability-container-size-empty)
        AssertFalse("" == rpp::strview{"hello"}); // NOLINT(readability-container-size-empty)
    }

    TestCase(empty_string_equals_empty_string)
    {
        AssertEqual(rpp::strview{""}, "");
        AssertEqual("", rpp::strview{""});
        AssertEqual(rpp::strview{""}, rpp::strview{""});
        AssertEqual(rpp::strview{""}, std::string{""});
        AssertEqual(std::string{""}, rpp::strview{""});
    }

    TestCase(empty_string_must_not_equal_nonempty)
    {
        // regression test: this was a surprising regression in equality operator
        AssertNotEqual(rpp::strview{""}, "--help");
        AssertNotEqual("--help", rpp::strview{""});
        AssertFalse(rpp::strview{""} == "--help");
        AssertFalse("--help" == rpp::strview{""});
        AssertTrue(rpp::strview{""} != "--help");
        AssertTrue("--help" != rpp::strview{""});
    }

    TestCase(string_compare_less)
    {
        AssertLess(rpp::strview{"aaaa"}, "bbbbbbbb");
        AssertLess(rpp::strview{"aaaa"}, rpp::strview{"bbbbbbbb"});
        AssertLess(rpp::strview{"aaaa"}, std::string{"bbbbbbbb"});

        AssertLess("aaaa", rpp::strview{"bbbbbbbb"});
        AssertLess(rpp::strview{"aaaa"}, rpp::strview{"bbbbbbbb"});
        AssertLess(std::string{"aaaa"}, rpp::strview{"bbbbbbbb"});
    }

    TestCase(string_compare_greater)
    {
        AssertGreater(rpp::strview{"bbbb"}, "aaaaaaaa");
        AssertGreater(rpp::strview{"bbbb"}, rpp::strview{"aaaaaaaa"});
        AssertGreater(rpp::strview{"bbbb"}, std::string{"aaaaaaaa"});

        AssertGreater("bbbb", rpp::strview{"aaaaaaaa"});
        AssertGreater(rpp::strview{"bbbb"}, rpp::strview{"aaaaaaaa"});
        AssertGreater(std::string{"bbbb"}, rpp::strview{"aaaaaaaa"});
    }

    TestCase(can_detect_utf8_strings)
    {
        AssertEqual(rpp::strview{u8"𝕳𝖊𝖑𝖑𝖔"}.length(), 20);
        AssertFalse(rpp::is_likely_utf8("hello"));
        AssertTrue(rpp::is_likely_utf8(u8"hello: Привет"));
        AssertTrue(rpp::is_likely_utf8(u8"hello: 你好"));
        AssertTrue(rpp::is_likely_utf8(u8"hello: 𝕳𝖊𝖑𝖑𝖔"));
        AssertTrue(rpp::is_likely_utf8(u8"2-byte sequence (€)"));
        AssertTrue(rpp::is_likely_utf8(u8"3-byte sequence (ℵ)"));
        AssertTrue(rpp::is_likely_utf8(u8"4-byte sequence (𝄞)"));
        AssertTrue(rpp::is_likely_utf8(u8"valid utf8: 😀 𝄞 ℵ €"));
    }

#if RPP_ENABLE_UNICODE
    TestCase(can_convert_utf8_to_utf16)
    {
        rpp::ustring empty = rpp::to_ustring("");
        AssertEqual(empty, u"");
        AssertEqual(empty.length(), 0);

        rpp::ustring ustr = rpp::to_ustring(u8"𝕳𝖊𝖑𝖑𝖔");
        AssertEqual(ustr, u"𝕳𝖊𝖑𝖑𝖔");
        AssertEqual(ustr.size(), 10u);

        ustr = rpp::to_ustring(u8"hello: Привет");
        AssertEqual(ustr, u"hello: Привет");
        AssertEqual(ustr.length(), 13);

        ustr = rpp::to_ustring(u8"hello: 你好");
        AssertEqual(ustr, u"hello: 你好");
        AssertEqual(ustr.length(), 9);

        ustr = rpp::to_ustring(u8"hello: 𝕳𝖊𝖑𝖑𝖔");
        AssertEqual(ustr, u"hello: 𝕳𝖊𝖑𝖑𝖔");
        AssertEqual(ustr.length(), 17u);

        ustr = rpp::to_ustring(u8"äääääää");
        AssertEqual(ustr, u"äääääää");
        AssertEqual(ustr.length(), 7);

        char16_t ubuf[512];
        int ulen = rpp::to_ustring(ubuf, 512, u8"𝕳𝖊𝖑𝖑𝖔");
        AssertEqual(ulen, 10);
        AssertEqual(ubuf, u"𝕳𝖊𝖑𝖑𝖔");
    }

    TestCase(can_convert_utf16_to_utf8)
    {
        std::string empty = rpp::to_string(u"");
        AssertEqual(empty, "");
        AssertEqual(empty.length(), 0);

        rpp::string str = rpp::to_string(u"𝕳𝖊𝖑𝖑𝖔");
        AssertEqual(str, u8"𝕳𝖊𝖑𝖑𝖔");
        AssertEqual(str.size(), 20u);

        str = rpp::to_string(u"hello: Привет");
        AssertEqual(str, u8"hello: Привет");
        AssertEqual(str.length(), 19);

        str = rpp::to_string(u"hello: 你好");
        AssertEqual(str, u8"hello: 你好");
        AssertEqual(str.length(), 13);

        str = rpp::to_string(u"hello: 𝕳𝖊𝖑𝖑𝖔");
        AssertEqual(str, u8"hello: 𝕳𝖊𝖑𝖑𝖔");
        AssertEqual(str.length(), 27u);

        str = rpp::to_string(u"äääääää");
        AssertEqual(str, u8"äääääää");
        AssertEqual(str.length(), 14);

        char8_t buf[512];
        int len = rpp::to_string(buf, 512, u"𝕳𝖊𝖑𝖑𝖔");
        AssertEqual(len, 20);
        AssertEqual(buf, u8"𝕳𝖊𝖑𝖑𝖔");

        std::string path = rpp::to_string(u"/tmp/äöüß/hello.txt");
        AssertEqual(path, u8"/tmp/äöüß/hello.txt");
        AssertEqual(path.length(), 23);
    }
#endif // RPP_ENABLE_UNICODE

};
