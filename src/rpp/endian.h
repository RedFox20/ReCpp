#pragma once
/**
 * Endian conversion utilities, Copyright (c) 2025, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "config.h"
#include <cstdint> // uint32_t, uint64_t
#if _MSC_VER
#  include <stdlib.h> // _byteswap_ulong, _byteswap_uint64
#  include <cstring> // memcpy
#else
#  include <byteswap.h> // __builtin_bswap32, __builtin_bswap64
#endif

namespace rpp
{
    #if _MSC_VER
        #define RPP_BYTESWAP16(x) _byteswap_ushort(x)
        #define RPP_BYTESWAP32(x) _byteswap_ulong(x)
        #define RPP_BYTESWAP64(x) _byteswap_uint64(x)
        #define RPP_BUILTIN_MEMCPY(dst, src, size) memcpy(dst, src, size)
    #else
        #define RPP_BYTESWAP16(x) __builtin_bswap16(x)
        #define RPP_BYTESWAP32(x) __builtin_bswap32(x)
        #define RPP_BYTESWAP64(x) __builtin_bswap64(x)
        #define RPP_BUILTIN_MEMCPY(dst, src, size) __builtin_memcpy(dst, src, size)
    #endif

    #if RPP_LITTLE_ENDIAN
        #define RPP_TO_BIG16(x) RPP_BYTESWAP16(x)
        #define RPP_TO_BIG32(x) RPP_BYTESWAP32(x)
        #define RPP_TO_BIG64(x) RPP_BYTESWAP64(x)
        #define RPP_TO_LITTLE16(x) (x)
        #define RPP_TO_LITTLE32(x) (x)
        #define RPP_TO_LITTLE64(x) (x)
    #else
        #define RPP_TO_BIG16(x) (x)
        #define RPP_TO_BIG32(x) (x)
        #define RPP_TO_BIG64(x) (x)
        #define RPP_TO_LITTLE16(x) RPP_BYTESWAP16(x)
        #define RPP_TO_LITTLE32(x) RPP_BYTESWAP32(x)
        #define RPP_TO_LITTLE64(x) RPP_BYTESWAP64(x)
    #endif

    // The read/write helpers load/store through RPP_BUILTIN_MEMCPY rather than
    // *reinterpret_cast<T*>(ptr). A T* access at a non-T-aligned address is UB:
    // the standard lets the compiler assume the alignment, so optimizers
    // (store-merging, vectorization) may emit aligned-only instructions that
    // fault on the packed odd-offset fields these helpers exist to serve. The
    // hazard is independent of hardware. RPP_BUILTIN_MEMCPY keeps the bswap
    // intrinsic for the conversion while doing an alignment-safe load/store that
    // the compiler folds to a single instruction.

    /////////////////////////////////////////////////////////////////////
    //// BIG ENDIAN

    /** @brief Write a 16-bit unsigned integer in big-endian format */
    inline void writeBEU16(void* out, uint16_t value) noexcept
    {
        uint16_t be_value = RPP_TO_BIG16(value);
        RPP_BUILTIN_MEMCPY(out, &be_value, sizeof(be_value));
    }
    /** @brief Write a 32-bit unsigned integer in big-endian format */
    inline void writeBEU32(void* out, uint32_t value) noexcept
    {
        uint32_t be_value = RPP_TO_BIG32(value);
        RPP_BUILTIN_MEMCPY(out, &be_value, sizeof(be_value));
    }
    /** @brief Write a 64-bit unsigned integer in big-endian format */
    inline void writeBEU64(void* out, uint64_t value) noexcept
    {
        uint64_t be_value = RPP_TO_BIG64(value);
        RPP_BUILTIN_MEMCPY(out, &be_value, sizeof(be_value));
    }
    /** @brief Read a 16-bit unsigned integer in big-endian format to machine format*/
    inline uint16_t readBEU16(const void* in) noexcept
    {
        uint16_t v;
        RPP_BUILTIN_MEMCPY(&v, in, sizeof(v));
        return RPP_TO_BIG16(v);
    }
    /** @brief Read a 32-bit unsigned integer in big-endian format to machine format */
    inline uint32_t readBEU32(const void* in) noexcept
    {
        uint32_t v;
        RPP_BUILTIN_MEMCPY(&v, in, sizeof(v));
        return RPP_TO_BIG32(v);
    }
    /** @brief Read a 64-bit unsigned integer in big-endian format to machine format */
    inline uint64_t readBEU64(const void* in) noexcept
    {
        uint64_t v;
        RPP_BUILTIN_MEMCPY(&v, in, sizeof(v));
        return RPP_TO_BIG64(v);
    }

    /////////////////////////////////////////////////////////////////////
    //// LITTLE ENDIAN

    /** @brief Write a 16-bit unsigned integer in little-endian format */
    inline void writeLEU16(void* out, uint16_t value) noexcept
    {
        uint16_t le_value = RPP_TO_LITTLE16(value);
        RPP_BUILTIN_MEMCPY(out, &le_value, sizeof(le_value));
    }
    /** @brief Write a 32-bit unsigned integer in little-endian format */
    inline void writeLEU32(void* out, uint32_t value) noexcept
    {
        uint32_t le_value = RPP_TO_LITTLE32(value);
        RPP_BUILTIN_MEMCPY(out, &le_value, sizeof(le_value));
    }
    /** @brief Write a 64-bit unsigned integer in little-endian format */
    inline void writeLEU64(void* out, uint64_t value) noexcept
    {
        uint64_t le_value = RPP_TO_LITTLE64(value);
        RPP_BUILTIN_MEMCPY(out, &le_value, sizeof(le_value));
    }
    /** @brief Read a 16-bit unsigned integer in little-endian format to machine format */
    inline uint16_t readLEU16(const void* in) noexcept
    {
        uint16_t v;
        RPP_BUILTIN_MEMCPY(&v, in, sizeof(v));
        return RPP_TO_LITTLE16(v);
    }
    /** @brief Read a 32-bit unsigned integer in little-endian format to machine format */
    inline uint32_t readLEU32(const void* in) noexcept
    {
        uint32_t v;
        RPP_BUILTIN_MEMCPY(&v, in, sizeof(v));
        return RPP_TO_LITTLE32(v);
    }
    /** @brief Read a 64-bit unsigned integer in little-endian format to machine format */
    inline uint64_t readLEU64(const void* in) noexcept
    {
        uint64_t v;
        RPP_BUILTIN_MEMCPY(&v, in, sizeof(v));
        return RPP_TO_LITTLE64(v);
    }

    /////////////////////////////////////////////////////////////////////
}
