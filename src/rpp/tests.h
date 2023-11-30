#pragma once
/**
 * Minimal Unit Testing Framework, Copyright (c) 2016-2018, Jorma Rebane
 * Distributed under MIT Software License
 */
// most of these includes are for convenience in TestImpl's not for tests.cpp
#include <cstdio>  // some basic printf etc.
#include <cstdint> // uint32_t, etc
#include <cmath>   // fabs
#include <cfloat>  // FLT_EPSILON, DBL_EPSILON
#include <vector>  // access to std::vector and std::string
#include <typeinfo>
#include <rpp/sprint.h> // we love strview and sprint, so it's a common dependency
#include <rpp/debugging.h> // for adapting debugging API-s with tests API

namespace rpp
{
#if _MSC_VER
    // warning C4251: 'rpp::test::name': struct 'rpp::strview' needs to have dll-interface to be used by clients of struct 'rpp::test'
    #pragma warning( disable : 4251 ) 
#endif

    struct test;
    struct test_info;
    struct test_results;

    using test_factory = std::unique_ptr<test> (*)(strview name);

    struct RPPAPI test_info
    {
        strview name;
        test_factory factory;

        strview case_filter;      // internal: only execute test cases that pass this filter
        bool test_enabled = true; // internal: this is automatically set by the test system
        bool auto_run     = true; // internal: will this test run automatically (true) or do you have to specify it? (false)
    };

    RPPAPI void register_test(strview name, test_factory factory, bool autorun);

    enum class TestVerbosity : int
    {
        None        = 0, // 0: silent, only failure summary is shown
        Summary     = 1, // 1: show test summary [OK] or [FAILED] with error details
        TestLabels  = 2, // 2: show individual test labels
        AllMessages = 3, // 3: show all information test messages
    };

    struct RPPAPI test
    {
        struct dummy { };

        // minimal version from delegate.h for impossibly fast delegates
        #if _MSC_VER  // VC++
            #if INTPTR_MAX != INT64_MAX // __thiscall only applies for 32-bit MSVC
                using memb_type = void (__thiscall*)(void*);
            #else
                using memb_type = void (*)(void*);
            #endif
            using dummy_type = void (dummy::*)();
        #elif __clang__
            using memb_type  = void (*)(void*);
            using dummy_type = void (dummy::*)();
        #else
            using memb_type  = void (*)(void*);
            using dummy_type = void (dummy::*)(void*);
        #endif

        union test_func_type {
            memb_type mfunc;
            dummy_type dfunc;
        };

        struct test_func;
        struct test_impl;

        strview name;
    private:
        test_impl* impl = nullptr;

    public:
        explicit test(strview name);
        virtual ~test() noexcept;
        void assert_failed(const char* file, int line, const char* fmt, ...);
        void assert_failed_custom(const char* fmt, ...);
    private:
        void add_assert_failure(const char* file, int line, const char* msg, int len);
        static void add_message(int type, const char* msg, int len);
    public:
        static void print_error(const char* fmt, ...);
        static void print_warning(const char* fmt, ...);
        static void print_info(const char* fmt, ...);

        /**
         * Adapter for rpp/debugging.h, enables piping `LogInfo()` etc into test framework output
         * @code
         * SetLogErrorHandler(&rpp::test::log_adapter);
         * @endcode
         */
        static void log_adapter(LogSeverity severity, const char* err, int len);

        /**
         * Runs all the registered tests on this test impl.
         * If a non-empty methodFilter is provided, then only test methods that contain
         * the methodFilter substring are run
         * @return TRUE if ALL tests passed, FALSE if any failure
         */
        bool run_test(test_results& results, strview methodFilter = {});
    private:
        bool run_test_func(test_func& test);

    public:
        /**
         * @brief Generic sleep for testing purposes
         * @warning This is not a high-precision sleep, it's only for vague waiting purposes
         * @param millis Milliseconds to sleep, give or take 15ms (on windows)
         */
        static void sleep(int millis);

        /**
         * Spin sleep is more accurate than std::this_thread::sleep_for
         * which can be seriously bad on windows (+- 15ms)
         * @note Does not rely on timer.h implementation
         * @param seconds Seconds to sleep, should be very accurate
         */
        static void spin_sleep_for(double seconds);

        // main entry/initialization point for the test class
        virtual void init_test() {}

        // optionally clean up the test
        virtual void cleanup_test() {}

        bool run_init();
        void run_cleanup();

        /**
         * Run tests with a single filter pattern
         * Example:
         *   run_tests("test_strview");          -- Specifically runs "test_strview" test module
         *   run_tests("strview");               -- All test modules that contain "strview"
         *   run_tests("test_strview.compare")   -- Only runs specific 'compare' test under "test_strview"
         *   run_tests("strview.compare")        -- All test modules that contain "strview" and have "compare" subtest
         *   run_tests("strview file future")    -- All tests that contain "strview" or "file" or "future"
         *   run_tests("-test_strview -strview") -- All tests except these;
         * @return 0 on success, -1 on failure
         */
        static int run_tests(strview testNamePatterns);

        /**
         * Pass multiple patterns for enabling multiple different tests
         */
        static int run_tests(const char** testNamePatterns, int numPatterns);

        /**
         * Same as run_tests(const char*), but it expects program argc and argv from main()
         * As usual to C++, argv[0] should be exe name and argv[1...] should be testNamePatterns 
         */
        static int run_tests(int argc, char* argv[]);

        /**
         * Runs ALL the tests, no filtering is done
         */
        static int run_tests();

        /**
         * For clean leak detection, this allows to deallocate all unit testing state.
         * Call this after run_tests() to cleanup everything.
         * 
         * After calling this, unit tests cannot be run again.
         */
        static void cleanup_all_tests();

        /**
         * Sets the verbosity level for unit tests.
         * The default verbosity level is 1
         * TestVerbosity::None        0: silent, only failure summary is shown
         * TestVerbosity::Summary     1: show test summary [OK] or [FAILED] with error details
         * TestVerbosity::TestLabels  2: show individual test labels
         * TestVerbosity::AllMessages 3: show all information test messages
         */
        static void set_verbosity(TestVerbosity verbosity);

        template<class T> static std::string as_short_string(const T& obj, int maxLen = 512)
        {
            rpp::string_buffer sb;
            sb << obj;
            if (sb.size() > maxLen)
            {
                sb.resize(maxLen);
                sb << "...";
            }
            return sb.str();
        }

        template<class Actual, class Expected>
        void assumption_failed(const char* file, int line,
            const char* expr, const Actual& actual, const char* why, const Expected& expected)
        {
            std::string sActual = as_short_string(actual);
            std::string sExpect = as_short_string(expected);
            assert_failed(file, line, "%s => '%s' %s '%s'", expr, sActual.c_str(), why, sExpect.c_str());
        }

        template<class Actual, class Min, class Max>
        void assumption_failed(const char* file, int line,
            const char* expr, const Actual& actual, const char* why, const Min& min, const Max& max)
        {
            std::string sActual = as_short_string(actual);
            std::string sMin = as_short_string(min);
            std::string sMax = as_short_string(max);
            assert_failed(file, line, "%s => '%s' %s min:'%s' max:'%s'", expr, sActual.c_str(), why, sMin.c_str(), sMax.c_str());
        }

        int add_test_func(strview name, test_func_type fn, size_t expectedExHash, bool autorun);

        // adds a test to the automatic test run list
        template<class TestClass>
        static int add_test_func(TestClass* self, strview name, void (TestClass::*test_method)(), 
                                 const std::type_info* ti = nullptr, bool autorun = true)
        {
            test_func_type fn;
            #if _MSC_VER // VC++ and MSVC clang
                fn.dfunc = reinterpret_cast<dummy_type>(test_method);
            #elif __clang__
                fn.dfunc = reinterpret_cast<dummy_type>(test_method);
            #elif __GNUG__ // G++
                #pragma GCC diagnostic push
                #pragma GCC diagnostic ignored "-Wpmf-conversions"
                #pragma GCC diagnostic ignored "-Wpedantic"
                fn.mfunc = (memb_type)((*self).*test_method); // de-virtualize / pfm-conversion
                #pragma GCC diagnostic pop
            #endif

            size_t expectedExHash = ti ? ti->hash_code() : 0;
            return self->add_test_func(name, fn, expectedExHash, autorun);
        }
    };

    
    template<class T>
    bool operator==(const std::vector<T>& a, const std::vector<T>& b)
    {
        size_t len = a.size();
        if (len != b.size()) return false;
        for (size_t i = 0; i < len; ++i)
            if (a[i] != b[i]) return false;
        return true;
    }

    struct Compare
    {
        template<class Expr, class Expected> static bool eq(const Expr& expr, const Expected& expected)
        {
            return expr == expected;
        }
        static bool eq(unsigned int expr, int expected) noexcept { return expr == (unsigned int)expected; }
        static bool eq(long unsigned int expr, int expected) noexcept { return expr == (long unsigned int)expected; }
        static bool eq(long unsigned int expr, long int expected) noexcept { return expr == (long unsigned int)expected; }
        static bool eq(long unsigned long expr, long long expected) noexcept { return expr == (long unsigned long)expected; }
        static bool eq(float expr, float expected) noexcept { return fabs(expr - expected) < (FLT_EPSILON*2); }
        static bool eq(double expr, double expected) noexcept { return fabs(expr - expected) < (DBL_EPSILON*2); }

        template<class Expr, class Than> static bool gt(const Expr& expr, const Than& than) noexcept
        {
            return expr > than;
        }
        static bool gt(unsigned int expr, int than) noexcept { return expr > (unsigned int)than; }
        static bool gt(long unsigned int expr, int than) noexcept { return expr > (long unsigned int)than; }
        static bool gt(long unsigned int expr, long int than) noexcept { return expr > (long unsigned int)than; }
        static bool gt(long unsigned long expr, long long than) noexcept { return expr > (long unsigned long)than; }

        template<class Expr, class Than> static bool lt(const Expr& expr, const Than& than) noexcept
        {
            return expr < than;
        }
        static bool lt(unsigned int expr, int than) noexcept { return expr < (unsigned int)than; }
        static bool lt(long unsigned int expr, int than) noexcept { return expr < (long unsigned int)than; }
        static bool lt(long unsigned int expr, long int than) noexcept { return expr < (long unsigned int)than; }
        static bool lt(long unsigned long expr, long long than) noexcept { return expr < (long unsigned long)than; }

        template<class Expr, class Than> static bool lte(const Expr& expr, const Than& than) noexcept
        {
            return lt(expr, than) || eq(expr, than);
        }

        template<class Expr, class Than> static bool gte(const Expr& expr, const Than& than) noexcept
        {
            return gt(expr, than) || eq(expr, than);
        }

        template<class Expr, class Min, class Max> static bool rngEx(const Expr& expr, const Min& min, const Max& max) noexcept
        {
            return gt(expr, min) && lt(expr, max);
        }
        template<class Expr, class Min, class Max> static bool rngInc(const Expr& expr, const Min& min, const Max& max) noexcept
        {
            return gte(expr, min) && lte(expr, max);
        }
    };


#undef Assert
#undef AssertFailed
#undef AssertTrue
#undef AssertFalse
#undef AssertMsg
#undef AssertThat
#undef AssertEqual
#undef AssertThrows
#undef AssertNotEqual
#undef AssertGreater
#undef AssertLess
#undef AssertGreaterOrEqual
#undef AssertLessOrEqual
#undef TestImpl
#undef TestInit
#undef TestInitNoAutorun
#undef TestCleanup
#undef __TestLambda
#undef TestCase
#undef TestCaseExpectedEx

#define Assert(expr) do { \
    if (!(expr)) { assumption_failed(__FILE__, __LINE__, #expr, false, "but expected", true); } \
}while(0)

#define AssertFailed(fmt, ...) do { \
    assert_failed(__FILE__, __LINE__, "assertion failed => " fmt, ##__VA_ARGS__); \
}while(0)

#define AssertTrue Assert
#define AssertFalse(expr) do { \
    if ((expr)) { assumption_failed(__FILE__, __LINE__, #expr, true, "but expected", false); } \
}while(0)

#define AssertMsg(expr, fmt, ...) do { \
    if (!(expr)) { assert_failed(__FILE__, __LINE__, #expr " $ " fmt, ##__VA_ARGS__); } \
}while(0)

#define AssertThat(expr, expected) do { \
    const auto& __expr   = expr;        \
    const auto& __expect = expected;    \
    if (!rpp::Compare::eq(__expr, __expect)) { \
        assumption_failed(__FILE__, __LINE__, #expr, __expr, "but expected", __expect); \
    } \
}while(0)

#define AssertEqual(expr, expected) do { \
    const auto& __expr   = expr;        \
    const auto& __expect = expected;    \
    if (!rpp::Compare::eq(__expr, __expect)) { \
        assumption_failed(__FILE__, __LINE__, #expr, __expr, "but expected", __expect); \
    } \
}while(0)

#define AssertThrows(expr, exceptionType) do { \
    try {                                      \
        expr;                                  \
        assert_failed(__FILE__, __LINE__, "%s => expected exception of type %s", #expr, #exceptionType); \
    } catch (const exceptionType&) {} \
}while(0)

#define AssertNotEqual(expr, mustNotEqual) do { \
    const auto& __expr    = expr;               \
    const auto& __mustnot = mustNotEqual;       \
    if (rpp::Compare::eq(__expr, __mustnot)) { \
        assumption_failed(__FILE__, __LINE__, #expr, __expr, "must not equal", __mustnot); \
    } \
}while(0)

#define AssertGreater(expr, than) do { \
    const auto& __expr = expr;         \
    const auto& __than = than;         \
    if (!rpp::Compare::gt(__expr, __than)) { \
        assumption_failed(__FILE__, __LINE__, #expr, __expr, "must be greater than", __than); \
    } \
}while(0)

#define AssertLess(expr, than) do { \
    const auto& __expr = expr;      \
    const auto& __than = than;      \
    if (!rpp::Compare::lt(__expr, __than)) { \
        assumption_failed(__FILE__, __LINE__, #expr, __expr, "must be less than", __than); \
    } \
}while(0)

#define AssertGreaterOrEqual(expr, than) do { \
    const auto& __expr = expr;                \
    const auto& __than = than;                \
    if (!rpp::Compare::gte(__expr, __than)) { \
        assumption_failed(__FILE__, __LINE__, #expr, __expr, "must be greater or equal than", __than); \
    } \
}while(0)

#define AssertLessOrEqual(expr, than) do { \
    const auto& __expr = expr;             \
    const auto& __than = than;             \
    if (!rpp::Compare::lte(__expr, __than)) { \
        assumption_failed(__FILE__, __LINE__, #expr, __expr, "must be less or equal than", __than); \
    } \
}while(0)

// whether expr is in range (INCLUSIVE) [rangeMin, rangeMax]
#define AssertInRange(expr, rangeMin, rangeMax) do { \
    const auto& __expr = expr;                       \
    const auto& __rmin = rangeMin;                   \
    const auto& __rmax = rangeMax;                   \
    if (!rpp::Compare::rngInc(__expr, __rmin, __rmax)) { \
        assumption_failed(__FILE__, __LINE__, #expr, __expr, "must be within inclusive range ["#rangeMin","#rangeMax"]", __rmin, __rmax); \
    } \
}while(0)

// whether expr is in range (EXCLUSIVE) (rangeMin, rangeMax)
#define AssertExRange(expr, rangeMin, rangeMax) do { \
    const auto& __expr = expr;                       \
    const auto& __rmin = rangeMin;                   \
    const auto& __rmax = rangeMax;                   \
    if (!rpp::Compare::rngEx(__expr, __rmin, __rmax)) { \
        assumption_failed(__FILE__, __LINE__, #expr, __expr, "must be within exclusive range ("#rangeMin","#rangeMax")", __rmin, __rmax); \
    } \
}while(0)

#define TestImpl(testclass) struct testclass : public rpp::test

#define __TestInit(testclass, autorun)                                \
    explicit testclass(rpp::strview name) : rpp::test{name} {}        \
    static std::unique_ptr<test> __create(rpp::strview name)          \
    { return std::unique_ptr<test>{ new testclass{name} }; }          \
    RPP_INLINE_STATIC bool __registered = [] {                        \
        rpp::register_test(#testclass, &__create, autorun);           \
        return true;                                                  \
    }();                                                              \
    using ClassType = testclass;                                      \
    ClassType* self() { return this; }                                \
    void init_test() override

#define TestInit(testclass)          __TestInit(testclass, true)
#define TestInitNoAutorun(testclass) __TestInit(testclass, false)

#define TestCleanup() void cleanup_test() override


#if _MSC_VER
#  define __TestLambda(testname) [&](){ this->test_##testname(); }
#else
#  define __TestLambda(testname) [&](){ self()->test_##testname(); }
#endif

#define TestCase(testname) \
    const int _test_##testname = add_test_func(self(), #testname, &ClassType::test_##testname ); \
    void test_##testname()

#define TestCaseExpectedEx(testname, expectedExceptionType) \
    const int _test_##testname = add_test_func(self(), #testname, &ClassType::test_##testname, &typeid(expectedExceptionType)); \
    void test_##testname()

#define TestCaseNoAutorun(testname) \
    const int _test_##testname = add_test_func(self(), #testname, &ClassType::test_##testname, nullptr, false); \
    void test_##testname()

}
