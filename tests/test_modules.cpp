/**
 * Checks that the C++20 modules are usable the way a consumer uses them.
 * Only built when the toolchain carries modules, see BUILD_WITH_MODULES in CMakeLists.txt.
 */
#include <rpp/tests.h>

#if RPP_BUILD_WITH_MODULES

#include <rpp/debugging.macros.h> // LogInfo, LogWarning, Assert, ThrowErr
#include <stdexcept>              // ThrowErr throws std::runtime_error
#include <string>

import rpp.strview;   // includes come first, the import goes last
import rpp.debugging;

TestImpl(test_modules)
{
    TestInit(test_modules)
    {
    }

    static void log_handler(void*, LogSeverity, const char*, int) noexcept {}

    // The macros expand to _LogInfo, _LogWarning, _FmtString and rpp::__wrap, so this
    // fails to build if rpp.debugging stops exporting any of them.
    TestCase(debugging_macros_work_beside_the_module)
    {
        LogSeverity previous = GetLogSeverityFilter();
        SetLogSeverityFilter(LogSeverityWarn);
        AssertThat(GetLogSeverityFilter(), LogSeverityWarn);

        LogInfo("filtered out at Warn: %d", 42);
        LogInfoFL("file.cpp", 1, "func", "custom source %d", 1);

        std::string wrapped = "passed through rpp::__wrap";
        LogWarning("%s", wrapped); // no .c_str(), the macro wraps it

        // no Assert() here: <rpp/tests.h> defines its own one-argument Assert and wins
        AssertExpr(GetLogSeverityFilter() == LogSeverityWarn);
        DbgAssert(true, "never fires %d", 0);

        rpp::add_log_handler(nullptr, &log_handler);
        rpp::remove_log_handler(nullptr, &log_handler);

        SetLogSeverityFilter(previous);
    }

    TestCase(throw_err_macro_reaches_the_module)
    {
        bool caught = false;
        try { ThrowErr("thrown %d", 3); }
        catch (const std::exception& e) { caught = true; AssertThat(rpp::strview{e.what()}, "thrown 3"); }
        AssertThat(caught, true);
    }

    // rpp::strview from the module and from <rpp/strview.h> name one type, so a value
    // built here crosses into code that only included the header.
    TestCase(strview_module_and_header_name_one_type)
    {
        using namespace rpp::literals; // the module exports the literal operator too
        rpp::strview sv = "hello world"_sv;
        AssertThat(sv.length(), 11);
        AssertThat(rpp::concat(sv, "!"), "hello world!");
        AssertThat(sv.next(' '), "hello"); // next() consumes the token from sv
        AssertThat(sv, "world");
    }
};

#endif // RPP_BUILD_WITH_MODULES
