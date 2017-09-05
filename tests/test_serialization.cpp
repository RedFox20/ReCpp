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
    string c = "";

    Struct2(){}
    Struct2(float a, int b, string c) : a(a), b(b), c(c) {}

    void introspect()
    {
        bind(a, b, c);
    }
};

TestImpl(test_serialization)
{
    TestInit(test_serialization)
    {

    }

    TestCase(serialize_simple)
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

    TestCase(serialize_nested)
    {

    }

    TestCase(deserialize_nested)
    {

    }

    TestCase(deserialize_version_check)
    {

    }

} Impl;