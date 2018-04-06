#pragma once
/**
 * Minimal Unit Testing Framework, Copyright (c) 2016-2018, Jorma Rebane
 * Distributed under MIT Software License
 */
// most of these includes are for convenience in TestImpl's not for tests.cpp
#include <cstdio>  // some basic printf etc.
#include <vector>  // access to std::vector and std::string
#include <rpp/sprint.h> // we love strview and sprint, so it's a common dependency
#include <typeinfo>

/**
 * If you want the tests framework to define the main function, then
 * set this to 1 in PROJECT BUILD SETTINGS,
 * otherwise you have to call rpp::run_tests(...) manually to run any of the tests
 */
#ifndef RPP_TESTS_DEFINE_MAIN
#define RPP_TESTS_DEFINE_MAIN 0
#endif

namespace rpp
{
    using std::string;
    using std::vector;
    using std::move;
    using std::unique_ptr;

#if _MSC_VER
    // warning C4251: 'rpp::test::name': struct 'rpp::strview' needs to have dll-interface to be used by clients of struct 'rpp::test'
    #pragma warning( disable : 4251 ) 
#endif

    struct test;
    struct test_info;

    using test_factory = unique_ptr<test> (*)(strview name);

    struct RPPAPI test_info
    {
        strview name;
        test_factory factory;

        strview case_filter;      // internal: only execute test cases that pass this filter
        bool test_enabled = true; // internal: this is automatically set by the test system
        bool auto_run     = true; // internal: will this test run automatically (true) or do you have to specify it? (false)
    };

    extern RPPAPI vector<test_info> all_tests;

    RPPAPI void register_test(strview name, test_factory factory, bool autorun);

    struct RPPAPI test
    {
        struct lambda_base { test* self; };
        struct test_func
        {
            strview name;
            lambda_base lambda;
            void (lambda_base::*func)();
            size_t expectedExType;
            bool autorun = true;
        };

        static int asserts_failed;
        strview name;
    private:
        // @note Need to use raw array because std::vector cannot be exported on MSVC
        test_func* test_funcs = nullptr; // all the automatic tests to run
        int test_count = 0;
        int test_cap = 0;

    
    public:
        explicit test(strview name);
        virtual ~test();
        static void assert_failed(const char* file, int line, const char* fmt, ...);

        enum ConsoleColor { Default, Green, Yellow, Red, };

        // colored printing to stdout and stderr if color == Red
        static void consolef(ConsoleColor color, const char* fmt, ...);

        /**
         * Runs all the registered tests on this test impl.
         * If a non-empty methodFilter is provided, then only test methods that contain
         * the methodFilter substring are run
         */
        void run_test(strview methodFilter = {});
        void run_test(test_func& test);

        // generic sleep for testing purposes
        static void sleep(int millis);

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
         *   run_tests("-test_strview -strview") -- All tests except these;
         * @return 0 on success, -1 on failure
         */
        static int run_tests(const char* testNamePattern);

        /**
         * Pass multiple patterns for enabling multiple different tests
         */
        static int run_tests(const vector<string>& testNamePatterns);
        static int run_tests(const char** testNamePatterns, int numPatterns);
        template<int N> static int run_tests(const char* (&testNamePatterns)[N]) {
            return run_tests(testNamePatterns, N);
        }

        /**
         * Same as run_tests(const char*), but it expects program argc and argv from main()
         * As usual to C++, argv[0] should be exe name and argv[1...] should be testNamePatterns 
         */
        static int run_tests(int argc, char* argv[]);

        /**
         * Runs ALL the tests, no filtering is done
         */
        static int run_tests();

        static const string& as_string(const string& v) { return v; }

        static string as_string(const strview& s) { return { s.str, (size_t)s.len }; }
        static string as_string(const char* s)    { return string{s};}
        static string as_string(std::nullptr_t)   { return "null"; }
        static string as_string(int v)    { return rpp::to_string(v); }
        static string as_string(uint v)   { return rpp::to_string(v); }
        static string as_string(long v)   { return rpp::to_string(v); }
        static string as_string(ulong v)  { return rpp::to_string(v); }
        static string as_string(int64 v)  { return rpp::to_string(v); }
        static string as_string(uint64 v) { return rpp::to_string(v); }
        static string as_string(short v)  { return rpp::to_string(v); }
        static string as_string(ushort v) { return rpp::to_string(v); }
        static string as_string(byte v)   { return rpp::to_string(v); }
        template<class T> static string as_string(const T& v) { return to_string(v); }

        template<class T> static string as_short_string(const T& obj, size_t maxLen = 512)
        {
            string s = as_string(obj);
            if (s.size() <= maxLen)
                return s;
            s.resize(maxLen);
            s += "...";
            return s;
        }

        template<class Actual, class Expected>
        static void assumption_failed(const char* file, int line, 
            const char* expr, const Actual& actual, const char* why, const Expected& expected)
        {
            string sActual = as_short_string(actual);
            string sExpect = as_short_string(expected);
            assert_failed(file, line, "%s => '%s' %s '%s'", expr, sActual.c_str(), why, sExpect.c_str());
        }

        int add_test_func(test_func func);

        // adds a test to the automatic test run list
        template<class T, class Lambda> static int add_test_func(
            T* self, strview name, Lambda lambda, const std::type_info* ti = nullptr, bool autorun = true)
        {
            (void)lambda;
            size_t expectedExHash = ti ? ti->hash_code() : 0;
            return self->add_test_func(test_func{ name, {self}, (void (lambda_base::*)())&Lambda::operator(), expectedExHash, autorun });
        }
    };



#undef Assert
#undef AssertMsg
#undef AssertThat
#undef AssertEqual
#undef AssertNotEqual
#undef TestImpl
#undef TestInit
#undef TestInitNoAutorun
#undef TestCleanup
#undef TestLambda
#undef TestCase
#undef TestCaseExpectedEx

#define Assert(expr) if (!(expr)) { assert_failed(__FILE__, __LINE__, #expr); }
#define AssertMsg(expr, fmt, ...) if (!(expr)) { assert_failed(__FILE__, __LINE__, #expr " $ " fmt, ##__VA_ARGS__); }
#define AssertThat(expr, expected) do { \
    const auto& __expr     = expr;           \
    const auto& __expected = expected;       \
    if (__expr != __expected) { assumption_failed(__FILE__, __LINE__, #expr, __expr, "but expected", __expected); } \
} while (false)
#define AssertEqual AssertThat
#define AssertNotEqual(expr, mustNotEqual) do { \
    const auto& __expr    = expr;                    \
    const auto& __mustnot = mustNotEqual;            \
    if (__expr == __mustnot) { assumption_failed(__FILE__, __LINE__, #expr, __expr, "must not equal", __mustnot); } \
} while (false)

#define TestImpl(testclass) struct testclass : public rpp::test

#define __TestInit(testclass, autorun)                                \
    explicit testclass(rpp::strview name) : rpp::test{name} {}        \
    static std::unique_ptr<test> __create(rpp::strview name)          \
    { return std::unique_ptr<test>( new testclass{name} ); }          \
    inline static bool __registered = [] {                            \
        rpp::register_test(#testclass, &__create, autorun);           \
        return true;                                                  \
    }();                                                              \
    using ClassType = testclass;                                      \
    ClassType* self() { return this; }                                \
    void init_test() override

#define TestInit(testclass)          __TestInit(testclass, true)
#define TestInitNoAutorun(testclass) __TestInit(testclass, false)

#define TestCleanup(testclass) void cleanup_test() override


#if _MSC_VER
#  define TestLambda(testname) [=]{ this->test_##testname(); }
#else
#  define TestLambda(testname) [=]{ self()->test_##testname(); }
#endif

#define TestCase(testname) \
    const int _test_##testname = add_test_func(self(), #testname, TestLambda(testname) ); \
    void test_##testname()

#define TestCaseExpectedEx(testname, expectedExceptionType) \
    const int _test_##testname = add_test_func(self(), #testname, TestLambda(testname), &typeid(expectedExceptionType)); \
    void test_##testname()

#define TestCaseNoAutorun(testname) \
    const int _test_##testname = add_test_func(self(), #testname, TestLambda(testname), nullptr, false); \
    void test_##testname()

}
