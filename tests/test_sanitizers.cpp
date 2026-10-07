#include <rpp/tests.h>

// gcc defines __SANITIZE_THREAD__, and clang answers __has_feature instead
#if defined(__SANITIZE_THREAD__)
    #define RPP_TSAN_BUILD 1
#elif defined(__has_feature)
    #if __has_feature(thread_sanitizer)
        #define RPP_TSAN_BUILD 1
    #endif
#endif

#if RPP_TSAN_BUILD && !_WIN32
#include <dlfcn.h> // dlsym, RTLD_DEFAULT
#include <cstring> // strstr

// TSAN cannot see the refcounts inside the uninstrumented standard library, so an object
// freed after the last reader released looks like a race. See BUGS.md C25.
extern "C" __attribute__((visibility("default"))) // -fvisibility=hidden hides it from libtsan.so
const char* __tsan_default_suppressions() { // NOLINT(bugprone-reserved-identifier)
    return "race:std::__1::promise\n"           // libc++ puts every promise in namespace __1
           "race:std::__future_base\n"          // libstdc++ keeps the shared state here
           "race:std::__exception_ptr\n"        // and the exception refcount here
           "race:std::runtime_error::~runtime_error\n"
           "race:std::range_error::~range_error\n"
           // the report for this one names no library frame, so only the test scopes it
           "race:test_future::test_except_handler_chaining\n";
}
#endif

#if RPP_TSAN_BUILD && __linux__
#include <rpp/mutex.h>
#include <atomic> // std::atomic_int
#include <memory> // std::make_unique
#include <mutex> // std::lock_guard
#include <sanitizer/tsan_interface.h>

// NOLINTBEGIN(bugprone-reserved-identifier,readability-redundant-declaration) the gcc tsan_interface.h omits it
extern "C" int __tsan_get_report_data(void* report, const char** description, int* count, int* stack_count,
                                      int* mop_count, int* loc_count, int* mutex_count, int* thread_count,
                                      int* unique_tid_count, void** sleep_trace, unsigned long trace_size);
static std::atomic_bool expect_lock_order { false };
static std::atomic_int lock_order_reports { 0 };

namespace __tsan
{
    struct ReportDesc;
    // libtsan calls this weak hook for every report, and a true result drops the report
    __attribute__((visibility("default"))) bool OnReport(const ReportDesc* rep, bool suppressed)
    {
        const char* type = "";
        int n = 0;
        void* trace = nullptr;
        __tsan_get_report_data(const_cast<ReportDesc*>(rep), &type, &n, &n, &n, &n, &n, &n, &n, &trace, 0);
        if (!expect_lock_order || strcmp(type, "lock-order-inversion") != 0)
            return suppressed;
        ++lock_order_reports;
        return true;
    }
}
// NOLINTEND(bugprone-reserved-identifier,readability-redundant-declaration)
#endif

TestImpl(test_sanitizers)
{
    TestInit(test_sanitizers)
    {
    }

    // gcc links libtsan.so, which reads the hook through the global dynamic symbol table.
    // A hidden or unexported symbol leaves every suppression dead. See BUGS.md C25.
    TestCase(tsan_suppression_hook_is_reachable)
    {
    #if RPP_TSAN_BUILD && !_WIN32 && defined(__GNUC__) && !defined(__clang__)
        // libtsan.so carries its own weak hook, so the address alone proves nothing
        using hook = const char* (*)();
        hook global = reinterpret_cast<hook>(dlsym(RTLD_DEFAULT, "__tsan_default_suppressions"));
        AssertNotEqual(global, nullptr);
        const char* patterns = global();
        AssertNotEqual(patterns, nullptr);
        AssertNotEqual(strstr(patterns, "race:std::__future_base"), nullptr);
    #elif RPP_TSAN_BUILD
        // clang links its runtime statically, so the linker binds the hook and dlsym never sees it
        AssertNotEqual(strstr(__tsan_default_suppressions(), "race:std::__future_base"), nullptr);
    #else
        AssertTrue(true); // only a TSAN build carries the hook
    #endif
    }

    // TSAN sees only the atomics of rpp::futex_mutex, so the mutex annotates its own locks
    TestCase(tsan_sees_futex_mutex_lock_order_inversion)
    {
    #if RPP_TSAN_BUILD && __linux__
        auto m = std::make_unique<rpp::futex_mutex[]>(2); // deleting the array clears the lock order which TSAN learns here
        lock_order_reports = 0;
        expect_lock_order = true;
        for (int first : { 0, 1 }) // the second round takes the locks in the other order
        {
            std::lock_guard outer { m[first] };
            std::lock_guard inner { m[1 - first] };
        }
        expect_lock_order = false;
        AssertThat(lock_order_reports.load(), 1);
    #else
        AssertTrue(true); // only a TSAN build carries the annotations
    #endif
    }
};
