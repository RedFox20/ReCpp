#include <rpp/tests.h>
#include <vector>
#include <string>
#include <rpp/sort.h>
#include <rpp/sprint.h>

// return -1 if sorted, else index where sorting breaks
template<class T> int first_unsorted_index(const T* arr, int count)
{
    for (int i = 1; i < count; ++i)
        if (arr[i - 1] > arr[i]) /* prev > next */
            return i;
    return -1;
}
template<class T> bool is_sorted(const T* arr, int count)
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
        AssertFalse(std::is_sorted(array.begin(), array.end()));

        rpp::insertion_sort(array.data(), array.size(), [](auto& a, auto& b) { return a < b; });
        AssertEqual(first_unsorted_index(array.data(), array.size()), -1);
        AssertTrue(std::is_sorted(array.begin(), array.end()));

        if (!is_sorted(array.data(), array.size()))
        {
            print_error("UNSORTED array after insertion_sort: %s\n", rpp::sprint(array).c_str());
        }
    }

    TestCase(insertion_sort_string)
    {
        std::vector<std::string> array = array_random_str(32);
        AssertFalse(is_sorted(array.data(), array.size()));
        AssertFalse(std::is_sorted(array.begin(), array.end()));

        rpp::insertion_sort(array.data(), array.size(), [](auto& a, auto& b) { return a < b; });
        AssertEqual(first_unsorted_index(array.data(), array.size()), -1);
        AssertTrue(std::is_sorted(array.begin(), array.end()));

        if (!is_sorted(array.data(), array.size()))
        {
            print_error("UNSORTED array after insertion_sort: %s\n", rpp::sprint(array).c_str());
        }
    }
};
