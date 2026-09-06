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

TestImpl(test_sanitizers)
{
    TestInit(test_sanitizers)
    {
    }

    // gcc links libtsan.so, which reads the hook through the global dynamic symbol table.
    // A hidden or unexported symbol leaves every suppression dead. See BUGS.md C25.
    TestCase(tsan_suppression_hook_is_reachable)
    {
    #if RPP_TSAN_BUILD && !_WIN32
        // libtsan.so carries its own weak hook, so the address alone proves nothing
        using hook = const char* (*)();
        hook global = reinterpret_cast<hook>(dlsym(RTLD_DEFAULT, "__tsan_default_suppressions"));
        AssertNotEqual(global, nullptr);
        const char* patterns = global();
        AssertNotEqual(patterns, nullptr);
        AssertNotEqual(strstr(patterns, "race:std::__future_base"), nullptr);
    #else
        AssertTrue(true); // only a TSAN build carries the hook
    #endif
    }
};
