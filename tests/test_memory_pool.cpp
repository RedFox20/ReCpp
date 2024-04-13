#include <rpp/memory_pool.h>
#include <rpp/proc_utils.h>
#include <rpp/scope_guard.h>
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

        float* floatsPrev = pool.construct_array<float>(5, 11.0f);
        float* floats = pool.construct_array<float>(10, 42.0f);
        float* floatsAfter = pool.construct_array<float>(5, 33.0f);

        for (int i = 0; i < 5; ++i)
            AssertThat(floatsPrev[i], 11.0f);
        for (int i = 0; i < 10; ++i)
            AssertThat(floats[i], 42.0f);
        for (int i = 0; i < 5; ++i)
            AssertThat(floatsAfter[i], 33.0f);

        // check out of range entries before and after
        AssertEqual(floats[-1], 11.0f);
        AssertEqual(floats[10], 33.0f);
    }

    TestCase(construct_array_nonpod)
    {
        rpp::linear_dynamic_pool pool { 1024 };

        // this works thanks to C++11 Small String Optimization
        std::string* strings = pool.construct_array<std::string>(10, "hello");
        for (int i = 0; i < 10; ++i)
            AssertThat(strings[i], "hello");
    }

    TestCase(proc_mem_usage_works)
    {
        rpp::proc_mem_info info = rpp::proc_current_mem_used();
        AssertGreater(info.virtual_size, 0ull);
        AssertGreater(info.physical_mem, 0ull);

        print_info("#1 Virtual Size: %llu KB\n", info.virtual_size / 1000);
        print_info("#1 Physical Mem: %llu KB\n", info.physical_mem / 1000);


        // allocate enough bytes to cause virtual size to increase
        // TODO: may have to use OS level allocation functions to ensure this works
        size_t num_bytes = 50 * 1000 * 1000;
        print_info("-- Allocating %llu KB --\n", num_bytes / 1000);

        char* mem = (char*)malloc(num_bytes);
        scope_guard([mem] { free(mem); });

        for (size_t i = 0; i < num_bytes; ++i)
            mem[i] = 1; // touch every page to commit the memory

        rpp::proc_mem_info info2 = rpp::proc_current_mem_used();

        print_info("#2 Virtual Size: %llu KB\n", info2.virtual_size / 1000);
        print_info("#2 Physical Mem: %llu KB\n", info2.physical_mem / 1000);

        // this doesn't work on CircleCI for some reason :shrug:
        if (getenv("CIRCLECI"))
            return;

        AssertGreater(info2.virtual_size, info.virtual_size);
        AssertGreater(info2.physical_mem, info.physical_mem);

        // ensure it's within reasonable range
        AssertLess(info2.virtual_size - info.virtual_size, size_t(num_bytes*1.1));
        AssertLess(info2.physical_mem - info.physical_mem, size_t(num_bytes*1.1));
    }
};
