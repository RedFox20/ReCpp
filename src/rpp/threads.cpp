#include "threads.h"
#include "debugging.h"
#include <thread> // hardware_concurrency
#include <vector>
#if __APPLE__ || __linux__
# include <pthread.h>
# include <unistd.h> // getpid()
#endif
//////////////////////////////////////////////////////////////////////////////////////////

#if _WIN32
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
#endif

namespace rpp
{
    //////////////////////////////////////////////////////////////////////////////////////////

#if _MSC_VER
    static int to_wchar_str(wchar_t* out, int maxlen, rpp::strview str) noexcept
    {
        int outlen = str.size() < maxlen ? str.size() : maxlen-1;
        for (int i = 0; i < outlen; ++i)
            out[i] = wchar_t(str[i]);
        out[outlen] = L'\0';
        return outlen;
    }
#endif

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
        #else
            // pthread limit is 16 chars, including null terminator
            char threadName[16];
            size_t n = name.size() < 15 ? name.size() : 15;
            memcpy(threadName, name.data(), n);
            threadName[n] = '\0';
            #if __APPLE__
                int r = pthread_setname_np(threadName);
            #elif __linux__
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

    std::string get_thread_name(uint64 thread_id) noexcept
    {
        std::string thread_name;
        if (thread_id == 0)
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
            if (pthread_getname_np(pthread_t(thread_id), name, sizeof(name)) == 0)
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

#if _WIN32
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
            return cores > 0 ? cores : 1;
        }();
        return num_cores;
    }
#else
    int num_physical_cores() noexcept
    {
        static int num_cores = []
        {
            // TODO: figure out which types of CPU-s have SMT/HT
            #if MIPS || RASPI || OCLEA || __ANDROID__
                constexpr int hyperthreading_factor = 1;
            #else
                constexpr int hyperthreading_factor = 2;
            #endif
            int n = (int)std::thread::hardware_concurrency() / hyperthreading_factor;
            return n > 0 ? n : 1;
        }();
        return num_cores;
    }
#endif

    //////////////////////////////////////////////////////////////////////////////////////////
}
