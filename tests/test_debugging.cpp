#include <rpp/debugging.h>
#include <rpp/timer.h>
#include <rpp/tests.h>
using namespace rpp;

static std::string log_output;

#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define LINE_STR STRINGIZE(__LINE__)

TestImpl(test_debugging)
{
    TestInit(test_debugging)
    {
        SetLogHandler([](LogSeverity severity, const char* message, int len)
        {
            (void)severity;
            log_output = std::string{message, message+len};
        });
    }

    TestCleanup()
    {
        SetLogHandler(nullptr);
        LogEnableTimestamps(false);
    }

    TestCase(debug_api)
    {
        std::string a = "string";
        rpp::strview b = "strview";
        int   c = 42;
        float d = 42.0f;
        char  e = '4';

        log_output.clear();

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

        LogWarning("Warn(0):"); AssertThat(log_output, "test_debugging.cpp:" LINE_STR " test_debug_api $ Warn(0):");
        LogWarning("Warn(1): '%s'", a); AssertThat(log_output, "test_debugging.cpp:" LINE_STR " test_debug_api $ Warn(1): 'string'");
        LogWarning("Warn(2): '%s', '%s'", a, b); AssertThat(log_output, "test_debugging.cpp:" LINE_STR " test_debug_api $ Warn(2): 'string', 'strview'");
        LogWarning("Warn(3): '%s', '%s', %d", a, b, c); AssertThat(log_output, "test_debugging.cpp:" LINE_STR " test_debug_api $ Warn(3): 'string', 'strview', 42");
        LogWarning("Warn(4): '%s', '%s', %d, %.1f", a, b, c, d); AssertThat(log_output, "test_debugging.cpp:" LINE_STR " test_debug_api $ Warn(4): 'string', 'strview', 42, 42.0");
        LogWarning("Warn(5): '%s', '%s', %d, %.1f, `%c`", a, b, c, d, e); AssertThat(log_output, "test_debugging.cpp:" LINE_STR " test_debug_api $ Warn(5): 'string', 'strview', 42, 42.0, `4`");
        LogWarning("Warn(6): '%s', '%s', %d, %.1f, `%c`, '%s'", a, b, c, d, e, a); AssertThat(log_output, "test_debugging.cpp:" LINE_STR " test_debug_api $ Warn(6): 'string', 'strview', 42, 42.0, `4`, 'string'");
        LogWarning("Warn(7): '%s', '%s', %d, %.1f, `%c`, '%s', '%s'", a, b, c, d, e, a, b); AssertThat(log_output, "test_debugging.cpp:" LINE_STR " test_debug_api $ Warn(7): 'string', 'strview', 42, 42.0, `4`, 'string', 'strview'");
        LogWarning("Warn(8): '%s', '%s', %d, %.1f, `%c`, '%s', '%s', %d", a, b, c, d, e, a, b, c); AssertThat(log_output, "test_debugging.cpp:" LINE_STR " test_debug_api $ Warn(8): 'string', 'strview', 42, 42.0, `4`, 'string', 'strview', 42");
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

    TestCase(log_handler_with_timestamps)
    {
        log_output.clear();

        ::LogEnableTimestamps(true);
        rpp::TimePoint time = rpp::TimePoint::now();
        LogInfo("TimestampTest");
        ::LogEnableTimestamps(false);

        std::string timestamp = rpp::strview{log_output}.split_first('$').trim();
        std::string message = rpp::strview{log_output}.split_second('$');

        AssertGreater(timestamp.size(), 11);
        AssertEqual(timestamp, time.to_string(3));
        AssertEqual(message, " TimestampTest");
    }

    TestCaseExpectedEx(must_throw, std::runtime_error)
    {
        throw std::runtime_error{"This error is expected"};
    }

    TestCase(assert_throws)
    {
        AssertThrows(throw std::runtime_error{"error!"}, std::runtime_error);
    }
};
