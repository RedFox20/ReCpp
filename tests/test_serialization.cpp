#include <rpp/tests.h>
#include <rpp/binary_serializer.h>
using namespace rpp;

struct Struct1 : serializable<Struct1>
{
    float a = 0.0f;

    Struct1() {}
    Struct1(float a) : a(a) {}

    void introspect()
    {
        bind(a);
    }
};

struct Struct2 : serializable<Struct2>
{
    float a = 0.0f;
    int b = 0;
    std::string c = "";

    Struct2(){}
    Struct2(float a, int b, std::string c) : a(a), b(b), c(c) {}

    void introspect()
    {
        bind(a, b, c);
    }
};

struct Struct3 : serializable<Struct3>
{
    int a = 0;
    std::string b = "";

    Struct3(){}
    Struct3(int a, std::string b) : a(a), b(b) {}

    void introspect()
    {
        bind_name("a", a);
        bind_name("b", b);
    }
};

TestImpl(test_serialization)
{
    TestInit(test_serialization)
    {

    }

    TestCase(binary_serialize_simple)
    {
        binary_buffer buf;
        buf << Struct1{ 34.0f };
        Struct1 s1; buf >> s1;
        AssertThat(s1.a, 34.0f);

        buf.clear();
        buf << Struct2{ 42.0f, 42, "42"s };
        Struct2 s2; buf >> s2;
        AssertThat(s2.a, 42.0f);
        AssertThat(s2.b, 42);
        AssertThat(s2.c, "42");
    }

    TestCase(binary_serialize_nested)
    {

    }

    TestCase(binary_deserialize_nested)
    {

    }

    TestCase(binary_deserialize_version_check)
    {

    }

    TestCase(string_serialize_simple)
    {
        string_buffer buf;
        Struct3{ 42, "42" }.serialize(buf);
        Struct3 s3; buf >> s3;
        AssertThat(s3.a, 42);
        AssertThat(s3.b, "42");
        AssertThat(buf.view(), "a;42;b;42;\n");
    }

};