#include <rpp/tests.h>
#include <rpp/binary_serializer.h>
using namespace rpp;

struct Struct1 : serializable<Struct1>
{
    float a = 42.0f;

    static void introspect()
    {
        bind(&Struct1::a);
    }
};

struct Struct2 : serializable<Struct2>
{
    float a = 44.0f;
    float b = 46.0f;

    static void introspect()
    {
        bind(&Struct2::a);
        bind(&Struct2::b);
    }
};

TestImpl(test_serialization)
{
    TestInit(test_serialization)
    {
        Struct1 s1;
        Struct2 s2;

        s1.introspect();
        s2.introspect();

        socket_writer w1;
        s1.serialize(w1);
        
    }

    TestCase(object_size)
    {
        AssertThat(sizeof(char), size_of<char>(123));
        AssertThat(sizeof(int), size_of<int>(1234));
		
        vector<int> simpleVec = { 1, 2, 3, 4 };
        int simpleVecSize = size_of(simpleVec);
        int simpleVecExpected = sizeof(int) + sizeof(int) * (int)simpleVec.size();
        AssertThat(simpleVecSize, simpleVecExpected);

        string str = "test";
        AssertThat(size_of(str), sizeof(int) + str.size());

        auto t1 = make_tuple(int(1), string("22"), vector<string>{"333", "4444"});
        int t1expected = sizeof(int);
        t1expected += sizeof(int) + 2;
        t1expected += sizeof(int) + (sizeof(int) + 3) + (sizeof(int) + 4);
        int t1size = size_of(t1);
        AssertThat(t1size, t1expected);
    }

    TestCase(serialize_simple)
    {

    }

    TestCase(deserialize_simple)
    {

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