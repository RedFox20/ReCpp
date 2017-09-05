#include <rpp/debugging.h>
#include <rpp/tests.h>
using namespace rpp;

TestImpl(test_debugging)
{
    TestInit(test_debugging)
    {
    }

    TestCase(variadic_printf_wrapper)
    {
        std::string s = "hello string";
        LogWarning("test std::string '%s'", s);
    }

} Impl;