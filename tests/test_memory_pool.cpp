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

        pool.allocate(57);
        pool.allocate(17);
        pool.allocate(45);
        pool.allocate(33);

        AssertThat(obj->Name, "TestObject");
        AssertThat(obj->Value, 10.0f);
    }
};
