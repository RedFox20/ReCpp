// C++20 module interface unit for <rpp/debugging.h>
// The macros are NOT here, because a module cannot export one.
// An importer adds `#include <rpp/debugging.macros.h>` for LogInfo, LogError and Assert.
module;

// Global module fragment: the umbrella include. Every declaration below stays
// attached to the global module, so an importer and an includer share one entity.
#include "debugging.h"

export module rpp.debugging;

// The C logging API keeps global scope, so a call site needs no change.
export using ::LogSeverity;
export using ::LogSeverityInfo;   // an unscoped enum does not carry its enumerators
export using ::LogSeverityWarn;
export using ::LogSeverityError;
export using ::LogMessageCallback;
export using ::LogExceptCallback;

export using ::SetLogHandler;
export using ::SetLogExceptHandler;
export using ::LogDisableFunctionNames;
export using ::SetLogSeverityFilter;
export using ::GetLogSeverityFilter;
export using ::LogEnableTimestamps;
export using ::LogSetTimeOffset;
export using ::LogWriteToDefaultOutput;
export using ::RppAssertFail;
export using ::LogFormatv;
export using ::LogWrite;

// The macros of debugging.macros.h expand to these, so an importer needs them visible.
export using ::_LogInfo;
export using ::_LogWarning;
export using ::_LogError;
export using ::_LogExcept;
export using ::_FmtString;
export using ::_LogFuncname;

export namespace rpp
{
    using rpp::shorten_filename;
    using rpp::__wrap;        // the macros wrap each argument through this
    using rpp::__clean_type;
    using rpp::LogMsgHandler;
    using rpp::add_log_handler;
    using rpp::remove_log_handler;
#if RPP_HAS_QT
    using rpp::QtPrintable;
#endif
}
