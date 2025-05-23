#pragma once
/**
 * Stack tracing and traced exceptions, Copyright (c) 2017-2018, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "config.h"
#include "strview.h"
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdint>

namespace rpp
{
    /**
     * @brief Represents a single entry in a callstack and provides file, line and function name information
     */
    struct RPPAPI CallstackEntry
    {
        uint64_t addr = 0;  // if 0, we have no valid entry
        int      line = 0;
        std::string name; // function name
        std::string file; // if empty, we have no valid file info, try using module name instead
        std::string module;

        explicit CallstackEntry(uint64_t addr) noexcept : addr{addr} {}
        CallstackEntry() noexcept = default; // required for std::vector<CallstackEntry>

        /** @return short file or module path from long `file` path */
        const char* short_path() const noexcept;

        /** @return clean function name from long symbolicated name */
        std::string clean_name() const noexcept;

        /**
         * @return A string representation of this callstack entry
         * "[file]:[line]  in  [function]"
         */
        std::string to_string() const noexcept;
    };

    /**
     * @brief Performs lookup for address information
     * @brief addr Address to lookup
     */
    RPPAPI CallstackEntry get_address_info(uint64_t addr) noexcept;

    // absolute limit for callstack depth
    static inline constexpr size_t CALLSTACK_MAX_DEPTH = 256u;

    /**
     * @brief Walks the stack and returns a list of callstack addresses.
     * You can then use get_address_info() to get more information about each address,
     * or use format_trace() to get a formatted stack trace.
     * @param maxDepth Maximum number of stack frames to trace
     * @param entriesToSkip Number of initial entries to skip in order to hide stack tracing internals
     * @return List of callstack addresses
     */
    RPPAPI std::vector<uint64_t> get_callstack(size_t maxDepth = 32, size_t entriesToSkip = 0) noexcept;

    /**
     * @brief Walks the stack and returns a list of callstack addresses.
     * You can then use get_address_info() to get more information about each address,
     * or use format_trace() to get a formatted stack trace.
     * @param callstack [out] Pre-allocated buffer for callstack addresses
     * @param maxDepth Maximum number of stack frames to trace
     * @param entriesToSkip Number of initial entries to skip in order to hide stack tracing internals
     * @return Number of callstack entries written to the buffer
     */
    RPPAPI int get_callstack(uint64_t* callstack, size_t maxDepth, size_t entriesToSkip = 0) noexcept;

    /**
     * Prepends an error message before formatting the stack trace
     * @param message Message to prepend to stack trace
     * @param callstack [in] Pre-walked callstack
     * @param depth Number of entries in the callstack
     * @return Formatted stack trace with available debug information. Line information is not always available.
     */
    RPPAPI std::string format_trace(rpp::strview message, const uint64_t* callstack, size_t depth) noexcept;

    /**
     * @brief Generic implementation of stack trace, taking a pre-walked callstack
     * @param callstack [in] Pre-walked callstack
     * @return Formatted stack trace with available debug information. Line information is not always available.
     */
    inline std::string format_trace(const std::vector<uint64_t>& callstack) noexcept
    {
        return rpp::format_trace(rpp::strview{}, callstack.data(), callstack.size());
    }

    /**
     * Prepends an error message before formatting the stack trace
     * @param message Message to prepend to stack trace
     * @param callstack [in] Pre-walked callstack
     * @return Formatted stack trace with available debug information. Line information is not always available.
     */
    inline std::string format_trace(rpp::strview message, const std::vector<uint64_t>& callstack) noexcept
    {
        return rpp::format_trace(message, callstack.data(), callstack.size());
    }

    /**
     * Base implementation of stack trace. Only needed if you're implementing custom abstractions
     * @param message [nullable] Message to prepend to stack trace
     * @param messageLen Length of the message (or 0)
     * @param maxDepth Maximum number of stack frames to trace
     * @param entriesToSkip Number of initial entries to skip in order to hide stack tracing internals
     * @return Stack trace with available debug information. Line information is not always available.
     * @note
     * On linux you must compile with -rdynamic, otherwise internal symbols won't be visible
     * @endnote
     */
    RPPAPI std::string stack_trace(rpp::strview message, size_t maxDepth = 32,
                                   size_t entriesToSkip = 2) noexcept;

    /**
     * Prepares stack trace
     */
    inline std::string stack_trace(size_t maxDepth = 32) noexcept
    {
        return rpp::stack_trace(rpp::strview{}, maxDepth, 2);
    }


    /**
     * Prints stack trace to STDERR
     */
    RPPAPI void print_trace(size_t maxDepth = 32) noexcept;
    /**
     * Prints stack trace to STDERR WITH error message
     */
    RPPAPI void print_trace(rpp::strview message, size_t maxDepth = 32) noexcept;


    /**
     * @return Prepared runtime_error with error message and stack trace
     */
    RPPAPI std::runtime_error error_with_trace(rpp::strview message, size_t maxDepth = 32) noexcept;


    /**
     * Traced exception forms a complete [message]\\n[stacktrace] string
     * which can be retrieved via runtime_error::what()
     */
    struct RPPAPI traced_exception : std::runtime_error
    {
        explicit traced_exception(rpp::strview message) noexcept;
    };


    /**
     * Installs a default handler for SIGSEGV which will
     * throw a traced_exception instead of quietly terminating
     * @note Will cause std::terminate if SIGSEGV happens in noexcept context
     */
    RPPAPI void register_segfault_tracer();

    /**
     * Installs a default handler for SIGSEGV which will
     * prints stack trace to stderr and then calls std::terminate
     */
    RPPAPI void register_segfault_tracer(std::nothrow_t);

}
