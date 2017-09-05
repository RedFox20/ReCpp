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
        LogInfo("No arguments");

        std::string s = "hello string";
        LogWarning("test std::string '%s'", s);

        rpp::strview sv = "hello strview";
        LogInfo("test more args '%s', %d, '%s'", s, 42, sv);
    }

} Impl;