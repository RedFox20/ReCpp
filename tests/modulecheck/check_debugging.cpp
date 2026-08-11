/**
 * Module-only consumer check. It includes no rpp header except the macro header,
 * so it fails to compile when rpp.debugging forgets to export a name.
 * The normal test suite cannot check this: <rpp/tests.h> pulls in <rpp/debugging.h>.
 */
#include <rpp/debugging.macros.h> // LogInfo, LogWarning, LogError, Assert, ThrowErr
#include <stdexcept>              // ThrowErr throws std::runtime_error
#include <string>

import rpp.debugging; // includes come first, the import goes last

static void handler(void*, LogSeverity, const char*, int) noexcept {}

int check_debugging()
{
    SetLogSeverityFilter(LogSeverityWarn);
    if (GetLogSeverityFilter() != LogSeverityWarn)
        return 1;

    LogInfo("filtered out at Warn: %d", 42);
    LogWarning("visible %d", 7);
    LogInfoFL("file.cpp", 1, "func", "custom source %d", 1);

    std::string s = "wrapped through rpp::__wrap";
    LogWarning("%s", s); // the macro calls rpp::__wrap<std::string>, not .c_str()

    AssertExpr(GetLogSeverityFilter() == LogSeverityWarn);
    Assert(true, "never fires %d", 0);

    rpp::add_log_handler(nullptr, &handler);
    rpp::remove_log_handler(nullptr, &handler);

    try { ThrowErr("thrown %d", 3); }
    catch (const std::exception& e) { LogWarning("caught %s", e.what()); }

    SetLogSeverityFilter(LogSeverityInfo);
    return 0;
}
