#pragma once
/**
 * Minimal wrapper for std::source_location for platforms with outdated toolchains.
 * Copyright (c) 20266, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "config.h"

#if RPP_HAS_CXX20 && __has_include(<source_location>)
#include <source_location> // std::source_location for better assert messages
#  define RPP_HAS_SOURCE_LOCATION 1
#  define RPP_SOURCE_LOC rpp::source_loc loc = { std::source_location::current() }
#  define RPP_SOURCE_LOC_CURRENT { std::source_location::current() }
#else
#  define RPP_HAS_SOURCE_LOCATION 0
#  define RPP_SOURCE_LOC rpp::source_loc loc = { __FILE__, __FUNCTION__, __LINE__ }
#  define RPP_SOURCE_LOC_CURRENT { __FILE__, __FUNCTION__, __LINE__ }
#endif

namespace rpp
{
    /**
     * @brief The problem: older platforms might not have up-to-date std::source_location support
     *        and this causes unnecessary build failures.
     *        This is a minimal wrapper that provides a similar interface as std::source_location
     */
    struct RPPAPI source_loc
    {
        const char* file;
        const char* func;
        int lineno;

        constexpr source_loc(const char* file, const char* func, int lineno) noexcept
            : file{file}
            , func{func}
            , lineno{lineno}
        {
        }

    #if RPP_HAS_SOURCE_LOCATION
        constexpr source_loc(std::source_location loc) noexcept
            : file{loc.file_name()}
            , func{loc.function_name()}
            , lineno{(int)loc.line()}
        {
        }
    #endif

        constexpr const char* file_name() const noexcept { return file; }
        constexpr const char* function_name() const noexcept { return func; }
        constexpr int line() const noexcept { return lineno; }
    };
}
