#pragma once
/**
 * Basic Collections and Range extensions, Copyright (c) 2017-2018, Jorma Rebane
 * Distributed under MIT Software License
 */
#include <array>
#include <vector>
#include <unordered_map>
#include <numeric>
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
        T* begin() { return first;    }
        T* end()   { return sentinel; }
        const T* begin() const { return first;    }
        const T* end()   const { return sentinel; }
        int size() const { return int(sentinel-first); }
              T& operator[](int index)       { return first[index]; }
        const T& operator[](int index) const { return first[index]; }
              T* data()       { return first; }
        const T* data() const { return first; }
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
    #if _MSC_VER >= 1910 || __cplusplus > 201700
        return v.emplace_back();
    #else
        v.emplace_back();
        return v.back();
    #endif
    }

    template<class T, class... Args> T& emplace_back(std::vector<T>& v, Args&&... args)
    {
    #if _MSC_VER >= 1910 || __cplusplus > 201700
        return v.emplace_back(std::forward<Args>(args)...);
    #else
        v.emplace_back(std::forward<Args>(args)...);
        return v.back();
    #endif
    }

    template<class T> T pop_back(std::vector<T>& v)
    {
        T item = std::move(v.back());
        v.pop_back();
        return item;
    }

    template<class T> void push_unique(std::vector<T>& v, const T& item)
    {
        for (const T& elem : v) if (elem == item) return;
        v.push_back(item);
    }
    
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

    // erases item at index i by moving the last item to [i]
    // and then popping the last element
    template<class T> void erase_back_swap(std::vector<T>& v, int i)
    {
        v[i] = std::move(v.back());
        v.pop_back();
    }
    
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
    
    template<class T, class Pred> void erase_back_swap_if(std::vector<T>& v, const Pred& condition)
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
