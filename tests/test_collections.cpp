#include <rpp/collections.h>
#include <rpp/stack_trace.h>
#include <rpp/tests.h>
using namespace rpp;
using std::unordered_map;

TestImpl(test_collections)
{
    TestInit(test_collections)
    {
    }

    TestCase(element_range)
    {
        std::vector<int> v = { 1, 1, 1, 1, 1 };

        int n = 0;
        for (auto& i : range(v)) n += i;
        AssertThat(n, 5);

        int m = 0;
        for (auto& i : range(v, 4)) m += i;
        AssertThat(m, 4);

        int k = 0;
        for (auto& i : range(v.data(), v.data()+5)) k += i;
        AssertThat(k, 5);
    }

    TestCase(const_element_range)
    {
        const std::vector<int> v = { 1, 1, 1, 1, 1 };

        int n = 0;
        for (auto& i : range(v)) n += i;
        AssertThat(n, 5);

        int m = 0;
        for (auto& i : range(v, 4)) m += i;
        AssertThat(m, 4);

        int k = 0;
        for (auto& i : range(v.data(), v.data()+5)) k += i;
        AssertThat(k, 5);
    }

    TestCase(rvalue_element_range)
    {
        auto sumRange = [](element_range<const int> range) {
            int n = 0;
            for (auto& i : range) n += i;
            return n;
        };
        int n = sumRange(range(std::vector<int>{ 1, 1, 1, 1, 1 }));
        AssertThat(n, 5);
    }

    struct StringCollection
    {
        std::vector<std::string> items;

        std::string* begin() { return items.data(); }
        std::string* end()   { return items.data() + items.size(); }
    };

    TestCase(implicit_range_from_iterable)
    {
        StringCollection collection { { "a", "b", "c", "d" } };
        element_range<std::string> stringRange = collection;

        AssertThat(stringRange.size(), (int)collection.items.size());
        AssertThat(stringRange[0], "a");
    }

    TestCase(explicit_range_from_iterable)
    {
        StringCollection collection { { "a", "b", "c", "d" } };
        element_range<std::string> stringRange = rpp::range(collection);

        AssertThat(stringRange.size(), (int)collection.items.size());
        AssertThat(stringRange[0], "a");
    }

    TestCase(index_range)
    {
        int n = 0;
        for (int i : range(6)) n += i;
        AssertThat(n, 15); // 0 + 1 + 2 + 3 + 4 + 5

        int m = 0;
        for (int i : range(1, 6)) m += i;
        AssertThat(m, 15); // 1 + 2 + 3 + 4 + 5

        int k = 0;
        for (int i : range(5, 0, -1)) k += i;
        AssertThat(k, 15); // 5 + 4 + 3 + 2 + 1
    }

    TestCase(pop_back)
    {
        std::vector<std::string> v { "first"s, "second"s };
        AssertThat(pop_back(v), "second"s);
        AssertThat(pop_back(v), "first"s);
    }

    TestCase(push_unique)
    {
        std::vector<std::string> v { "1"s, "2"s, "3"s, "4"s, "5"s };

        push_unique(v, "5"s);
        AssertThat(v.size(), 5ul);

        push_unique(v, "6"s);
        AssertThat(v.size(), 6ul);
    }

    TestCase(erase_item)
    {
        std::vector<std::string> v { "1"s, "2"s, "3"s, "4"s, "5"s };

        erase_item(v, "nonexisting"s);
        AssertThat(v.size(), 5ul);

        erase_item(v, "5");
        AssertThat(v.size(), 4ul);

        erase_item(v, "3"s);
        AssertThat(v.size(), 3ul);

        erase_item(v, "1");
        AssertThat(v.size(), 2ul);
    }

    TestCase(erase_first_if)
    {
        std::vector<std::string> v { "1"s, "2"s, "2"s, "4"s, "5"s };

        erase_first_if(v, [](auto& s){ return s == "nonexisting"s; });
        AssertThat(v.size(), 5ul);

        erase_first_if(v, [](auto& s){ return s == "1"s; });
        AssertThat(v, std::vector<std::string>({"2", "2", "4", "5"}));

        erase_first_if(v, [](auto& s){ return s == "2"s; });
        AssertThat(v, std::vector<std::string>({"2", "4", "5"}));

        erase_first_if(v, [](auto& s){ return s == "5"s; });
        AssertThat(v, std::vector<std::string>({"2", "4"}));

        erase_first_if(v, [](auto& s){ return s == "2"s; });
        AssertThat(v, std::vector<std::string>({"4"s}));
    }

    TestCase(erase_if)
    {
        std::vector<std::string> v { "1"s, "2"s, "3"s, "2"s, "1"s, "2"s };

        erase_if(v, [](auto& s){ return s == "nonexisting"s; });
        AssertThat(v.size(), 6ul);

        erase_if(v, [](auto& s){ return s == "2"s; });
        AssertThat(v, std::vector<std::string>({"1", "3", "1"}));

        erase_if(v, [](auto& s){ return s == "3"s; });
        AssertThat(v, std::vector<std::string>({"1", "1"}));
        
        erase_if(v, [](auto& s){ return s == "1"s; });
        AssertThat(v, std::vector<std::string>());
    }

    TestCase(erase_back_swap)
    {
        std::vector<std::string> v { "1"s, "2"s, "3"s, "4"s, "5"s };

        erase_back_swap(v, 4);
        AssertThat(v.size(), 4ul);

        erase_back_swap(v, 2);
        AssertThat(v.size(), 3ul);

        erase_back_swap(v, 0);
        AssertThat(v.size(), 2ul);
    }

    TestCase(erase_item_back_swap)
    {
        std::vector<std::string> v { "1"s, "2"s, "3"s, "4"s, "5"s };

        erase_item_back_swap(v, "nonexisting"s);
        AssertThat(v.size(), 5ul);

        erase_item_back_swap(v, "5"s);
        AssertThat(v.size(), 4ul);

        erase_item_back_swap(v, "3"s);
        AssertThat(v.size(), 3ul);

        erase_item_back_swap(v, "1"s);
        AssertThat(v.size(), 2ul);
    }

    TestCase(erase_back_swap_first_if)
    {
        std::vector<std::string> v { "1"s, "2"s, "3"s, "4"s, "5"s };

        erase_back_swap_first_if(v, [](auto& s) { return s == "nonexisting"; });
        AssertThat(v.size(), 5ul);

        erase_back_swap_first_if(v, [](auto& s) { return s == "5"; });
        AssertThat(v.size(), 4ul);

        erase_back_swap_first_if(v, [](auto& s) { return s == "3"; });
        AssertThat(v.size(), 3ul);
        
        erase_back_swap_first_if(v, [](auto& s) { return s == "1"; });
        AssertThat(v.size(), 2ul);
    }
    
    TestCase(erase_back_swap_all_if)
    {
        std::vector<std::string> v { "1"s, "1"s, "2"s, "3"s, "3"s, "4"s, "5"s, "5"s };

        erase_back_swap_all_if(v, [](auto& s) { return s == "nonexisting"; });
        AssertThat(v.size(), 8ul);

        erase_back_swap_all_if(v, [](auto& s) { return s == "5"; });
        AssertThat(v.size(), 6ul);

        erase_back_swap_all_if(v, [](auto& s) { return s == "3"; });
        AssertThat(v.size(), 4ul);
        
        erase_back_swap_all_if(v, [](auto& s) { return s == "1"; });
        AssertThat(v.size(), 2ul);
    }

    TestCase(vector_contains)
    {
        std::vector<std::string> v { "1"s, "2"s, "3"s };

        AssertThat(contains(v, "1337"s), false);
        AssertThat(contains(v, ""s),     false);
        
        AssertThat(contains(v, "1337"), false);
        AssertThat(contains(v, ""),     false);

        AssertThat(contains(v, "1"s), true);
        AssertThat(contains(v, "2"s), true);
        AssertThat(contains(v, "3"s), true);
        
        AssertThat(contains(v, "1"), true);
        AssertThat(contains(v, "2"), true);
        AssertThat(contains(v, "3"), true);
    }

    TestCase(unordered_map_contains)
    {
        std::unordered_map<std::string,std::string> v { 
            {"1","x1"},
            {"2","x2"}, 
            {"3","x3"}
        };

        AssertThat(contains(v, "1337"s), false);
        AssertThat(contains(v, ""s),     false);
        
        AssertThat(contains(v, "1337"), false);
        AssertThat(contains(v, ""),     false);

        AssertThat(contains(v, "1"s), true);
        AssertThat(contains(v, "2"s), true);
        AssertThat(contains(v, "3"s), true);
        
        AssertThat(contains(v, "1"), true);
        AssertThat(contains(v, "2"), true);
        AssertThat(contains(v, "3"), true);
    }

    TestCase(vector_append)
    {
        std::vector<std::string> a { "1"s, "2"s };
        std::vector<std::string> b { "3"s, "4"s, "5"s };

        append(a, b);
        AssertThat(a.size(), 5ul);
        AssertThat(a[0], "1");
        AssertThat(a[1], "2");
        AssertThat(a[2], "3");
        AssertThat(a[3], "4");
        AssertThat(a[4], "5");
    }

    TestCase(vector_find)
    {
        std::vector<std::string> v { "1"s, "2"s, "3"s };

        AssertThat(find(v, "1337"s), nullptr);
        AssertNotEqual(find(v, "1"s), nullptr);
        AssertNotEqual(find(v, "2"s), nullptr);
        AssertNotEqual(find(v, "3"s), nullptr);

        AssertEqual(*find(v, "1"s), "1"s);
        AssertEqual(*find(v, "2"s), "2"s);
        AssertEqual(*find(v, "3"s), "3"s);
    }

    TestCase(unordered_map_find)
    {
        std::unordered_map<std::string,std::string> v { 
            {"1","x1"},
            {"2","x2"}, 
            {"3","x3"}
        };

        AssertThat(find(v, "1337"s), nullptr);
        AssertNotEqual(find(v, "1"s), nullptr);
        AssertNotEqual(find(v, "2"s), nullptr);
        AssertNotEqual(find(v, "3"s), nullptr);

        AssertEqual(*find(v, "1"s), "x1"s);
        AssertEqual(*find(v, "2"s), "x2"s);
        AssertEqual(*find(v, "3"s), "x3"s);
    }

    TestCase(vector_find_if)
    {
        std::vector<std::string> v { "1"s, "2"s, "1"s, "3"s, "2"s };

        AssertThat(find_if(range(v), [](const std::string& s) { return s == "x"s; }), nullptr);
        AssertThat(find_if(range(v), [](const std::string& s) { return s == "1"s; }), &v[0]  );
        AssertThat(find_if(range(v), [](const std::string& s) { return s == "2"s; }), &v[1]  );
    }

    TestCase(vector_find_last_if)
    {
        std::vector<std::string> v { "1"s, "2"s, "1"s, "3"s, "2"s };

        AssertThat(find_if(range(v), [](const std::string& s) { return s == "x"s; }), nullptr);
        AssertThat(find_if(range(v), [](const std::string& s) { return s == "1"s; }), &v[0]  );
        AssertThat(find_if(range(v), [](const std::string& s) { return s == "2"s; }), &v[1]  );
    }

    TestCase(vector_find_smallest)
    {
        std::vector<std::string> v { "100"s, "50"s, "25"s, "5"s, "2"s };
        AssertThat(find_smallest(v, [](const std::string& s) { return s.size(); }), &v[3]);
    }

    TestCase(vector_find_largest)
    {
        std::vector<std::string> v { "100"s, "50"s, "25"s, "5"s, "2"s };
        AssertThat(find_largest(v, [](const std::string& s) { return s.size(); }), &v[0]);
    }

    TestCase(any_of)
    {
        std::vector<std::string> empty;
        AssertThat(any_of(empty, [](auto s){ (void)s; return true; }), false);

        std::vector<std::string> v { "a"s, "bb"s, "ccc"s, "dddd"s };
        AssertThat(any_of(v, [](auto s){ return s == "xxx"s; }), false);
        AssertThat(any_of(v, [](auto s){ return s == "ccc"s; }), true);
    }

    TestCase(sum_all)
    {
        std::vector<std::string> v { "a"s, "bb"s, "ccc"s, "dddd"s };

        AssertThat(sum_all(v), "abbcccdddd"s);

        // we cannot grab address of C++ stdlib methods, so we need a dummy wrapper
        struct custom : public std::string {
            using std::string::string;
            custom(std::string&& s) : std::string(std::move(s)) {}
            int len() const { return (int)this->length(); }
        };
        std::vector<custom> u { "a"s, "bb"s, "ccc"s, "dddd"s };
        AssertThat(sum_all(u, &custom::len), 10);
    }

    static int string_to_int(const std::string& s)
    {
        return std::stoi(s);
    }

    TestCase(transform)
    {
        std::vector<std::string> original { "1"s, "2"s, "3"s, "4"s, "5"s };
        std::vector<int>    expected { 1,2,3,4,5 };

        std::vector<int> transformed1 = rpp::transform(original, &string_to_int);
        AssertThat(transformed1, expected);

        std::vector<int> transformed2 = rpp::transform(original, [](const std::string& s) -> int
        {
            return std::stoi(s);
        });
        AssertThat(transformed2, expected);
    }

    //TestCase(reduce)
    //{
    //    vector<string> v { "1"s, "2"s, "3"s, "4"s, "5"s };
    //    int reduced = rpp::reduce(v, [](const string& a, const string& b)
    //    {
    //       return std::stoi(a) + std::stoi(b); 
    //    });
    //    AssertThat(reduced, 15);
    //}

};
