#include <rpp/memory_pool.h>
#include <rpp/tests.h>
using namespace std::literals;

struct TestObject
{
    std::string Name = "DefaultName";
    float Value = 2.0f;
};

TestImpl(memory_pool)
{
    TestInit(memory_pool)
    {
    }

    TestCase(linear_static_pool)
    {
        const int align = 8;
        rpp::linear_static_pool pool { 36 };
        AssertThat(pool.available(), 36);

        AssertNotEqual(pool.allocate(16, align), nullptr);
        AssertThat(pool.available(), 20);
        AssertNotEqual(pool.allocate(16, align), nullptr);
        AssertThat(pool.available(), 4);

        // try allocate 8 with only 4 available
        AssertThat(pool.allocate(8, align), nullptr);
        AssertThat(pool.available(), 4);

        AssertNotEqual(pool.allocate(4, align), nullptr);
        AssertThat(pool.available(), 0);

        AssertThat(pool.allocate(8, align), nullptr);
    }

    TestCase(aligned_allocation)
    {
        const int align = 16;
        rpp::linear_static_pool pool { 64 };
        AssertThat(pool.available(), 64);

        AssertThat(size_t(pool.allocate(14, align)) % align, 0ul);
        AssertThat(pool.available(), 64-14); // first alloc is always aligned to 16

        AssertThat(size_t(pool.allocate(5, align)) % align, 0ul);
        AssertThat(pool.available(), 48-5);

        AssertThat(size_t(pool.allocate(16, align)) % align, 0ul);
        AssertThat(pool.available(), 32-16);

        AssertThat(size_t(pool.allocate(12, align)) % align, 0ul);
        AssertThat(pool.available(), 16-12);
    }

    TestCase(linear_dynamic_pool)
    {
        const int align = 8;
        rpp::linear_dynamic_pool pool { 32, 1.0f };
        AssertThat(pool.available(), 32);

        AssertNotEqual(pool.allocate(16, align), nullptr);
        AssertThat(pool.available(), 16);
        AssertNotEqual(pool.allocate(16, align), nullptr);
        AssertThat(pool.available(), 0);

        // now test if it can reallocate
        AssertNotEqual(pool.allocate(16, align), nullptr);
        AssertThat(pool.available(), 16);
        AssertNotEqual(pool.allocate(8, align), nullptr);
        AssertThat(pool.available(), 8);

        // try to get it perfectly empty
        AssertNotEqual(pool.allocate(8, align), nullptr);
        AssertThat(pool.available(), 0);

        // try to allocate way too much
        AssertThat(pool.allocate(64, align), nullptr);
        AssertThat(pool.available(), 0);
    }

    TestCase(object_construct)
    {
        rpp::linear_static_pool pool { 1024 };

        TestObject* deflt = pool.construct<TestObject>();
        AssertThat(deflt->Name, "DefaultName");
        AssertThat(deflt->Value, 2.0f);


        TestObject* init = pool.construct<TestObject>("TestObject"s, 10.0f);
        AssertThat(init->Name, "TestObject");
        AssertThat(init->Value, 10.0f);
    }

    TestCase(object_construct_pool_grow)
    {
        rpp::linear_dynamic_pool pool { sizeof(TestObject) };

        TestObject* obj = pool.construct<TestObject>("TestObject"s, 10.0f);
        AssertThat(obj->Name, "TestObject");
        AssertThat(obj->Value, 10.0f);

        (void)pool.allocate(57);
        (void)pool.allocate(17);
        (void)pool.allocate(45);
        (void)pool.allocate(33);

        AssertThat(obj->Name, "TestObject");
        AssertThat(obj->Value, 10.0f);
    }

    TestCase(allocate_array)
    {
        rpp::linear_static_pool pool { 16 };

        float* floats = pool.allocate_array<float>(5);
        AssertThat(floats, nullptr);

        floats = pool.allocate_array<float>(2);
        AssertNotEqual(floats, nullptr);

        floats = pool.allocate_array<float>(2);
        AssertNotEqual(floats, nullptr);

        floats = pool.allocate_array<float>(2);
        AssertThat(floats, nullptr);
    }

    TestCase(construct_array_pod)
    {
        rpp::linear_dynamic_pool pool { 1024 };

        float* floats = pool.construct_array<float>(10, 42.0f);
        for (int i = 0; i < 10; ++i)
            AssertThat(floats[i], 42.0f);
        
        AssertNotEqual(floats[-1], 42.0f); // check out of range
        AssertNotEqual(floats[10], 42.0f);
    }

    TestCase(construct_array_nonpod)
    {
        rpp::linear_dynamic_pool pool { 1024 };

        // this works thanks to C++11 Small String Optimization
        std::string* strings = pool.construct_array<std::string>(10, "hello");
        for (int i = 0; i < 10; ++i)
            AssertThat(strings[i], "hello");
    }
};
