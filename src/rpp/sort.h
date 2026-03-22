#pragma once
/**
 * A minimal sort header for reduced template instantiations, Copyright (c) 2023, Jorma Rebane
 * Distributed under MIT Software License
 * 
 * @note This implementation is NOT faster than std::sort, it simply produces smaller binaries.
 */
#include "config.h"
#include <stddef.h> // size_t
#if RPP_HAS_CXX20
#  include <concepts> // std::convertible_to
#  include <iterator>  // std::contiguous_iterator
#endif

namespace rpp
{

#if RPP_HAS_CXX20
    /**
     * @brief Concept for a comparison function used in sorting,
     *        which should be a callable with signature: bool(const T& a, const T& b)
     *        where the result is true if a < b, and false if a >= b
     */
    template<typename T, typename Comparison>
    concept sort_comparison = requires(const Comparison& cmp, const T& a, const T& b) {
        { cmp(a, b) } -> std::convertible_to<bool>;
    };

    /**
     * @brief Concept for a container with contiguous storage and random access,
     *        so it can be sorted efficiently with rpp::insertion_sort.
     *        The container must have:
     *         - T* data() member function returning pointer to contiguous storage
     *         - size_t|int size() const member function returning the number of elements
     *         - operator[](size_t|int) const for random access to elements
     */
    template<typename Container>
    concept sortable_container = requires(Container& c) {
        { c.data() } -> std::contiguous_iterator;
        { c.size() } -> std::convertible_to<size_t>;
        c[size_t{}];
    };
#endif

    /**
     * @brief A simple insertion sort algorithm
     * @param data The data to sort
     * @param count The number of elements in the data
     * @param comparison The comparison function with signature: 
     * bool(T a, T b) where result is true if a < b, false if a >= b
     */
    template<typename T, typename Comparison>
    NOINLINE void insertion_sort(T* data, size_t count, const Comparison& comparison)
        noexcept(noexcept(comparison(data[0], data[0])) && noexcept(std::swap(data[0], data[0])))
        #if RPP_HAS_CXX20
            requires sort_comparison<T, Comparison>
        #endif
    {
        for (size_t i = 1; i < count; ++i)
        {
            size_t i_being_sorted = i; // index of the element currently being sorted

            // keep shifting `i_being_sorted` to lower index until comparison returns true
            while (i_being_sorted > 0 && comparison(data[i_being_sorted], data[i_being_sorted-1]))
            {
                // shift i_being_sorted to lower position
                // swap is especially good for types like std::string that specialize std::swap
                std::swap(data[i_being_sorted-1], data[i_being_sorted]);
                // keep track of the position change
                --i_being_sorted;
            }
        }
    }

    template<typename Container, typename Comparison>
    FINLINE void insertion_sort(Container& container, const Comparison& comparison)
        noexcept(noexcept(insertion_sort(container.data(), container.size(), comparison)))
        #if RPP_HAS_CXX20
            requires sortable_container<Container> &&
                     sort_comparison<std::remove_pointer_t<decltype(std::declval<Container&>().data())>, Comparison>
        #endif
    {
        insertion_sort(container.data(), container.size(), comparison);
    }
}
