#pragma once
/**
 * Integer type aliases for rpp, split from config.h so a module can export them.
 * config.h holds the macros, which a module cannot export, and includes this file.
 * Distributed under MIT Software License
 */
#include "config.h" // RPP_INT_SIZE, and the platform probes it rests on

namespace rpp
{
    #ifndef RPP_BASIC_INTEGER_TYPEDEFS
    #define RPP_BASIC_INTEGER_TYPEDEFS
        using byte   = unsigned char;
        using ushort = unsigned short;
        using uint   = unsigned int;
        using ulong  = unsigned long;

        using int16 = short;
        using uint16 = unsigned short;

    #if RPP_INT_SIZE == 4
        using int32  = int;
        using uint32 = unsigned int;
    #else
        using int32  = long;
        using uint32 = unsigned long;
    #endif

        using int64  = long long;
        using uint64 = unsigned long long;

    #endif // RPP_BASIC_INTEGER_TYPEDEFS
}
