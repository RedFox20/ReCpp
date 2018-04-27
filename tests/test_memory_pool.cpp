#include <rpp/memory_pool.h>
#include <rpp/tests.h>

TestImpl(memory_pool)
{
    TestInit(memory_pool)
    {
    }

    TestCase(linear_static_pool)
    {
        rpp::linear_static_pool pool { 36 };
        AssertThat(pool.available(), 36);

        AssertNotEqual(pool.allocate(16), nullptr);
        AssertThat(pool.available(), 20);
        AssertNotEqual(pool.allocate(16), nullptr);
        AssertThat(pool.available(), 4);

        // try allocate 8 with only 4 available
        AssertThat(pool.allocate(8), nullptr);
        AssertThat(pool.available(), 4);

        AssertNotEqual(pool.allocate(4), nullptr);
        AssertThat(pool.available(), 0);

        AssertThat(pool.allocate(8), nullptr);
    }

    TestCase(linear_dynamic_pool)
    {
        rpp::linear_dynamic_pool pool { 32, 1.0f };
        AssertThat(pool.available(), 32);

        AssertNotEqual(pool.allocate(16), nullptr);
        AssertThat(pool.available(), 16);
        AssertNotEqual(pool.allocate(16), nullptr);
        AssertThat(pool.available(), 0);

        // now test if it can reallocate
        AssertNotEqual(pool.allocate(16), nullptr);
        AssertThat(pool.available(), 16);
        AssertNotEqual(pool.allocate(8), nullptr);
        AssertThat(pool.available(), 8);

        // try to get it perfectly empty
        AssertNotEqual(pool.allocate(8), nullptr);
        AssertThat(pool.available(), 0);

        // try to allocate way too much
        AssertThat(pool.allocate(64), nullptr);
        AssertThat(pool.available(), 0);
    }
};
