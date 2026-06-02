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

    // The read/write helpers compose the value one byte at a time instead of
    // dereferencing *reinterpret_cast<T*>(ptr). This is safe at any alignment:
    // callers commonly read/write packed fields at odd byte offsets, where an
    // unaligned T* access is undefined behaviour and faults on strict-alignment
    // targets (ARM/AVR32). The byte order is explicit, so the helpers are also
    // host-endian-independent; the compiler folds the byte shifts back into a
    // single load/store + bswap where unaligned access is allowed.

    /////////////////////////////////////////////////////////////////////
    //// BIG ENDIAN

    /** @brief Write a 16-bit unsigned integer in big-endian format */
    inline void writeBEU16(void* out, uint16_t value) noexcept
    {
        uint8_t* p = static_cast<uint8_t*>(out);
        p[0] = uint8_t(value >> 8);
        p[1] = uint8_t(value);
    }
    /** @brief Write a 32-bit unsigned integer in big-endian format */
    inline void writeBEU32(void* out, uint32_t value) noexcept
    {
        uint8_t* p = static_cast<uint8_t*>(out);
        p[0] = uint8_t(value >> 24);
        p[1] = uint8_t(value >> 16);
        p[2] = uint8_t(value >> 8);
        p[3] = uint8_t(value);
    }
    /** @brief Write a 64-bit unsigned integer in big-endian format */
    inline void writeBEU64(void* out, uint64_t value) noexcept
    {
        uint8_t* p = static_cast<uint8_t*>(out);
        p[0] = uint8_t(value >> 56);
        p[1] = uint8_t(value >> 48);
        p[2] = uint8_t(value >> 40);
        p[3] = uint8_t(value >> 32);
        p[4] = uint8_t(value >> 24);
        p[5] = uint8_t(value >> 16);
        p[6] = uint8_t(value >> 8);
        p[7] = uint8_t(value);
    }
    /** @brief Read a 16-bit unsigned integer in big-endian format to machine format*/
    inline uint16_t readBEU16(const void* in) noexcept
    {
        const uint8_t* p = static_cast<const uint8_t*>(in);
        return uint16_t(uint16_t(p[0]) << 8 | uint16_t(p[1]));
    }
    /** @brief Read a 32-bit unsigned integer in big-endian format to machine format */
    inline uint32_t readBEU32(const void* in) noexcept
    {
        const uint8_t* p = static_cast<const uint8_t*>(in);
        return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | uint32_t(p[3]);
    }
    /** @brief Read a 64-bit unsigned integer in big-endian format to machine format */
    inline uint64_t readBEU64(const void* in) noexcept
    {
        const uint8_t* p = static_cast<const uint8_t*>(in);
        return uint64_t(p[0]) << 56 | uint64_t(p[1]) << 48 | uint64_t(p[2]) << 40 | uint64_t(p[3]) << 32
             | uint64_t(p[4]) << 24 | uint64_t(p[5]) << 16 | uint64_t(p[6]) << 8  | uint64_t(p[7]);
    }

    /////////////////////////////////////////////////////////////////////
    //// LITTLE ENDIAN

    /** @brief Write a 16-bit unsigned integer in little-endian format */
    inline void writeLEU16(void* out, uint16_t value) noexcept
    {
        uint8_t* p = static_cast<uint8_t*>(out);
        p[0] = uint8_t(value);
        p[1] = uint8_t(value >> 8);
    }
    /** @brief Write a 32-bit unsigned integer in little-endian format */
    inline void writeLEU32(void* out, uint32_t value) noexcept
    {
        uint8_t* p = static_cast<uint8_t*>(out);
        p[0] = uint8_t(value);
        p[1] = uint8_t(value >> 8);
        p[2] = uint8_t(value >> 16);
        p[3] = uint8_t(value >> 24);
    }
    /** @brief Write a 64-bit unsigned integer in little-endian format */
    inline void writeLEU64(void* out, uint64_t value) noexcept
    {
        uint8_t* p = static_cast<uint8_t*>(out);
        p[0] = uint8_t(value);
        p[1] = uint8_t(value >> 8);
        p[2] = uint8_t(value >> 16);
        p[3] = uint8_t(value >> 24);
        p[4] = uint8_t(value >> 32);
        p[5] = uint8_t(value >> 40);
        p[6] = uint8_t(value >> 48);
        p[7] = uint8_t(value >> 56);
    }
    /** @brief Read a 16-bit unsigned integer in little-endian format to machine format */
    inline uint16_t readLEU16(const void* in) noexcept
    {
        const uint8_t* p = static_cast<const uint8_t*>(in);
        return uint16_t(uint16_t(p[0]) | uint16_t(p[1]) << 8);
    }
    /** @brief Read a 32-bit unsigned integer in little-endian format to machine format */
    inline uint32_t readLEU32(const void* in) noexcept
    {
        const uint8_t* p = static_cast<const uint8_t*>(in);
        return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
    }
    /** @brief Read a 64-bit unsigned integer in little-endian format to machine format */
    inline uint64_t readLEU64(const void* in) noexcept
    {
        const uint8_t* p = static_cast<const uint8_t*>(in);
        return uint64_t(p[0]) | uint64_t(p[1]) << 8 | uint64_t(p[2]) << 16 | uint64_t(p[3]) << 24
             | uint64_t(p[4]) << 32 | uint64_t(p[5]) << 40 | uint64_t(p[6]) << 48 | uint64_t(p[7]) << 56;
    }

    /////////////////////////////////////////////////////////////////////
}
