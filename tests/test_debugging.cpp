#include <rpp/debugging.h>
#include <rpp/timer.h>
#include <rpp/tests.h>
using namespace rpp;

static std::string log_output;

#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define LINE_STR STRINGIZE(__LINE__)

// _va_comma picks __VA_OPT__ where the preprocessor has it, so this reaches the fallback everywhere
#define LogInfoFallback(format, ...) _LogInfo("$ " format _rpp_wrap_args_fallback(__VA_ARGS__))

template<class...T> static constexpr int va_count(T... args) { return sizeof...(args); }
#define VaCount(...) va_count(0 _rpp_va_comma_fallback(__VA_ARGS__) __VA_ARGS__)

static_assert(VaCount() == 1, "an empty list must not gain a comma");
static_assert(VaCount(1) == 2, "one argument must gain a comma");
static_assert(VaCount(1, 2) == 3, "two arguments must gain one comma");
static_assert(VaCount((true) ? 1 : 2) == 2, "a leading ( must not read as an empty list");
static_assert(VaCount((long long)(1)) == 2, "a C-style cast must not read as an empty list");
static_assert(VaCount((true) ? 1 : 2, 3) == 3, "a leading ( must keep the arguments after it");

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

    TestCase(paren_first_arg)
    {
        bool flyView = false;
        long long v = 42;
        log_output.clear();

        LogInfo("flyView %s", (flyView) ? "true" : "false");
        AssertThat(log_output, "$ flyView false");

        LogInfo("cast %lld", (long long)(v));
        AssertThat(log_output, "$ cast 42");

        LogInfo("mixed %s %lld %d", (flyView) ? "true" : "false", (long long)(v), 7);
        AssertThat(log_output, "$ mixed false 42 7");

        LogWarning("warn %s", (flyView) ? "t" : "f"); AssertThat(log_output, "test_debugging.cpp:" LINE_STR " test_paren_first_arg $ warn f");

        std::string thrown;
        try { ThrowErr("throw %s", (flyView) ? "t" : "f"); } catch (const std::runtime_error& e) { thrown = e.what(); }
        AssertThat(thrown, "throw f");
    }

    TestCase(paren_first_arg_va_comma_fallback)
    {
        log_output.clear();

        LogInfoFallback("no args");
        AssertThat(log_output, "$ no args");

        LogInfoFallback("flyView %s", (false) ? "true" : "false");
        AssertThat(log_output, "$ flyView false");

        LogInfoFallback("cast %lld", (long long)(42));
        AssertThat(log_output, "$ cast 42");

        LogInfoFallback("mixed %s %lld %d", (false) ? "true" : "false", (long long)(42), 7);
        AssertThat(log_output, "$ mixed false 42 7");

        LogInfoFallback("plain %s %lld", "str", 42LL);
        AssertThat(log_output, "$ plain str 42");
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
        rpp::TimePoint timeBefore = rpp::TimePoint::system_now(); // debugging.cpp uses TimePoint::system_now() -- system clock
        LogInfo("TimestampTest");
        rpp::TimePoint timeAfter = rpp::TimePoint::system_now();
        ::LogEnableTimestamps(false);

        std::string timestamp = rpp::strview{log_output}.split_first('$').trim();
        std::string message = rpp::strview{log_output}.split_second('$');

        AssertGreater(timestamp.size(), 11);
        AssertEqual(message, " TimestampTest");
        print_info("Timestamp: '%s' message: '%s'\n", timestamp.c_str(), message.c_str());

        // the timestamp is captured inside LogInfo, so it can be anywhere between before and after
        std::string before = timeBefore.to_string(3);
        std::string after = timeAfter.to_string(3);
        AssertMsg(timestamp >= before && timestamp <= after,
                  "timestamp '%s' expected in range ['%s', '%s']", timestamp.c_str(), before.c_str(), after.c_str());
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
