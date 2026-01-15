#pragma once
#include "config.h"
#include "strview.h"

namespace rpp
{
    //////////////////////////////////////////////////////////////////////////////////////////
#if !RPP_BARE_METAL
    /**
     * @brief Sets the debug name for this thread
     * @warning Linux pthreads only supports max length 15 characters for thread names
     */
    RPPAPI void set_this_thread_name(rpp::strview name) noexcept;

    /**
     * @returns The debug name of this thread
     */
    RPPAPI std::string get_this_thread_name() noexcept;

    /**
     * @returns The debug name of the thread with the given ID (@see rpp::get_thread_id())
     */
    RPPAPI std::string get_thread_name(uint64 thread_id) noexcept;

    /**
     * @returns Current thread ID as a 64-bit integer
     */
    RPPAPI uint64 get_thread_id() noexcept;

    /**
     * @returns Current process ID as a 32-bit integer
     */
    RPPAPI uint32 get_process_id() noexcept;

    /**
     * @returns Number of physicals cores on the system
     */
    RPPAPI int num_physical_cores() noexcept;
#endif // !RPP_BARE_METAL

    /**
     * @brief Yields execution to another thread
     */
    RPPAPI void yield() noexcept;

    //////////////////////////////////////////////////////////////////////////////////////////
}
