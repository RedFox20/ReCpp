#include "proc_utils.h"
#if _MSC_VER
#  define WIN32_LEAN_AND_MEAN
#  include <Windows.h> // WINAPI
#  include <Psapi.h> // PROCESS_MEMORY_COUNTERS, GetProcessMemoryInfo
#  include <cstdio> // fprintf
#elif __APPLE__
#  include <mach/mach.h>
#else // linux and Android
#  include <unistd.h> // getpagesize
#  include <sys/time.h> // timeval
#  include <sys/resource.h> // getrusage
#  include "file_io.h" // rpp::file
#endif

namespace rpp
{
    proc_mem_info proc_current_mem_used() noexcept
    {
        proc_mem_info result;
    #if _MSC_VER
        PROCESS_MEMORY_COUNTERS info;
        GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info));
        result.virtual_size = static_cast<uint64>(info.PagefileUsage);
        result.physical_mem = static_cast<uint64>(info.WorkingSetSize);
    #elif __APPLE__
        struct mach_task_basic_info info;
        mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
        if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) == KERN_SUCCESS)
        {
            result.virtual_size = static_cast<uint64>(info.virtual_size);
            result.physical_mem = static_cast<uint64>(info.resident_size);
        }
    #else
        // // https://man7.org/linux/man-pages/man5/proc.5.html
        // // size       (1) total program size (same as VmSize in /proc/pid/status)
        // // resident   (2) resident set size (inaccurate; same as VmRSS in /proc/pid/status)
        // if (rpp::file f { "/proc/self/statm", rpp::file::READONLY })
        // {
        //     f.seek(0, SEEK_SET); // seeking to 0 will force OS to re-generate the data
        //     char buffer[1024];
        //     int len = f.read(buffer, sizeof(buffer));
        //     if (len > 0)
        //     {
        //         static int page_size = getpagesize();
        //         strview contents { buffer, len };
        //         result.virtual_size = contents.next_int64() * page_size;
        //         result.physical_mem = contents.next_int64() * page_size;
        //     }
        // }

        // https://man7.org/linux/man-pages/man5/proc.5.html
        // (23) vsize  %lu  Virtual memory size in bytes.
        // (24) rss  %ld  Resident Set Size: number of pages the process has in real memory.
        // this is more stable than /proc/self/statm - due to OS differences
        if (rpp::file f { "/proc/self/stat", rpp::file::READONLY })
        {
            char buffer[2048];
            int len = f.read(buffer, sizeof(buffer));
            if (len > 0)
            {
                static int page_size = getpagesize();
                int field_id = 0;
                strview contents { buffer, len };
                strview token;
                while (contents.next(token, ' '))
                {
                    ++field_id; // counting from (1)
                    if      (field_id == 23) result.virtual_size = token.to_int64();
                    else if (field_id == 24) result.physical_mem = token.to_int64() * page_size;
                }
            }
        }
    #endif
        return result;
    }

    #if _MSC_VER
    static FINLINE uint64 to_uint64(const FILETIME& f) noexcept
    {
        ULARGE_INTEGER time;
        time.LowPart = f.dwLowDateTime;
        time.HighPart = f.dwHighDateTime;
        return time.QuadPart;
    }
    #endif

    cpu_usage_info proc_total_cpu_usage() noexcept
    {
        cpu_usage_info r;
    #if _MSC_VER
        FILETIME creation_time, exit_time, kernel_time, user_time;
        if (GetProcessTimes(GetCurrentProcess(), &creation_time, &exit_time, &kernel_time, &user_time))
        {
            // convert 100ns intervals to microseconds
            r.kernel_time_us = to_uint64(kernel_time) / 10;
            r.user_time_us   = to_uint64(user_time) / 10;
            r.cpu_time_us    = r.kernel_time_us + r.user_time_us;
        }
    #else // linux
        struct rusage usg;
        getrusage(RUSAGE_SELF, &usg);
        // convert timeval to microseconds
        r.kernel_time_us = usg.ru_stime.tv_sec * 1'000'000ull + (uint64)usg.ru_stime.tv_usec;
        r.user_time_us   = usg.ru_utime.tv_sec * 1'000'000ull + (uint64)usg.ru_utime.tv_usec;
        r.cpu_time_us    = r.kernel_time_us + r.user_time_us;
    #endif
        return r;
    }
}
