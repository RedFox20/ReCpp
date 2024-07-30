#include <rpp/sprint.h>
#include <rpp/file_io.h>
#include <map>
#include <cfloat> // DBL_MAX
#include "TempFILE.h"
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

struct string_buffer_member_operator
{
    string_buffer& operator<<(string_buffer& sb) const
    {
        return sb << "string_buffer_member_operator";
    }
};

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

        std::string bigs(4096, 'z');
        string_buffer buf2{ bigs };
        AssertEqual(buf2.view(), bigs);
    }

    TestCase(string_buf_move)
    {
        string_buffer bigbuf;
        for (int i = 0; i < 20; ++i)
            bigbuf.writeln("str 10 20.1 2132 test this big string");
        std::string content = bigbuf.str();
        AssertGreaterOrEqual(content.size(), string_buffer::SIZE);

        string_buffer move_init { std::move(bigbuf) };
        AssertThat(move_init.view(), content);

        string_buffer move_assign{ std::string(4096, 'z') };
        move_assign = std::move(move_init);
        AssertThat(move_assign.view(), content);
    }

    TestCase(string_buf_loop)
    {
        // ensure it won't crash with random input loop
        string_buffer buf;
        for (int i = 0; i < 100; ++i)
            buf.writeln("str", 10, 20.1, "2132"_sv, "abcd"s);

        std::string str;
        for (int i = 0; i < 100; ++i)
            str += "str 10 20.1 2132 abcd\n";
        AssertEqual(buf.view(), str);

        // ensure it won't crash with gradual growth:
        string_buffer buf2;
        for (int i = 0; i < 4096; ++i)
            buf2.write('z');

        std::string bigs(4096, 'z');
        AssertEqual(buf2.view(), bigs);
    }

    TestCase(println)
    {
        TempFILE printed;
        println(printed.out, "hello", 10, "println", 20);
        AssertEqual(printed.text(), "hello 10 println 20\n");
    }

    TestCase(println_vector_strings)
    {
        std::vector<std::string> names = { "Bob", "Marley", "Mick", "Jagger", "Bruce" };

        TempFILE printed;
        println(printed.out, names);
        AssertEqual(printed.text(),
            "[5] = { \"Bob\", \"Marley\", \"Mick\", \"Jagger\", \"Bruce\" }\n");
    }

    TestCase(println_vector_shared_ptrs)
    {
        std::vector<std::shared_ptr<double>> ptrs = { std::make_shared<double>(1.1), 
                                                      std::make_shared<double>(2.2), 
                                                      std::make_shared<double>(3.4) };
        
        TempFILE printed;
        println(printed.out, ptrs);
        AssertEqual(printed.text(), "{ *{1.1}, *{2.2}, *{3.4} }\n");
    }

    TestCase(println_map)
    {
        std::map<int, std::string> map = { {0,"Bob"}, {1,"Marley"}, {2,"Mick"}, {3,"Jagger"}, {4,"Bruce"} };

        TempFILE printed;
        println(printed.out, map);
        AssertEqual(printed.text(),
            "[5] = { 0: \"Bob\", 1: \"Marley\", 2: \"Mick\", 3: \"Jagger\", 4: \"Bruce\" }\n");
    }

    TestCase(format)
    {
        std::string s = rpp::format("%02d, %s, %.1f\n", 7, "format", 0.5);
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

        //println("float_format:", sb.view());
        AssertThat(sb.view(), "-0.1708;-2.0012;0.9959");
    }

    TestCase(big_doubles)
    {
        double x = +4.8418443193907041e+30;
        double y = -4.8418443193907041e+30;
        rpp::string_buffer sb;
        sb.separator = ";";
        sb.write(x, y);
        //println("big_doubles:", sb.view());
    #if OCLEA || __ANDROID__
        // ARMv8 rounds stuff differently
        AssertThat(sb.view(), "9223372036854775807;-9223372036854775807");
    #else
        AssertThat(sb.view(), "9223372036854775808;-9223372036854775808");
    #endif
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
        //println("float_edge_case:", sb.view());
    #if OCLEA || __ANDROID__
        // ARMv8 rounds stuff differently
        AssertThat(sb.view(), "9223372036854775807;-9223372036854775807;0.0;-0.0");
    #else
        AssertThat(sb.view(), "9223372036854775808;-9223372036854775808;0.0;-0.0");
    #endif
    }

    TestCase(write_hex)
    {
        auto referenceHex = [](strview in) {
            string_buffer sb;
            for (uint8_t ch : in) sb.writef("%02x", ch);
            return sb.str();
        };

        std::string input = "simple STRING with ! different CHARS_and 0123456789;";
        string_buffer sb;
        sb.write_hex(input);
        std::string ashex = sb.str();
        AssertThat(ashex, referenceHex(input));
    }
    
    TestCase(string_buffer_write_any)
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

        // sb.write(string_buffer_operator{});
        // AssertThat(sb.view(), "string_buffer_operator");
        // sb.clear();

        sb.write(string_buffer_member_operator{});
        AssertThat(sb.view(), "string_buffer_member_operator");
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

        sb << string_buffer_member_operator{};
        AssertThat(sb.view(), "string_buffer_member_operator");
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
        AssertThat(rpp::sprint(1,2,string_buffer_operator{}), "1 2 string_buffer_operator");
        AssertThat(rpp::sprint(string_buffer_member_operator{}), "string_buffer_member_operator");
        AssertThat(rpp::sprint(ostream_operator{}, 1), "ostream_operator 1");

        external_to_string ext;
        AssertThat(rpp::sprint(&ext), "*{external_to_string}");
        AssertThat(rpp::sprint((const external_to_string*)&ext), "*{external_to_string}");
    }

};
