#pragma once
/**
 * Integer type aliases for rpp, split from config.h so a module can export them.
 * This header stays self-contained, so config.h includes it one way.
 * Distributed under MIT Software License
 */

// integer-size macros from compiler builtins, so this header needs no include
#if _MSC_VER
#  define RPP_SHORT_SIZE     2
#  define RPP_INT_SIZE       4
#  define RPP_LONG_SIZE      4
#  define RPP_LONG_LONG_SIZE 8
#else // GCC/Clang
#  define RPP_SHORT_SIZE     __SIZEOF_SHORT__
#  define RPP_INT_SIZE       __SIZEOF_INT__
#  define RPP_LONG_SIZE      __SIZEOF_LONG__
#  define RPP_LONG_LONG_SIZE __SIZEOF_LONG_LONG__
#endif // _MSC_VER

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
