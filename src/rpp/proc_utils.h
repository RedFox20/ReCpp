/**
 * Cross platform process information, Copyright (c) 2024, Jorma Rebane
 * Distributed under MIT Software License
 */
#pragma once
#include "config.h"

namespace rpp
{
    struct proc_mem_info
    {
        /// @brief Total virtual memory reserved for this process.
        //         Includes all memory mapped files, swap, etc.
        uint64 virtual_size = 0;

        /// @brief Resident set size (RSS) is the actual physical memory
        //         currently mapped to the process.
        uint64 physical_mem = 0;
    };

    /**
     * @return Current physical memory usage in bytes.
     */
    proc_mem_info proc_current_mem_used() noexcept;

    struct proc_cpu_info
    {
        /// @brief Total CPU time used by this process in microseconds.
        ///        User + Kernel.
        uint64 cpu_time_us = 0;

        /// @brief Total CPU time used by this process in user mode in microseconds.
        uint64 user_time_us = 0;

        /// @brief Total CPU time used by this process in kernel mode in microseconds.
        uint64 kernel_time_us = 0;
    };

    /**
     * @return Total CPU time used by this process in microseconds.
     * @brief To calculate CPU usage %, call this function twice over a known time interval.
     */
    proc_cpu_info proc_total_cpu_usage() noexcept;

} // namespace rpp
