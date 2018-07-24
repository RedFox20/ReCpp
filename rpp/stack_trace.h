#pragma once
/**
 * Stack tracing and traced exceptions, Copyright (c) 2017-2018, Jorma Rebane
 * Distributed under MIT Software License
 */
#include <stdexcept>
#include <string>
#include "config.h"

namespace rpp
{
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
    RPPAPI std::string stack_trace(const char* message, size_t messageLen, int maxDepth, int entriesToSkip) noexcept;


    /**
     * Prepares stack trace
     */
    RPPAPI std::string stack_trace(int maxDepth = 32) noexcept;
    /**
     * Prepares stack trace WITH error message
     */
    RPPAPI std::string stack_trace(const char* message, int maxDepth = 32) noexcept;
    RPPAPI std::string stack_trace(const std::string& message, int maxDepth = 32) noexcept;


    /**
     * Prints stack trace to STDERR
     */
    RPPAPI void print_trace(int maxDepth = 32) noexcept;
    /**
     * Prints stack trace to STDERR WITH error message
     */
    RPPAPI void print_trace(const char* message, int maxDepth = 32) noexcept;
    RPPAPI void print_trace(const std::string& message, int maxDepth = 32) noexcept;


    /**
     * @return Prepared runtime_error with error message and stack trace
     */
    RPPAPI std::runtime_error error_with_trace(const char* message, int maxDepth = 32) noexcept;
    RPPAPI std::runtime_error error_with_trace(const std::string& message, int maxDepth = 32) noexcept;


    /**
     * Traced exception forms a complete [message]\\n[stacktrace] string
     * which can be retrieved via runtime_error::what()
     */
    struct RPPAPI traced_exception : std::runtime_error
    {
        explicit traced_exception(const char* message) noexcept;
        explicit traced_exception(const std::string& message) noexcept;
    };


    /**
     * Installs a default handler for SIGSEGV which will
     * throw a traced_exception instead of terminating
     * @note Will cause std::terminate if SIGSEGV happens in noexcept context
     */
    RPPAPI void register_segfault_tracer();
    /**
     * Installs a default handler for SIGSEGV which will
     * prints stack trace to stderr and then calls std::terminate
     */
    RPPAPI void register_segfault_tracer(std::nothrow_t);

}