#include <rpp/debugging.h>
#include <rpp/tests.h>
using namespace rpp;

TestImpl(test_debugging)
{
    TestInitNoAutorun(test_debugging)
    {
    }

    TestCase(debug_api)
    {
        std::string a = "string";
        rpp::strview b = "strview";
        int   c = 42;
        float d = 42.0f;
        char  e = '4';

        LogInfo("Log(0)");
        LogInfo("Log(1): '%s'", a);
        LogInfo("Log(2): '%s' '%s'", a, b);
        LogInfo("Log(3): '%s', '%s', %d", a, b, c);
        LogInfo("Log(4): '%s', '%s', %d, %.1f", a, b, c, d);
        LogInfo("Log(5): '%s', '%s', %d, %.1f, %c", a, b, c, d, e);
        LogInfo("Log(6): '%s', '%s', %d, %.1f, `%c`, '%s'", a, b, c, d, e, a);
        LogInfo("Log(7): '%s', '%s', %d, %.1f, `%c`, '%s', '%s'", a, b, c, d, e, a, b);
        LogInfo("Log(8): '%s', '%s', %d, %.1f, `%c`, '%s', '%s', %d", a, b, c, d, e, a, b, c);

        LogWarning("Warn(0)");
        LogWarning("Warn(1): '%s'", a);
        LogWarning("Warn(2): '%s' '%s'", a, b);
        LogWarning("Warn(3): '%s', '%s', %d", a, b, c);
        LogWarning("Warn(4): '%s', '%s', %d, %.1f", a, b, c, d);
        LogWarning("Warn(5): '%s', '%s', %d, %.1f, `%c`", a, b, c, d, e);
        LogWarning("Warn(6): '%s', '%s', %d, %.1f, `%c`, '%s'", a, b, c, d, e, a);
        LogWarning("Warn(7): '%s', '%s', %d, %.1f, `%c`, '%s', '%s'", a, b, c, d, e, a, b);
        LogWarning("Warn(8): '%s', '%s', %d, %.1f, `%c`, '%s', '%s', %d", a, b, c, d, e, a, b, c);
    }

    TestCase(log_except)
    {
        try
        {
            throw std::runtime_error("aaarghh!! something happened!");
        }
        catch (std::exception& e)
        {
            std::string param = "test@user.com";
            LogExcept(e, "This should trigger a breakpoint in your IDE. Testing log except with params: %s", param);
        }
    }

} Impl;