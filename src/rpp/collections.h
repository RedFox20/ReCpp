#pragma once
/**
 * Basic Collections and Range extensions, Copyright (c) 2017-2018, Jorma Rebane
 * Distributed under MIT Software License
 */
#include <array>
#include <vector>
#include <deque>
#include <unordered_map>
#include <numeric>
#include "sort.h"
#include <stdexcept>

namespace rpp
{
    ////////////////////////////////////////////////////////////////////////////////////

    template<class T> struct element_range
    {
        T* first;
        T* sentinel;
        element_range() noexcept : first{nullptr}, sentinel{nullptr} {}
        element_range(T* first, T* sentinel) noexcept : first{first}, sentinel{sentinel} {}
        element_range(T* ptr, int n)         noexcept : first{ptr},   sentinel{ptr+n}    {}
        element_range(T* ptr, size_t n)      noexcept : first{ptr},   sentinel{ptr+n}    {}

        element_range(std::vector<T>& v) noexcept : first{v.data()}, sentinel{v.data()+v.size()} {}

        template<class Container>
        element_range(Container& c) noexcept : first{&*c.begin()}, sentinel{&*c.end()} {}

        T* begin() noexcept { return first;    }
        T* end()   noexcept { return sentinel; }
        const T* begin() const noexcept { return first;    }
        const T* end()   const noexcept { return sentinel; }
        bool empty() const noexcept { return first == sentinel; }
        size_t size() const noexcept { return size_t(sentinel-first); }
              T& operator[](int index)       noexcept { return first[index]; }
        const T& operator[](int index) const noexcept { return first[index]; }
              T* data()       noexcept { return first; }
        const T* data() const noexcept { return first; }

        std::vector<T> to_vec() const noexcept { return {first, sentinel}; }
    };

    template<class T> struct element_range<const T>
    {
        const T* first;
        const T* sentinel;
        element_range() noexcept : first{nullptr}, sentinel{nullptr} {}
        element_range(const T* first, const T* sentinel) noexcept : first{first}, sentinel{sentinel} {}
        element_range(const T* ptr, int n)               noexcept : first{ptr},   sentinel{ptr+n}    {}
        element_range(const T* ptr, size_t n)            noexcept : first{ptr},   sentinel{ptr+n}    {}

        element_range(const std::vector<T>& v) noexcept : first{v.data()}, sentinel{v.data()+v.size()} {}

        template<class Container>
        element_range(const Container& c) noexcept : first{&*c.begin()}, sentinel{&*c.end()} {}

        const T* begin() const noexcept { return first;    }
        const T* end()   const noexcept { return sentinel; }
        bool empty() const noexcept { return first == sentinel; }
        size_t size() const noexcept { return size_t(sentinel-first); }
        const T& operator[](int index) const noexcept { return first[index]; }
        const T* data() const noexcept { return first; }

        std::vector<T> to_vec() const noexcept { return {first, sentinel}; }
    };

    template<class T>           element_range<T> range(T* data, T* end)      noexcept { return { data, end  }; }
    template<class T>           element_range<T> range(T* data, int size)    noexcept { return { data, size }; }
    template<class T>           element_range<T> range(T* data, size_t size) noexcept { return { data, size }; }
    template<class T, size_t N> element_range<T> range(std::array<T, N>&  v) noexcept { return { v.data(), v.size() }; }
    template<class T, class A>  element_range<T> range(std::vector<T, A>& v) noexcept { return { v.data(), v.size() }; }
    template<class T, size_t N> element_range<T> range(std::array<T, N>&  v, size_t n) noexcept { return { v.data(), n }; }
    template<class T, class A>  element_range<T> range(std::vector<T, A>& v, size_t n) noexcept { return { v.data(), n }; }

    template<class T>           element_range<const T> range(const T* data, const T* end)noexcept { return { data, end  }; }
    template<class T>           element_range<const T> range(const T* data, int size)    noexcept { return { data, size }; }
    template<class T>           element_range<const T> range(const T* data, size_t size) noexcept { return { data, size }; }
    template<class T, size_t N> element_range<const T> range(const std::array<T, N>&  v) noexcept { return { v.data(), v.size() }; }
    template<class T, class A>  element_range<const T> range(const std::vector<T, A>& v) noexcept { return { v.data(), v.size() }; }
    template<class T, size_t N> element_range<const T> range(const std::array<T, N>&  v, size_t n) noexcept { return { v.data(), n }; }
    template<class T, class A>  element_range<const T> range(const std::vector<T, A>& v, size_t n) noexcept { return { v.data(), n }; }

    template<class Container>
    using element_type_with_const = std::remove_reference_t<decltype(*std::declval<Container>().begin())>;

    template<class C>
    element_range<element_type_with_const<C>> range(C& container) noexcept
    {
        return { &*container.begin(), &*container.end() };
    }

    /**
     * @brief Create a sub-range of the given container starting at the given index
     * @param start The starting index of the range
     */
    template<class C>
    element_range<element_type_with_const<C>> slice(C& container, size_t start) noexcept
    {
        using T = element_type_with_const<C>; // element type, such as `string` or `const string`
        auto data = container.data();
        auto begin = data + start;
        auto end = data + container.size();
        return begin < end ? element_range<T>{ begin, end } : element_range<T>{};
    }

    /**
     * @brief Create a sub-range of the given container starting at the given index and with a count limit
     *        If the count exceeds the container size, the range will be clamped to the container size.
     * @param start The starting index of the range
     * @param count The maximum number of elements in the range
     */
    template<class C>
    element_range<element_type_with_const<C>> slice(C& container, size_t start, size_t count) noexcept
    {
        using T = element_type_with_const<C>; // element type, such as `string` or `const string`
        auto data = container.data();
        auto begin = data + start;
        auto max_end = data + container.size();
        auto end = (begin + count) < max_end ? (begin + count) : max_end;
        return begin < end ? element_range<T>{ begin, end } : element_range<T>{};
    }

    ////////////////////////////////////////////////////////////////////////////////////


    struct index_range
    {
        int first;
        int sentinel;
        int step;

        // create an index range [0..count)
        explicit index_range(int count) noexcept : first{0}, sentinel{count}, step{1} {}

        // generic index range [first..sentinel) with step
        index_range(int first, int sentinel, int step = 1) noexcept : first{first}, sentinel{sentinel}, step{step} {}

        struct iter
        {
            int i;
            int step;
            int operator*() const noexcept { return i; }
            iter& operator++()    noexcept { i += step; return *this; } // preincrement
            iter& operator--()    noexcept { i -= step; return *this; } // predecrement
            iter operator++(int)  noexcept { iter it{i,step}; i += step; return it; } // postincrement
            iter operator--(int)  noexcept { iter it{i,step}; i -= step; return it; } // postdecrement
            iter& operator+=(int n) noexcept { i += n*step; return *this; }
            iter& operator-=(int n) noexcept { i -= n*step; return *this; }
            bool operator==(const iter& it) const noexcept { return i == it.i; }
            bool operator!=(const iter& it) const noexcept { return i != it.i; }
        };
        iter begin() const { return {first,step};    }
        iter end()   const { return {sentinel,step}; }
    };

    inline index_range::iter operator+(const index_range::iter& it, int n) noexcept { return {it.i + it.step*n, it.step}; }
    inline index_range::iter operator+(int n, const index_range::iter& it) noexcept { return {it.i + it.step*n, it.step}; }
    inline index_range::iter operator-(const index_range::iter& it, int n) noexcept { return {it.i - it.step*n, it.step}; }
    inline index_range::iter operator-(int n, const index_range::iter& it) noexcept { return {it.i - it.step*n, it.step}; }

    inline void swap(index_range::iter& a, index_range::iter& b) noexcept {
        int i  = a.i;
        int s  = a.step;
        a.i    = b.i;
        a.step = b.step;
        b.i    = i;
        b.step = s;
    }

    // create an index range [0..count) with step=1
    inline index_range range(int count) noexcept { return index_range{ count }; }

    // generic index range [first..sentinel) with step
    inline index_range range(int first, int sentinel, int step = 1) noexcept { return index_range{ first, sentinel, step }; }


    /////////////////////////////////////////////////////////////////////////////////////

    // Pre C++17
    template<class T> T& emplace_back(std::vector<T>& v)
    {
    #if _MSC_VER >= 1910 && RPP_HAS_CXX17
        return v.emplace_back();
    #else
        v.emplace_back();
        return v.back();
    #endif
    }

    template<class T, class... Args> T& emplace_back(std::vector<T>& v, Args&&... args)
    {
    #if _MSC_VER >= 1910 && RPP_HAS_CXX17
        return v.emplace_back(std::forward<Args>(args)...);
    #else
        v.emplace_back(std::forward<Args>(args)...);
        return v.back();
    #endif
    }

    template<class T> T pop_back(std::vector<T>& v)
    {
        if (v.empty()) throw std::runtime_error{"rpp::pop_back() failed: vector is empty"};
        T item = std::move(v.back());
        v.pop_back();
        return item;
    }

    template<class T> T pop_front(std::deque<T>& d)
    {
        if (d.empty()) throw std::runtime_error{"rpp::pop_front() failed: deque is empty"};
        T item = std::move(d.front());
        d.pop_front();
        return item;
    }

    template<class T> void push_unique(std::vector<T>& v, const T& item)
    {
        for (const T& elem : v) if (elem == item) return;
        v.push_back(item);
    }
    
    // erases a SINGLE item
    template<class T, class U> void erase_item(std::vector<T>& v, const U& item)
    {
        T* data = v.data();
        for (size_t i = 0, n = v.size(); i < n; ++i) {
            if (data[i] == item) {
                v.erase(v.begin() + i);
                return;
            }
        }
    }

    template<class T, class Pred> void erase_first_if(std::vector<T>& v, const Pred& pred)
    {
        T* data = v.data();
        for (size_t i = 0, n = v.size(); i < n; ++i) {
            if (pred(data[i])) {
                v.erase(v.begin() + i);
                return;
            }
        }
    }

    // short-hand for the erase-remove-if idiom
    // erases ALL items that match the predicate
    template<class T, class Pred> void erase_if(std::vector<T>& v, const Pred& pred)
    {
        T* first = v.data();
        T* end   = first + v.size();
        for (;; ++first) {
            if (first == end)
                return; // nothing to erase
            if (pred(*first))
                break;
        }

        T* next = first;
        while (++first != end) {
            if (!pred(*first)) {
                *next = std::move(*first);
                ++next;
            }
        }

        size_t pop = (end - next);
        v.resize(v.size() - pop);
    }

    // erases item at index i by moving the last item to [i]
    // and then popping the last element
    template<class T> void erase_back_swap(std::vector<T>& v, int i)
    {
        if (i != (int)v.size() - 1)
            v[i] = std::move(v.back());
        v.pop_back();
    }
    
    // erases a SINGLE item using ERASE-BACK-SWAP idiom
    template<class T, class U> void erase_item_back_swap(std::vector<T>& v, const U& item)
    {
        T* data = v.data();
        for (size_t i = 0, n = v.size(); i < n; ++i)
        {
            if (data[i] == item)
            {
                if (i != n-1)
                    data[i] = std::move(data[n-1]); // move last item to data[i]
                v.pop_back();
                return;
            }
        }
    }
    
    // erases a SINGLE item using ERASE-BACK-SWAP idiom
    template<class T, class Pred> void erase_back_swap_first_if(std::vector<T>& v, const Pred& condition)
    {
        T* data = v.data();
        for (size_t i = 0, n = v.size(); i < n; ++i)
        {
            if (condition(data[i]))
            {
                if (i != n-1)
                    data[i] = std::move(data[n-1]); // move last item to data[i]
                v.pop_back();
                return;
            }
        }
    }

    // erases ALL matching items using ERASE-BACK-SWAP idiom
    template<class T, class Pred> void erase_back_swap_all_if(std::vector<T>& v, const Pred& condition)
    {
        T* data = v.data();
        for (size_t i = 0, n = v.size(); i < n; )
        {
            if (condition(data[i]))
            {
                if (i != n-1)
                    data[i] = std::move(data[n-1]); // move last item to data[i]
                v.pop_back();
                --n;
            }
            else ++i;
        }
    }

    template<class T> bool contains(const std::vector<T>& v, const T& item)
    {
        for (const T& elem : v) if (elem == item) return true;
        return false;
    }

    template<class T, class U> bool contains(const std::vector<T>& v, const U& item)
    {
        for (const T& elem : v) if (elem == item) return true;
        return false;
    }

    template<class C, class T> bool contains(const C& c, const T& item)
    {
        return c.find(item) != c.end();
    }

    template<class T> std::vector<T>& append(std::vector<T>& v, const std::vector<T>& other)
    {
        v.insert(v.end(), other.begin(), other.end());
        return v;
    }


    /////////////////////////////////////////////////////////////////////////////////////


    template<class T> T* find(std::vector<T>& v, const T& item) noexcept
    {
        for (T& elem : v) if (elem == item) return &elem;
        return nullptr;
    }

    template<class T> const T* find(const std::vector<T>& v, const T& item) noexcept
    {
        for (const T& elem : v) if (elem == item) return &elem;
        return nullptr;
    }

    template<class K, class V> V* find(std::unordered_map<K,V>& map, const K& key) noexcept
    {
        auto it = map.find(key);
        return it != map.end() ? &it->second : nullptr;
    }

    template<class K, class V> const V* find(const std::unordered_map<K,V>& map, const K& key) noexcept
    {
        auto it = map.find(key);
        return it != map.end() ? &it->second : nullptr;
    }

    /////////////////////////////////////////////////////////////////////////////////////

    template<class T> T* find(element_range<T> v, const T& item) noexcept
    {
        for (T& elem : v) if (elem == item) return &elem;
        return nullptr;
    }

    template<class T, class Pred> T* find_if(element_range<T> v, const Pred& predicate) noexcept
    {
        for (T& elem : v) if (predicate(elem)) return &elem;
        return nullptr;
    }

    template<class T, class Pred> T* find_last_if(element_range<T> v, const Pred& predicate) noexcept
    {
        T* first = v.begin();
        T* last  = v.end();
        while (last != first) {
            if (predicate(*last)) return last;
            --last;
        }
        return nullptr;
    }

    /////////////////////////////////////////////////////////////////////////////////////

    template<class T, class Selector> T* find_smallest(element_range<T> v, const Selector& selector) noexcept
    {
        size_t count = v.size();
        if (!count)
            return nullptr;
        T* selected = &v[0];
        auto currentValue = selector(*selected);
        for (size_t i = 1; i < count; ++i) {
            auto value = selector(v[i]);
            if (value < currentValue) {
                currentValue = value;
                selected = &v[i];
            }
        }
        return selected;
    }

    template<class T, class Selector> T* find_smallest(std::vector<T>& v, const Selector& selector) noexcept
    {
        return find_smallest(range(v), selector);
    }

    template<class T, class Selector> T* find_largest(element_range<T> v, const Selector& selector) noexcept
    {
        size_t count = v.size();
        if (!count)
            return nullptr;
        T* selected = &v[0];
        auto currentValue = selector(*selected);
        for (size_t i = 1; i < count; ++i) {
            auto value = selector(v[i]);
            if (value > currentValue) {
                currentValue = value;
                selected = &v[i];
            }
        }
        return selected;
    }

    template<class T, class Selector> T* find_largest(std::vector<T>& v, const Selector& selector) noexcept
    {
        return find_largest(range(v), selector);
    }

    /////////////////////////////////////////////////////////////////////////////////////
    
    /** @return TRUE if any predicate(element) yields true */
    template<class T, class Pred> bool any_of(const std::vector<T>& v, const Pred& predicate)
    {
        auto* it = v.data();
        for (auto* end = it + v.size(); it != end; ++it)
            if (predicate(*it)) return true;
        return false;
    }
    
    /** @return TRUE if ALL predicate(element) yield true */
    template<class T, class Pred> bool all_of(const std::vector<T>& v, const Pred& predicate)
    {
        auto* it = v.data();
        for (auto* end = it + v.size(); it != end; ++it)
            if (!predicate(*it)) return false;
        return true;
    }

    /** @return TRUE if NONE of predicate(element) yield true */
    template<class T, class Pred> bool none_of(const std::vector<T>& v, const Pred& predicate)
    {
        auto* it = v.data();
        for (auto* end = it + v.size(); it != end; ++it)
            if (predicate(*it)) return false;
        return true;
    }

    /////////////////////////////////////////////////////////////////////////////////////

    template<class T> T sum_all(const std::vector<T>& v)
    {
        T sum {};
        for (auto& item : v) sum += item;
        return sum;
    }

    template<class T, class Aggregate> Aggregate sum_all(
        const std::vector<T>& v, Aggregate (T::*selector)() const)
    {
        Aggregate sum {};
        for (auto& item : v)
            sum += (item.*selector)();
        return sum;
    }

    /////////////////////////////////////////////////////////////////////////////////////

    template<class T, class S> auto transform(const std::vector<T>& v, S (*transform)(const T&))
    {
        std::vector<S> t {};
        t.reserve(v.size());
        for (auto& item : v)
            t.emplace_back(transform(item));
        return t;
    }

    template<class T, class Transform> auto transform(const std::vector<T>& v, Transform transform)
    {
        std::vector<decltype(transform( T{} ))> t {};
        t.reserve(v.size());
        for (auto& item : v)
            t.emplace_back(transform(item));
        return t;
    }

    /////////////////////////////////////////////////////////////////////////////////////

    template<class T, class A> void sort(std::vector<T, A>& v)
    {
        rpp::insertion_sort(v.data(), v.size(), [](T& a, T& b) { return a < b; });
    }

    template<typename T, typename Comparison>
    void sort(std::vector<T>& v, const Comparison& comparison)
    {
        rpp::insertion_sort(v.data(), v.size(), comparison);
    }

    template<typename T, typename Comparison>
    void sort(element_range<T> v, const Comparison& comparison)
    {
        rpp::insertion_sort(v.data(), v.size(), comparison);
    }

    /////////////////////////////////////////////////////////////////////////////////////

    //// std::reduce available in C++17, this is for C++14
    //template<class T, class Reduce> auto reduce(
    //    const std::vector<T>& v, Reduce reduce) -> decltype(reduce())
    //{
    //    decltype(reduce()) result {};
    //    for (auto& item : v)
    //        result += reduce(item);
    //    return result;
    //}

    /////////////////////////////////////////////////////////////////////////////////////
}
