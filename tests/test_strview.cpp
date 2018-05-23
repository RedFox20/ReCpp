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
        AssertThat(str2.length(), (int)stdstr.length());

        string stdstr3 = str2;
        AssertThat(stdstr3, str2);
        AssertThat(str2, stdstr3);
    }

	TestCase(thread_local_buffer)
    {
		string buffer(1023, 'a'); // this should be the same length as strview to_cstr thread_local buffer
	    vector<char> input(2048, 'a'); // this should be anything bigger and NOT null terminated
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

};
