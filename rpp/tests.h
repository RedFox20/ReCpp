#pragma once
#ifndef RPP_TESTSFRAMEWORK_H
#define RPP_TESTSFRAMEWORK_H
// most of these includes are for convenience in TestImpl's not for tests.cpp
#include <cstdio>  // some basic printf etc.
#include <vector>  // access to std::vector and std::string
#include <functional>
#include <rpp/strview.h> // we love strview, so it's a common dependency :)

/**
 * If you don't want the tests framework to define the main function, then
 * set this to 0 in PROJECT BUILD SETTINGS,
 * which means you have to call rpp::run_tests(...) manually to run any of the tests
 */ 
#ifndef RPP_TESTS_DEFINE_MAIN
#define RPP_TESTS_DEFINE_MAIN 1
#endif

namespace rpp
{
    using namespace std;

    struct test
    {
        struct lambda_base { test* self; };
        struct test_func
        {
            strview name;
            lambda_base lambda;
            void (lambda_base::*func)();
        };

        static int asserts_failed;

        strview name;
        vector<test_func> test_funcs; // all the automatic tests to run
        strview test_specific;
    private:
        bool test_enabled = true; // internal: this is automatically set by the test system
        bool auto_run = true; // internal: will this test run automatically (true) or do you have to specify it? (false)
    
    public:
        explicit test(strview name, bool autorun = true);
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

        // generic sleep for testing purposes
        static void sleep(int millis);

        // main entry/initialization point for the test class
        virtual void run() {}

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
         * Same as run_tests(const char*), but it expects program argc and argv from main()
         * As usual to C++, argv[0] should be exe name and argv[1...] should be testNamePatterns 
         */
        static int run_tests(int argc, char* argv[]);

        /**
         * Runs all the tests, no filtering is done
         */
        static int run_tests();

        static const string& as_string(const string& v) { return v; }
        static string as_string(const strview& s) { return { s.str, (size_t)s.len }; }
        static string as_string(const char* s)    { return string{s};}
        static string as_string(nullptr_t)        { return "null"; }
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

        // adds a test to the automatic test run list
        template<class T, class Lambda> int add_test_func(strview name, T* self, Lambda lambda)
        {
            (void)lambda;
            test_funcs.emplace_back(test_func{ name, {self}, (void (lambda_base::*)())&Lambda::operator() });
            return (int)test_funcs.size() - 1;
        }
    };

#undef Assert
#undef AssertMsg
#undef AssertThat
#undef AssertEqual
#undef AssertNotEqual
#undef TestImpl
#undef TestInit
#undef TestCleanup
#undef TestCase

#define Assert(expr) if (!(expr)) { assert_failed(__FILE__, __LINE__, #expr); }
#define AssertMsg(expr, fmt, ...) if (!(expr)) { assert_failed(__FILE__, __LINE__, #expr " $ " fmt, ##__VA_ARGS__); }
#define AssertThat(expr, expected) \
    if ((expr) != (expected)) { assumption_failed(__FILE__, __LINE__, #expr, expr, "but expected", expected); }

#define AssertEqual AssertThat
#define AssertNotEqual(expr, mustNotEqual) \
    if ((expr) == (mustNotEqual)) { assumption_failed(__FILE__, __LINE__, #expr, expr, "must not equal", mustNotEqual); }

#define TestImpl(testclass) static struct testclass : public rpp::test

#define TestInit(testclass)                \
    testclass() : test(#testclass){}       \
    using ClassType = testclass;           \
    void run() override

#define TestInitNoAutorun(testclass)       \
    testclass() : test(#testclass,false){} \
    using ClassType = testclass;           \
    void run() override

#define TestCleanup(testclass) ~testclass()

#define TestCase(testname) \
    const int _test_##testname = add_test_func(#testname, this, [this]{ test_##testname();}); \
    void test_##testname()

}

#endif // RPP_TESTSFRAMEWORK_H
