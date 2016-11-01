#pragma once
#ifndef TESTFRAMEWORK_TESTS_HPP
#define TESTFRAMEWORK_TESTS_HPP
// these includes are for convenience in TestImpl's not for tests.cpp
#include <stdio.h> // some basic printf etc.
#include <vector>  // access to std::vector and std::string
#include <string>
#include <functional>
using namespace std;

using test_func = function<void()>;

struct test
{
    static int asserts_failed;
    const char* name;

    // all the automatic tests to run
    vector<test_func> test_funcs;

    test(const char* name);
    virtual ~test();
    static void assert_failed(const char* file, int line, const char* expression, ...);
    void run_test();

    // adds a test to the automatic run list
    int add_test_func(test_func&& func);

    // generic sleep for testing purposes
    static void sleep(int millis); 
    
    // main entry/initialization point for the test class
    virtual void run() = 0;
};

#define Assert(expr) if (!(expr)) { assert_failed(__FILE__, __LINE__, #expr); }
#define AssertMsg(expr, fmt, ...) if (!(expr)) { assert_failed(__FILE__, __LINE__, #expr " $ " fmt, ##__VA_ARGS__); }
#define TestImpl(testclass) static struct testclass : public test


#define TestInit(testclass)                \
    testclass() : test(#testclass){}       \
    void run() override

#define TestCase(testname) \
    const int _test_##testname = add_test_func([this](){ this->test_##testname(); }); \
    void test_##testname()

#endif // TESTFRAMEWORK_TESTS_HPP
