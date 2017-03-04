#pragma once
#ifndef RPP_TESTSFRAMEWORK_HPP
#define RPP_TESTSFRAMEWORK_HPP
// most of these includes are for convenience in TestImpl's not for tests.cpp
#include <cstdio>  // some basic printf etc.
#include <vector>  // access to std::vector and std::string
#include <string>
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

    struct test_func
    {
        strview name;
        function<void()> func;
    };

    struct test
    {
        static int asserts_failed;

        const char* name;

        // all the automatic tests to run
        vector<test_func> test_funcs;

        test(const char* name);
        virtual ~test();
        static void assert_failed(const char* file, int line, const char* expression, ...);

        enum ConsoleColor { Default, Green, Yellow, Red, };

        // colored printing to stdout and stderr if color == Red
        static void consolef(ConsoleColor color, const char* fmt, ...);

        /**
         * Runs all the registered tests on this test impl.
         * If a non-empty methodFilter is provided, then only test methods that contain
         * the methodFilter substring are run
         */
        void run_test(strview methodFilter = {});

        // adds a test to the automatic run list
        int add_test_func(test_func&& func);

        // generic sleep for testing purposes
        static void sleep(int millis);

        // main entry/initialization point for the test class
        virtual void run() {}

        /**
         * Run tests with a single filter pattern
         * Example:
         *   run_tests("test_strview");        -- Specifically runs "test_strview" test module
         *   run_tests("strview");             -- All test modules that contain "strview"
         *   run_tests("test_strview.compare") -- Only runs specific 'compare' test under "test_strview"
         *   run_tests("strview.compare")      -- All test modules that contain "strview" and have "compare" subtest
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

        template<class Expected, class Actual>
        static void expected_failed(const char* file, int line, 
                    const Expected& expected, const Actual& actual, 
                    const char* expectedExpr, const char* actualExpr)
        {
            assert_failed(file, line, "Expected %s(%s) but got %s(%s) instead.",
                expectedExpr, to_string(expected).c_str(),
                actualExpr, to_string(actual).c_str());
        }
    };

#define Assert(expr) if (!(expr)) { assert_failed(__FILE__, __LINE__, #expr); }
#define AssertMsg(expr, fmt, ...) if (!(expr)) { assert_failed(__FILE__, __LINE__, #expr " $ " fmt, ##__VA_ARGS__); }
#define AssertThat(expected, actual) \
    if ((expected) != (actual)) { \
        expected_failed(__FILE__, __LINE__, expected, actual, #expected, #actual); }

#define TestImpl(testclass) static struct testclass : public test

#define TestInit(testclass)                \
    testclass() : test(#testclass){}       \
    void run() override

#define TestCase(testname) \
    const int _test_##testname = add_test_func({strview{#testname}, [this](){ this->test_##testname(); }}); \
    void test_##testname()
}

#endif // RPP_TESTSFRAMEWORK_HPP
