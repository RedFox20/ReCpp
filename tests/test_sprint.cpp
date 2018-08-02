#include <rpp/sprint.h>
#include <cfloat> // DBL_MAX
#include <rpp/tests.h>
using namespace rpp;

struct external_to_string { };
std::string to_string(external_to_string) { return "external_to_string"; }

struct member_to_string
{
    std::string to_string() const { return "member_to_string"; }
};

struct string_buffer_operator { };
string_buffer& operator<<(string_buffer& sb, const string_buffer_operator&)
{
    return sb << "string_buffer_operator";
}

struct ostream_operator { };
std::ostream& operator<<(std::ostream& os, const ostream_operator&)
{
    return os << "ostream_operator";
}

enum stringable_enum
{
    strenum_first,
    strenum_last,
};

const char* to_string(stringable_enum e)
{
    switch (e)
    {
        case strenum_first:   return "first";
        case strenum_last:    return "last";
        default: return "undefined";
    }
}

TestImpl(test_sprint)
{
    TestInit(test_sprint)
    {
    }

    TestCase(string_buf)
    {
        string_buffer buf; buf.writeln("str", 10, 20.1, "2132"_sv, "abcd"s);
        AssertThat(buf.view(), "str 10 20.1 2132 abcd\n");

        string bigs(4096, 'z');
        string_buffer buf2{ bigs };
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

    TestCase(format)
    {
        string s = rpp::format("%02d, %s, %.1f\n", 7, "format", 0.5);
        AssertThat(s, "07, format, 0.5\n");
    }

    TestCase(float_formatting)
    {
        double x = -0.17080;
        double y = -2.00120;
        double z = +0.99590;
        rpp::string_buffer sb;
        sb.separator = ";";
        sb.write(x, y, z);

        println("float_format:", sb.view());
        AssertThat(sb.view(), "-0.1708;-2.001199;0.995899");
    }

    TestCase(float_edge_cases)
    {
        double x = +DBL_MAX;
        double y = -DBL_MAX;
        double z = +DBL_MIN;
        double w = -DBL_MIN;
        rpp::string_buffer sb;
        sb.separator = ";";
        sb.write(x, y, z, w);
        println("float_edge_case:", sb.view());
        AssertThat(sb.view(), "9223372036854775808;-9223372036854775808;0.0;-0.0");
    }

    TestCase(write_hex)
    {
        auto referenceHex = [](strview in) {
            string_buffer sb;
            for (uint8_t ch : in) sb.writef("%02x", ch);
            return sb.str();
        };

        string input = "simple STRING with ! different CHARS_and 0123456789;";
        string_buffer sb;
        sb.write_hex(input);
        string ashex = sb.str();
        AssertThat(ashex, referenceHex(input));
    }

    
    TestCase(to_stringable)
    {
        string_buffer sb;

        sb.write(0.16);
        AssertThat(sb.view(), "0.16");
        sb.clear();

        stringable_enum strenum = strenum_last;
        sb.write(strenum);
        AssertThat(sb.view(), "last");
        sb.clear();

        sb.write(external_to_string{});
        AssertThat(sb.view(), "external_to_string");
        sb.clear();

        sb.write(member_to_string{});
        AssertThat(sb.view(), "member_to_string");
        sb.clear();

        sb.write(string_buffer_operator{});
        AssertThat(sb.view(), "string_buffer_operator");
        sb.clear();

        sb.write(ostream_operator{});
        AssertThat(sb.view(), "ostream_operator");
        sb.clear();

        external_to_string ext;
        sb.write(&ext);
        AssertThat(sb.view(), "*{external_to_string}");
        sb.clear();

        sb.write((const external_to_string*)&ext);
        AssertThat(sb.view(), "*{external_to_string}");
        sb.clear();
    }

    TestCase(string_buffer_shift_op)
    {
        string_buffer sb;

        sb << 0.16;
        AssertThat(sb.view(), "0.16");
        sb.clear();

        stringable_enum strenum = strenum_last;
        sb << strenum;
        AssertThat(sb.view(), "last");
        sb.clear();

        sb << external_to_string{};
        AssertThat(sb.view(), "external_to_string");
        sb.clear();

        sb << member_to_string{};
        AssertThat(sb.view(), "member_to_string");
        sb.clear();

        sb << string_buffer_operator{};
        AssertThat(sb.view(), "string_buffer_operator");
        sb.clear();

        sb << ostream_operator{};
        AssertThat(sb.view(), "ostream_operator");
        sb.clear();

        external_to_string ext;
        sb << &ext;
        AssertThat(sb.view(), "*{external_to_string}");
        sb.clear();

        sb << (const external_to_string*)&ext;
        AssertThat(sb.view(), "*{external_to_string}");
        sb.clear();
    }

    
    TestCase(sprint_to_stringable)
    {
        AssertThat(rpp::sprint(0.16), "0.16");
        AssertThat(rpp::sprint(strenum_last), "last");
        AssertThat(rpp::sprint(external_to_string{}), "external_to_string");
        AssertThat(rpp::sprint(member_to_string{}), "member_to_string");
        AssertThat(rpp::sprint(string_buffer_operator{}), "string_buffer_operator");
        AssertThat(rpp::sprint(ostream_operator{}), "ostream_operator");

        external_to_string ext;
        AssertThat(rpp::sprint(&ext), "*{external_to_string}");
        AssertThat(rpp::sprint((const external_to_string*)&ext), "*{external_to_string}");

    }


};
