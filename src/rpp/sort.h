#pragma once
/**
 * A minimal sort header for reduced template instantiations, Copyright (c) 2023, Jorma Rebane
 * Distributed under MIT Software License
 * 
 * @note This implementation is NOT faster than std::sort, it simply produces smaller binaries.
 */
#include <cstdint>

namespace rpp
{
    /**
     * @brief A simple insertion sort algorithm
     * @param data The data to sort
     * @param count The number of elements in the data
     * @param comparison The comparison function with signature: 
     * bool(T a, T b) where result is true if a < b, false if a >= b
     */
    template<typename T, typename Comparison>
    void insertion_sort(T* data, size_t count, const Comparison& comparison)
    {
        for (size_t i = 1; i < count; ++i)
        {
            size_t i_being_sorted = i; // index of the element currently being sorted

            // keep shifting `i_being_sorted` to lower index until comparison returns true
            while (i_being_sorted > 0 && !comparison(data[i_being_sorted-1], data[i_being_sorted]))
            {
                // shift i_being_sorted to lower position
                // swap is especially good for types like std::string that specialize std::swap
                std::swap(data[i_being_sorted-1], data[i_being_sorted]);
                // keep track of the position change
                --i_being_sorted;
            }
        }
    }
}
