#include "threads.h"
#include "debugging.h"

#if !RPP_BARE_METAL
# include <thread> // hardware_concurrency
# include <vector>
# include "file_io.h" // rpp::file::read_all_text, reading the cgroup CPU quota
# include "minmax.h"  // rpp::max
# if __APPLE__ || __linux__
#  include <pthread.h>
#  include <unistd.h> // getpid()
# endif
# if __linux__
#  include <sched.h> // sched_getaffinity
# endif
//////////////////////////////////////////////////////////////////////////////////////////

# if _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #  define WIN32_LEAN_AND_MEAN 1
    #endif
    #include <Windows.h>
    #include <sysinfoapi.h> // GetLogicalProcessorInformation
    #pragma pack(push,8)
    struct THREADNAME_INFO
    {
        DWORD dwType; // Must be 0x1000.
        LPCSTR szName; // Pointer to name (in user addr space).
        DWORD dwThreadID; // Thread ID (-1=caller thread).
        DWORD dwFlags; // Reserved for future use, must be zero.
    };
    #pragma pack(pop)
# endif
#else
# if RPP_FREERTOS
    #include <FreeRTOS.h>
    #include <task.h>
    #include <portmacro.h>
# endif
#endif // !RPP_BARE_METAL

namespace rpp
{
    //////////////////////////////////////////////////////////////////////////////////////////
#if !RPP_BARE_METAL
# if _MSC_VER
    static int to_wchar_str(wchar_t* out, int maxlen, rpp::strview str) noexcept
    {
        int outlen = str.size() < maxlen ? str.size() : maxlen-1;
        for (int i = 0; i < outlen; ++i)
            out[i] = wchar_t(str[i]);
        out[outlen] = L'\0';
        return outlen;
    }
# endif

    void set_this_thread_name(rpp::strview name) noexcept
    {
        #if _MSC_VER
            // set the global thread name for regular consumers
            wchar_t wname[64];
            to_wchar_str(wname, sizeof(wname), name);

            if (FAILED(SetThreadDescription(GetCurrentThread(), wname))) {
                LogError("set_this_thread_name('%s') failed", name);
            }

            // and then set it specifically for MSVC Debugger
            char threadName[33];
            THREADNAME_INFO info { 0x1000, name.to_cstr(threadName, sizeof(threadName)), DWORD(-1), 0 };
            #pragma warning(push)
            #pragma warning(disable: 6320 6322)
                const DWORD MS_VC_EXCEPTION = 0x406D1388;
                __try {
                    RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
                } __except (1){}
            #pragma warning(pop)
        #elif !RPP_BARE_METAL
            // pthread limit is 16 chars, including null terminator
            char threadName[16] = {0};
            size_t n = name.size() < 15 ? name.size() : 15;
            memcpy(threadName, name.data(), n);
            #if __APPLE__
                int r = pthread_setname_np(threadName);
            #else
                int r = pthread_setname_np(pthread_self(), threadName);
            #endif
            if (r != 0)
            {
                int err = errno;
                LogError("set_this_thread_name('%s') failed: %s", threadName, strerror(err));
            }
        #endif
    }

    std::string get_this_thread_name() noexcept
    {
        return get_thread_name(get_thread_id());
    }

    std::string get_thread_name(uint64 thread_id) noexcept // NOLINT(bugprone-exception-escape)
    {
        std::string thread_name;
        if (thread_id != 0)
        {
        #if _WIN32
            if (HANDLE thread_handle = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)thread_id))
            {
                PWSTR name = nullptr;
                if (SUCCEEDED(GetThreadDescription(thread_handle, &name)))
                {
                    thread_name = rpp::to_string(name); // wchar_t* to std::string
                    LocalFree(name);
                }
                CloseHandle(thread_handle);
            }
        #else
            char name[64];
            if (pthread_getname_np(static_cast<pthread_t>(thread_id), name, sizeof(name)) == 0)
            {
                thread_name = name;
            }
        #endif
        }
        return thread_name;
    }

    uint64 get_thread_id() noexcept
    {
        #if _WIN32
            return GetCurrentThreadId();
        #else
            return (uint64)pthread_self();
        #endif
    }

    uint32 get_process_id() noexcept
    {
    #if _WIN32
        return (uint32)GetCurrentProcessId();
    #else
        return (uint32)getpid();
    #endif
    }

#if __linux__
    /// @returns the cores the cgroup CPU quota allows, or 0 when nothing caps the group.
    ///          A file that holds "max" or -1 parses to 0 or a negative, which means no cap.
    static int cgroup_quota_cores() noexcept
    {
        // cgroup v2 puts "<quota> <period>" in one file
        std::string cpu_max = rpp::file::read_all_text("/sys/fs/cgroup/cpu.max");
        rpp::strview line { cpu_max };
        rpp::int64 quota  = line.next(' ').to_int64();
        rpp::int64 period = line.to_int64();
        if (quota <= 0) // cgroup v1 splits the same pair over two files
        {
            std::string q = rpp::file::read_all_text("/sys/fs/cgroup/cpu/cpu.cfs_quota_us");
            std::string p = rpp::file::read_all_text("/sys/fs/cgroup/cpu/cpu.cfs_period_us");
            quota  = rpp::strview{q}.to_int64();
            period = rpp::strview{p}.to_int64();
        }
        return (quota > 0 && period > 0) ? rpp::max(1, int(quota / period)) : 0;
    }
#endif

    // A container caps CPU with a cgroup quota or an affinity mask, and neither the core count
    // nor hardware_concurrency() sees the cap, so a pool sized by them oversubscribes.
    // @returns the cores this process may use, or 0 when nothing caps it
    static int max_usable_cores() noexcept
    {
    #if __linux__ // covers Android and Yocto
        if (int cores = cgroup_quota_cores())
            return cores;
        cpu_set_t set;
        CPU_ZERO(&set);
        if (sched_getaffinity(0, sizeof(set), &set) == 0)
        {
            int n = 0;
            for (int i = 0; i < CPU_SETSIZE; ++i)
                if (CPU_ISSET(i, &set)) ++n;
            if (n > 0) return n;
        }
    #elif _WIN32
        DWORD_PTR process_mask = 0, system_mask = 0;
        if (GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask))
        {
            int n = 0;
            for (DWORD_PTR m = process_mask; m; m >>= 1)
                n += int(m & 1);
            if (n > 0) return n;
        }
    #endif
        return 0; // macOS and iOS cap nothing we can read
    }

# if _WIN32
    int num_physical_cores() noexcept
    {
        static int num_cores = []
        {
            DWORD bytes = 0;
            GetLogicalProcessorInformation(nullptr, &bytes);
            std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> coreInfo(bytes / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
            GetLogicalProcessorInformation(coreInfo.data(), &bytes);

            int cores = 0;
            for (SYSTEM_LOGICAL_PROCESSOR_INFORMATION& info : coreInfo)
            {
                if (info.Relationship == RelationProcessorCore)
                    ++cores;
            }
            if (cores <= 0) cores = 1;
            if (int usable = max_usable_cores(); usable > 0 && usable < cores)
                cores = usable;
            return cores;
        }();
        return num_cores;
    }
# else
    int num_physical_cores() noexcept
    {
        static int num_cores = []
        {
        // TODO: figure out which types of CPU-s have SMT/HT
        #if MIPS || RASPI || YOCTO_LINUX || RPP_ANDROID
            constexpr int hyperthreading_factor = 1;
        #else
            constexpr int hyperthreading_factor = 2;
        #endif
            int n = (int)std::thread::hardware_concurrency() / hyperthreading_factor;
            if (n <= 0) n = 1;
            // the CIRCLECI guess this replaces divided by 4, which is still wrong on a 3 CPU box
            if (int usable = max_usable_cores(); usable > 0 && usable < n)
                n = usable;
            return n;
        }();
        return num_cores;
    }
# endif

#endif // !RPP_BARE_METAL

    void yield() noexcept
    {
        #if RPP_FREERTOS
            if (xPortIsInsideInterrupt())
                return; // cannot yield from ISR
            taskYIELD();
        #elif RPP_BARE_METAL && RPP_ARM_ARCH
            asm volatile("dsb");
            asm volatile("wfi"); // Wait For Interrupt - puts the CPU to sleep until the next interrupt occurs
        #else
            std::this_thread::yield();
        #endif
    }

    //////////////////////////////////////////////////////////////////////////////////////////
}
