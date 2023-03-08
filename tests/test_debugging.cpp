#include <rpp/debugging.h>
#include <rpp/tests.h>
using namespace rpp;

TestImpl(test_debugging)
{
    TestInit(test_debugging)
    {
    }

    TestCleanup()
    {
        SetLogErrorHandler(nullptr);
    }

    TestCase(debug_api)
    {
        std::string a = "string";
        rpp::strview b = "strview";
        int   c = 42;
        float d = 42.0f;
        char  e = '4';

        static std::string log_output;
        SetLogErrorHandler([](LogSeverity sev, const char* err, int len)
        {
            (void)sev;
            log_output = std::string{err, err+len};
        });

        LogInfo("Log(0)");
        AssertThat(log_output, "$ Log(0)");

        LogInfo("Log(1): '%s'", a);
        AssertThat(log_output, "$ Log(1): 'string'");

        LogInfo("Log(2): '%s', '%s'", a, b);
        AssertThat(log_output, "$ Log(2): 'string', 'strview'");

        LogInfo("Log(3): '%s', '%s', %d", a, b, c);
        AssertThat(log_output, "$ Log(3): 'string', 'strview', 42");

        LogInfo("Log(4): '%s', '%s', %d, %.1f", a, b, c, d);
        AssertThat(log_output, "$ Log(4): 'string', 'strview', 42, 42.0");

        LogInfo("Log(5): '%s', '%s', %d, %.1f, `%c`", a, b, c, d, e);
        AssertThat(log_output, "$ Log(5): 'string', 'strview', 42, 42.0, `4`");

        LogInfo("Log(6): '%s', '%s', %d, %.1f, `%c`, '%s'", a, b, c, d, e, a);
        AssertThat(log_output, "$ Log(6): 'string', 'strview', 42, 42.0, `4`, 'string'");

        LogInfo("Log(7): '%s', '%s', %d, %.1f, `%c`, '%s', '%s'", a, b, c, d, e, a, b);
        AssertThat(log_output, "$ Log(7): 'string', 'strview', 42, 42.0, `4`, 'string', 'strview'");

        LogInfo("Log(8): '%s', '%s', %d, %.1f, `%c`, '%s', '%s', %d", a, b, c, d, e, a, b, c);
        AssertThat(log_output, "$ Log(8): 'string', 'strview', 42, 42.0, `4`, 'string', 'strview', 42");

        LogWarning("Warn(0):");
        AssertThat(log_output, "test_debugging.cpp:57 test_debug_api $ Warn(0):");

        LogWarning("Warn(1): '%s'", a);
        AssertThat(log_output, "test_debugging.cpp:60 test_debug_api $ Warn(1): 'string'");

        LogWarning("Warn(2): '%s', '%s'", a, b);
        AssertThat(log_output, "test_debugging.cpp:63 test_debug_api $ Warn(2): 'string', 'strview'");

        LogWarning("Warn(3): '%s', '%s', %d", a, b, c);
        AssertThat(log_output, "test_debugging.cpp:66 test_debug_api $ Warn(3): 'string', 'strview', 42");

        LogWarning("Warn(4): '%s', '%s', %d, %.1f", a, b, c, d);
        AssertThat(log_output, "test_debugging.cpp:69 test_debug_api $ Warn(4): 'string', 'strview', 42, 42.0");

        LogWarning("Warn(5): '%s', '%s', %d, %.1f, `%c`", a, b, c, d, e);
        AssertThat(log_output, "test_debugging.cpp:72 test_debug_api $ Warn(5): 'string', 'strview', 42, 42.0, `4`");

        LogWarning("Warn(6): '%s', '%s', %d, %.1f, `%c`, '%s'", a, b, c, d, e, a);
        AssertThat(log_output, "test_debugging.cpp:75 test_debug_api $ Warn(6): 'string', 'strview', 42, 42.0, `4`, 'string'");

        LogWarning("Warn(7): '%s', '%s', %d, %.1f, `%c`, '%s', '%s'", a, b, c, d, e, a, b);
        AssertThat(log_output, "test_debugging.cpp:78 test_debug_api $ Warn(7): 'string', 'strview', 42, 42.0, `4`, 'string', 'strview'");

        LogWarning("Warn(8): '%s', '%s', %d, %.1f, `%c`, '%s', '%s', %d", a, b, c, d, e, a, b, c);
        AssertThat(log_output, "test_debugging.cpp:81 test_debug_api $ Warn(8): 'string', 'strview', 42, 42.0, `4`, 'string', 'strview', 42");
    }

    //TestCase(log_except)
    //{
    //    try
    //    {
    //        throw std::runtime_error("aaarghh!! something happened!");
    //    }
    //    catch (std::exception& e)
    //    {
    //        std::string param = "test@user.com";
    //        LogExcept(e, "This should trigger a breakpoint in your IDE. Testing log except with params: %s", param);
    //    }
    //}

    TestCaseExpectedEx(must_throw, std::runtime_error)
    {
        throw std::runtime_error{"This error is expected"};
    }

    TestCase(assert_throws)
    {
        AssertThrows(throw std::runtime_error{"error!"}, std::runtime_error);
    }
};
