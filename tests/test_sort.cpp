#include <rpp/tests.h>
#include <vector>
#include <string>
#include <rpp/sort.h>
#include <rpp/sprint.h>

// return -1 if sorted, else index where sorting breaks
template<class T> int first_unsorted_index(const T* arr, size_t count)
{
    for (size_t i = 1; i < count; ++i)
        if (arr[i - 1] > arr[i]) /* prev > next */
            return (int)i;
    return -1;
}
template<class T> bool is_sorted(const T* arr, size_t count)
{
    return first_unsorted_index(arr, count) == -1;
}

std::string random_string(int maxlen)
{
    std::string str;
    int len = abs(rand()) % maxlen;
    for (int i = 0; i < len; ++i)
        str.push_back((char)(abs(rand()) % 26 + 'a')) ;
    return str;
}

std::vector<int> array_random_int(int count)
{
    srand(104729);
    std::vector<int> arr(count);
    for (int i = 0; i < count; ++i)
        arr[i] = abs(rand()) % 0xFFFF;
    return arr;
}

std::vector<std::string> array_random_str(int count)
{
    srand(104729);
    std::vector<std::string> arr(count);
    for (int i = 0; i < count; ++i)
        arr[i] = random_string(32);
    return arr;
}

std::vector<int> array_reverse(int count)
{
    std::vector<int> arr(count);
    int n = count;
    for (int i = 0; i < count; ++i)
        arr[i] = --n; // [4 3 2 1 0]
    return arr;
}

TestImpl(test_sort)
{
    TestInit(test_sort)
    {
    }

    TestCase(insertion_sort_int)
    {
        std::vector<int> array = array_random_int(32);
        AssertFalse(is_sorted(array.data(), array.size()));

        rpp::insertion_sort(array.data(), array.size(), [](auto& a, auto& b) { return a < b; });
        AssertEqual(first_unsorted_index(array.data(), array.size()), -1);

        if (!is_sorted(array.data(), array.size()))
        {
            print_error("UNSORTED array after insertion_sort: %s\n", rpp::sprint(array).c_str());
        }
    }

    TestCase(insertion_sort_string)
    {
        std::vector<std::string> array = array_random_str(32);
        AssertFalse(is_sorted(array.data(), array.size()));

        rpp::insertion_sort(array.data(), array.size(), [](auto& a, auto& b) { return a < b; });
        AssertEqual(first_unsorted_index(array.data(), array.size()), -1);

        if (!is_sorted(array.data(), array.size()))
        {
            print_error("UNSORTED array after insertion_sort: %s\n", rpp::sprint(array).c_str());
        }
    }

    TestCase(insertion_sort_custom_sort_rule)
    {
        struct ipaddrinfo {
            std::string addr;
            std::string gateway;
        };

        std::vector<ipaddrinfo> array {
            { "192.168.1.102", "" },
            { "192.168.1.101", "" },
            { "192.168.1.110", "192.168.1.1" },
        };

        // first should be gateways
        // then by regular address
        rpp::insertion_sort(array.data(), array.size(), [](const ipaddrinfo& a, const ipaddrinfo& b)
        {
            if (!a.gateway.empty() && b.gateway.empty()) return true;
            if (!b.gateway.empty() && a.gateway.empty()) return false;
            return a.addr < b.addr;
        });

        AssertEqual(array[0].addr, "192.168.1.110"); // first because it has a gateway
        AssertEqual(array[1].addr, "192.168.1.101");
        AssertEqual(array[2].addr, "192.168.1.102");
    }

    TestCase(insertion_sort_no_swaps_on_equal)
    {
        // track the number of comparisons and swaps to detect redundant work on equal elements
        struct counted_int {
            int value;
            int* swap_count;
            counted_int() : value{0}, swap_count{nullptr} {}
            counted_int(int v, int* sc) : value{v}, swap_count{sc} {}
            counted_int(const counted_int&) = default;
            counted_int& operator=(const counted_int&) = default;
            counted_int(counted_int&& o) noexcept : value{o.value}, swap_count{o.swap_count} {
                if (swap_count) ++(*swap_count);
            }
            counted_int& operator=(counted_int&& o) noexcept {
                value = o.value; swap_count = o.swap_count;
                if (swap_count) ++(*swap_count);
                return *this;
            }
            // for is_sorted() comparator
            bool operator>(const counted_int& other) const { return value > other.value; }
        };

        int swap_count = 0;
        // all elements are equal, so a correct implementation should perform zero swaps
        std::vector<counted_int> array;
        for (int i = 0; i < 10; ++i)
            array.push_back(counted_int{42, &swap_count});

        swap_count = 0; // reset after vector construction
        rpp::insertion_sort(array.data(), array.size(),
            [](const counted_int& a, const counted_int& b) { return a.value < b.value; });

        // with a correct < comparator, equal elements should never be swapped
        AssertEqual(swap_count, 0);
        AssertTrue(is_sorted(array.data(), array.size()));
    }

    TestCase(insertion_sort_container_overload)
    {
        std::vector<int> array = array_random_int(32);
        AssertFalse(is_sorted(array.data(), array.size()));

        rpp::insertion_sort(array, [](const int& a, const int& b) { return a < b; });
        AssertEqual(first_unsorted_index(array.data(), array.size()), -1);
    }

    TestCase(insertion_sort_container_overload_string)
    {
        std::vector<std::string> array = array_random_str(32);
        AssertFalse(is_sorted(array.data(), array.size()));

        rpp::insertion_sort(array, [](const std::string& a, const std::string& b) { return a < b; });
        AssertEqual(first_unsorted_index(array.data(), array.size()), -1);
    }

    TestCase(sort_comparison_concept)
    {
        // valid comparator: bool(const T&, const T&)
        static_assert(rpp::sort_comparison<int, decltype([](const int& a, const int& b) { return a < b; })>);

        // valid: comparator returning int (convertible to bool)
        static_assert(rpp::sort_comparison<int, decltype([](const int& a, const int& b) -> int { return a - b; })>);

        // invalid: comparator with wrong parameter types should not match
        static_assert(!rpp::sort_comparison<int, decltype([](const std::string& a, const std::string& b) { return a < b; })>);

        // invalid: comparator returning void
        static_assert(!rpp::sort_comparison<int, decltype([](const int&, const int&) -> void { })>);
    }

    TestCase(sortable_container_concept)
    {
        static_assert(rpp::sortable_container<std::vector<int>>);
        static_assert(rpp::sortable_container<std::vector<std::string>>);

        // a type without .data()/.size()/operator[] should not satisfy the concept
        struct not_a_container { int x; };
        static_assert(!rpp::sortable_container<not_a_container>);
    }

    TestCase(insertion_sort_custom_container)
    {
        // custom container with T* data(), int size() const, operator[](int) const
        struct int_container {
            std::vector<int> v;
            int* data() { return v.data(); }
            const int* data() const { return v.data(); }
            int size() const { return (int)v.size(); }
            const int& operator[](int index) const { return v[index]; }
            int& operator[](int index) { return v[index]; }
        };

        static_assert(rpp::sortable_container<int_container>);

        int_container c;
        c.v = array_random_int(32);
        AssertFalse(is_sorted(c.data(), (size_t)c.size()));

        rpp::insertion_sort(c, [](const int& a, const int& b) { return a < b; });
        AssertEqual(first_unsorted_index(c.data(), (size_t)c.size()), -1);
    }
};
