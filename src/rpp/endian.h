#pragma once
/**
 * Endian conversion utilities, Copyright (c) 2025, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "config.h"
#include <cstdint> // uint32_t, uint64_t
#if _MSC_VER
#  include <stdlib.h> // _byteswap_ulong, _byteswap_uint64
#else
#  include <byteswap.h> // __builtin_bswap32, __builtin_bswap64
#endif

namespace rpp
{
    #if _MSC_VER
        #define RPP_BYTESWAP16(x) _byteswap_ushort(x)
        #define RPP_BYTESWAP32(x) _byteswap_ulong(x)
        #define RPP_BYTESWAP64(x) _byteswap_uint64(x)
    #else
        #define RPP_BYTESWAP16(x) __builtin_bswap16(x)
        #define RPP_BYTESWAP32(x) __builtin_bswap32(x)
        #define RPP_BYTESWAP64(x) __builtin_bswap64(x)
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

    /////////////////////////////////////////////////////////////////////
    //// BIG ENDIAN

    /** @brief Write a 16-bit unsigned integer in big-endian format */
    inline void writeBEU16(uint8_t* out, uint16_t value) noexcept
    {
        *reinterpret_cast<uint16_t*>(out) = RPP_TO_BIG16(value);
    }
    /** @brief Write a 32-bit unsigned integer in big-endian format */
    inline void writeBEU32(uint8_t* out, uint32_t value) noexcept
    {
        *reinterpret_cast<uint32_t*>(out) = RPP_TO_BIG32(value);
    }
    /** @brief Write a 64-bit unsigned integer in big-endian format */
    inline void writeBEU64(uint8_t* out, uint64_t value) noexcept
    {
        *reinterpret_cast<uint64_t*>(out) = RPP_TO_BIG64(value);
    }
    /** @brief Read a 16-bit unsigned integer in big-endian format */
    inline uint16_t readBEU16(const uint8_t* in) noexcept
    {
        return RPP_TO_BIG16(*reinterpret_cast<const uint16_t*>(in));
    }
    /** @brief Read a 32-bit unsigned integer in big-endian format */
    inline uint32_t readBEU32(const uint8_t* in) noexcept
    {
        return RPP_TO_BIG32(*reinterpret_cast<const uint32_t*>(in));
    }
    /** @brief Read a 64-bit unsigned integer in big-endian format */
    inline uint64_t readBEU64(const uint8_t* in) noexcept
    {
        return RPP_TO_BIG64(*reinterpret_cast<const uint64_t*>(in));
    }

    /////////////////////////////////////////////////////////////////////
}
