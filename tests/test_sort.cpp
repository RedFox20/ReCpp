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
};
