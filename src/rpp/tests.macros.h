#pragma once
/**
 * Unit test macros, Copyright (c) 2016-2018, Jorma Rebane
 * Distributed under MIT Software License
 *
 * A C++20 module cannot export a macro, so every macro of <rpp/tests.h> lives here.
 *
 *   #include <rpp/tests.h>          // classic: gives the declarations AND these macros
 *   import rpp.tests;               // module: gives the declarations
 *   #include <rpp/tests.macros.h>   // module: add this line for the macros
 *
 * Each macro below calls a name that <rpp/tests.h> or `import rpp.tests` declares.
 * Include one of the two first. The includes here carry the two macros an import cannot,
 * plus the three std names the macros expand to.
 */
#include "source_loc.h" // RPP_SOURCE_LOC_CURRENT
#include "future_types.h" // RPP_HAS_COROUTINES, which guards TestCaseCoro
#include <memory> // std::unique_ptr, which __TestInit returns
#include <typeinfo> // typeid, which TestCaseExpectedEx takes
#include <exception> // std::exception, which AssertNoThrowAny catches


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
    if (!(expr)) { rpp::test::assumption_failed(RPP_SOURCE_LOC_CURRENT, #expr, false, "BUT EXPECTED", true); } \
}while(0)

#define AssertFailed(fmt, ...) do { \
    rpp::test::assert_failed(RPP_SOURCE_LOC_CURRENT, "assertion failed => " fmt, ##__VA_ARGS__); \
}while(0)
#define AssertFailedLoc(source_loc, fmt, ...) do { \
    rpp::test::assert_failed((source_loc), "assertion failed => " fmt, ##__VA_ARGS__); \
}while(0)

#define AssertTrue Assert
#define AssertFalse(expr) do { \
    if ((expr)) { rpp::test::assumption_failed(RPP_SOURCE_LOC_CURRENT, #expr, true, "BUT EXPECTED", false); } \
}while(0)

// Asserts that expression is true, otherwise displays a custom formatted error message
#define AssertMsg(expr, fmt, ...) do { \
    if (!(expr)) { rpp::test::assert_failed(RPP_SOURCE_LOC_CURRENT, "%s $ " fmt, #expr, ##__VA_ARGS__); } \
}while(0)

#define AssertThat(expr, expected) do { \
    const auto& __expr   = expr;        \
    const auto& __expect = expected;    \
    if (!rpp::Compare::eq(__expr, __expect)) { \
        rpp::test::assumption_failed(RPP_SOURCE_LOC_CURRENT, #expr, __expr, "BUT EXPECTED", __expect); \
    } \
}while(0)

#define AssertThatLoc(source_loc, expr, expected) do { \
    const auto& __expr   = expr;        \
    const auto& __expect = expected;    \
    if (!rpp::Compare::eq(__expr, __expect)) { \
        rpp::test::assumption_failed((source_loc), #expr, __expr, "BUT EXPECTED", __expect); \
    } \
}while(0)

#define AssertEqual(expr, expected) do { \
    const auto& __expr   = expr;        \
    const auto& __expect = expected;    \
    if (!rpp::Compare::eq(__expr, __expect)) { \
        rpp::test::assumption_failed(RPP_SOURCE_LOC_CURRENT, #expr, __expr, "BUT EXPECTED", __expect); \
    } \
}while(0)

// Asserts that this expression does the specified exception
#define AssertThrows(expr, exceptionType) do { \
    try {                                      \
        expr;                                  \
        rpp::test::assert_failed(RPP_SOURCE_LOC_CURRENT, "%s => expected exception of type %s", #expr, #exceptionType); \
    } catch (const exceptionType&) {} \
}while(0)

// Asserts that this expression does not throw ANY exceptions
#define AssertNoThrowAny(expr) do { \
    try {                           \
        expr;                       \
    } catch (const std::exception& e) { \
        rpp::test::assert_failed(RPP_SOURCE_LOC_CURRENT, "%s => expected no exceptions but got: %s", #expr, e.what()); \
    } catch (...) { \
        rpp::test::assert_failed(RPP_SOURCE_LOC_CURRENT, "%s => expected no exceptions", #expr); \
    } \
}while(0)

// Asserts that this expression does not throw the specified exception type
#define AssertNoThrowExType(expr, exceptionType) do { \
    try {                                      \
        expr;                                  \
    } catch (const exceptionType&) {           \
        rpp::test::assert_failed(RPP_SOURCE_LOC_CURRENT, "%s => expected no exception of type %s", #expr, #exceptionType); \
    } catch (...) { /**any other ex is ok**/ }\
}while(0)

#define AssertNotEqual(expr, mustNotEqual) do { \
    const auto& __expr    = expr;               \
    const auto& __mustnot = mustNotEqual;       \
    if (rpp::Compare::eq(__expr, __mustnot)) { \
        rpp::test::assumption_failed(RPP_SOURCE_LOC_CURRENT, #expr, __expr, "must not equal", __mustnot); \
    } \
}while(0)

#define AssertGreater(expr, than) do { \
    const auto& __expr = expr;         \
    const auto& __than = than;         \
    if (!rpp::Compare::gt(__expr, __than)) { \
        rpp::test::assumption_failed(RPP_SOURCE_LOC_CURRENT, #expr, __expr, "must be greater than", __than); \
    } \
}while(0)

#define AssertLess(expr, than) do { \
    const auto& __expr = expr;      \
    const auto& __than = than;      \
    if (!rpp::Compare::lt(__expr, __than)) { \
        rpp::test::assumption_failed(RPP_SOURCE_LOC_CURRENT, #expr, __expr, "must be less than", __than); \
    } \
}while(0)

#define AssertGreaterOrEqual(expr, than) do { \
    const auto& __expr = expr;                \
    const auto& __than = than;                \
    if (!rpp::Compare::gte(__expr, __than)) { \
        rpp::test::assumption_failed(RPP_SOURCE_LOC_CURRENT, #expr, __expr, "must be greater or equal than", __than); \
    } \
}while(0)

#define AssertLessOrEqual(expr, than) do { \
    const auto& __expr = expr;             \
    const auto& __than = than;             \
    if (!rpp::Compare::lte(__expr, __than)) { \
        rpp::test::assumption_failed(RPP_SOURCE_LOC_CURRENT, #expr, __expr, "must be less or equal than", __than); \
    } \
}while(0)

// whether expr is in range (INCLUSIVE) [rangeMin, rangeMax]
#define AssertInRange(expr, rangeMin, rangeMax) do { \
    const auto& __expr = expr;                       \
    const auto& __rmin = rangeMin;                   \
    const auto& __rmax = rangeMax;                   \
    if (!rpp::Compare::rngInc(__expr, __rmin, __rmax)) { \
        rpp::test::assumption_failed(RPP_SOURCE_LOC_CURRENT, #expr, __expr, "must be within inclusive range ["#rangeMin","#rangeMax"]", __rmin, __rmax); \
    } \
}while(0)

// whether expr is in range (EXCLUSIVE) (rangeMin, rangeMax)
#define AssertExRange(expr, rangeMin, rangeMax) do { \
    const auto& __expr = expr;                       \
    const auto& __rmin = rangeMin;                   \
    const auto& __rmax = rangeMax;                   \
    if (!rpp::Compare::rngEx(__expr, __rmin, __rmax)) { \
        rpp::test::assumption_failed(RPP_SOURCE_LOC_CURRENT, #expr, __expr, "must be within exclusive range ("#rangeMin","#rangeMax")", __rmin, __rmax); \
    } \
}while(0)


// compatibility with Google Test style
#ifndef EXPECT_EQ
#define EXPECT_EQ(a, b) AssertEqual(a, b)
#define EXPECT_NE(a, b) AssertNotEqual(a, b)
#define EXPECT_LT(a, b) AssertLess(a, b)
#define EXPECT_LE(a, b) AssertLessOrEqual(a, b)
#define EXPECT_GT(a, b) AssertGreater(a, b)
#define EXPECT_GE(a, b) AssertGreaterOrEqual(a, b)
#define EXPECT_TRUE(a)  AssertTrue(a)
#define EXPECT_FALSE(a) AssertFalse(a)
#define EXPECT_THROW(a, b) AssertThrows(a, b)
#define EXPECT_NO_THROW(a) AssertNoThrowAny(a)
#endif


#define TestImpl(testclass) struct testclass : public rpp::test

#define __TestInit(testclass, autorun)                                \
    explicit testclass(rpp::strview name) : rpp::test{name} {}        \
    static std::unique_ptr<test> __create(rpp::strview name)          \
    { return std::unique_ptr<test>{ new testclass{name} }; }          \
    inline static bool __registered = [] {                            \
        rpp::register_test(#testclass, &__create, autorun);           \
        return true;                                                  \
    }();                                                              \
    using ClassType = testclass;                                      \
    ClassType* self() { return this; }                                \
    void init_test() override

// called once per TestImpl class init
#define TestInit(testclass)          __TestInit(testclass, true)
#define TestInitNoAutorun(testclass) __TestInit(testclass, false)

// called once per TestImpl class run to cleanup
#define TestCleanup() void cleanup_test() override

// called once for each test func
#define TestCaseSetup() void test_case_setup() override
// called once after each test func to clean up
#define TestCaseCleanup() void test_case_cleanup() override

#if _MSC_VER
#  define __TestLambda(testname) [&](){ this->test_##testname(); }
#else
#  define __TestLambda(testname) [&](){ self()->test_##testname(); }
#endif

#define TestCase(testname) \
    const int _test_##testname = add_test_func(self(), #testname, &ClassType::test_##testname ); \
    void test_##testname()

#if RPP_HAS_COROUTINES
// allows to use co_await in test cases, driven synchronously by the test runner
#define TestCaseCoro(testname) \
    const int _test_##testname = add_coro_test_func(self(), #testname, &ClassType::test_##testname ); \
    rpp::test::test_coro test_##testname()
#endif // RPP_HAS_COROUTINES

#define TestCaseExpectedEx(testname, expectedExceptionType) \
    const int _test_##testname = add_test_func(self(), #testname, &ClassType::test_##testname, &typeid(expectedExceptionType)); \
    void test_##testname()

#define TestCaseNoAutorun(testname) \
    const int _test_##testname = add_test_func(self(), #testname, &ClassType::test_##testname, nullptr, false); \
    void test_##testname()
