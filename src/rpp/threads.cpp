#include "threads.h"
#include <thread> // hardware_concurrency
#include <vector>
#if __APPLE__ || __linux__
# include <pthread.h>
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

    void set_this_thread_name(rpp::strview name) noexcept
    {
        #if _MSC_VER
            // set the global thread name for regular consumers
            wchar_t wname[64];
            int wname_len = name.size() < 63 ? name.size() : 63;
            for (int i = 0; i < wname_len; ++i)
                wname[i] = wchar_t(name[i]);
            wname[wname_len] = L'\0';
            SetThreadDescription(GetCurrentThread(), wname);

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
                pthread_setname_np(threadName);
            #elif __linux__
                pthread_setname_np(pthread_self(), threadName);
            #endif
        #endif
    }

    std::string get_this_thread_name() noexcept
    {
        return get_thread_name(get_thread_id());
    }

    std::string get_thread_name(uint64 thread_id) noexcept
    {
        std::string thread_name;
        #if _WIN32
            PWSTR name = nullptr;
            if (SUCCEEDED(GetThreadDescription(HANDLE(thread_id), &name)))
            {
                thread_name = rpp::to_string(name); // wchar_t* to std::string
                LocalFree(name);
            }
        #else
            char name[64];
            if (pthread_getname_np(pthread_t(thread_id), name, sizeof(name)) == 0)
                thread_name = name;
        #endif
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
