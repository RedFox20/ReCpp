#include "tests.h"
#include <rpp/binary_serializer.h>
using namespace rpp;

TestImpl(test_serialization)
{
    TestInit(test_serialization)
    {

    }

    TestCase(object_size)
    {
        Assert(sizeof(byte) == size_of<byte>(0));
        Assert(sizeof(int) == size_of<int>(0));

        vector<int> simpleVec = { 1, 2, 3, 4 };
        int simpleVecSize = size_of(simpleVec);
        int simpleVecExpected = sizeof(int) + sizeof(int) * (int)simpleVec.size();
        AssertMsg(simpleVecSize == simpleVecExpected, "Expected (%d) but got (%d)", simpleVecExpected, simpleVecSize);

        string str = "test";
        Assert(size_of(str) == sizeof(int) + str.size());

        auto t1 = make_tuple(int(1), string("02"), vector<string>{"3", "4"});
        int t1expected = sizeof(int);
        t1expected += sizeof(int) + 2;
        t1expected += sizeof(int) + (sizeof(int) + 1) + (sizeof(int) + 1);
        int t1size = size_of(t1);
        AssertMsg(t1size == t1expected, "Expected (%d) but got (%d)", t1expected, t1size);
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