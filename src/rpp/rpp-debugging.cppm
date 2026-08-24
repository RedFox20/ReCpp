// C++20 module interface unit for <rpp/debugging.h>.
// A module cannot export a macro, so an importer adds <rpp/debugging.macros.h> for
// LogInfo, LogError and Assert.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "debugging.h"

export module rpp.debugging;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block
export import rpp.config;

export using ::LogSeverity;
export using ::LogSeverityInfo;
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
export using ::_LogInfo;
export using ::_LogWarning;
export using ::_LogError;
export using ::_LogExcept;
export using ::_FmtString;
export using ::_LogFuncname;

export namespace rpp {
    using rpp::shorten_filename;
    using rpp::__wrap;
    using rpp::__clean_type;
    using rpp::LogMsgHandler;
    using rpp::add_log_handler;
    using rpp::remove_log_handler;
}
// GENERATED EXPORTS END

// QtPrintable sits behind RPP_HAS_QT, which the generator does not read, so it stays here
export namespace rpp {
#if RPP_HAS_QT
    using rpp::QtPrintable;
#endif
}
