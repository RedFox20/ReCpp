// C++20 module interface unit for the rpp.core headers, owned by tools/gen_module_exports.py.
// The headers stay in the global module fragment, so an importer and an includer share one entity.
module;

#include "config.types.h"
#include "debugging.h"
#include "source_loc.h"
#include "traits.h"
#include "type_traits.h"
#include "predicates.h"
#include "scope_guard.h"
#include "delegate.h"
#include "proc_utils.h"
#include "stack_trace.h"
#include "endian.h"
#include "bitutils.h"

export module rpp.core;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block

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
    using rpp::byte;
    using rpp::ushort;
    using rpp::uint;
    using rpp::ulong;
    using rpp::int16;
    using rpp::uint16;
    using rpp::int32;
    using rpp::uint32;
    using rpp::int64;
    using rpp::uint64;
    using rpp::__wrap;
    using rpp::shorten_filename;
    using rpp::__clean_type;
    using rpp::LogMsgHandler;
    using rpp::add_log_handler;
    using rpp::remove_log_handler;
    using rpp::source_loc;
    using rpp::function_traits;
    using rpp::first_arg_type;
    using rpp::cont_return_t;
    using rpp::task_return_t;
    using rpp::is_detected;
    using rpp::is_detected_v;
    using rpp::has_to_string;
    using rpp::has_to_string_memb;
    using rpp::has_get_memb;
    using rpp::has_set_memb;
    using rpp::is_iterable;
    using rpp::is_stringlike;
    using rpp::is_container;
    using rpp::IsCallable;
    using rpp::IsPredicate;
    using rpp::scope_finalizer;
    using rpp::make_scope_guard;
    using rpp::delegate;
    using rpp::multicast_delegate;
    using rpp::multicast_fwd;
    using rpp::multicast_fwd_t;
    using rpp::proc_mem_info;
    using rpp::proc_current_mem_used;
    using rpp::cpu_usage_info;
    using rpp::proc_total_cpu_usage;
    using rpp::CallstackEntry;
    using rpp::get_address_info;
    using rpp::CALLSTACK_MAX_DEPTH;
    using rpp::get_callstack;
    using rpp::ThreadCallstack;
    using rpp::get_all_callstacks;
    using rpp::format_trace;
    using rpp::stack_trace;
    using rpp::print_trace;
    using rpp::error_with_trace;
    using rpp::traced_exception;
    using rpp::register_segfault_tracer;
    using rpp::writeBEU16;
    using rpp::writeBEU32;
    using rpp::writeBEU64;
    using rpp::readBEU16;
    using rpp::readBEU32;
    using rpp::readBEU64;
    using rpp::writeLEU16;
    using rpp::writeLEU32;
    using rpp::writeLEU64;
    using rpp::readLEU16;
    using rpp::readLEU32;
    using rpp::readLEU64;
    using rpp::bit_array;
}
// GENERATED EXPORTS END

// QtPrintable sits behind RPP_HAS_QT, which the generator cannot parse without Qt
export namespace rpp {
#if RPP_HAS_QT
    using rpp::QtPrintable;
#endif
}
