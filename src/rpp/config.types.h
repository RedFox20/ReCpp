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

#ifdef __cplusplus
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
    #else
        using int32  = long;
    #endif
    // Cortex-M newlib makes std::uint32_t an unsigned long. config.h can come after this header,
    // so test the inputs from which it derives RPP_CORTEX_M_ARCH.
    #if RPP_CORTEX_M_ARCH || RPP_STM32_HAL
        using uint32 = __UINT32_TYPE__;
    #elif RPP_INT_SIZE == 4
        using uint32 = unsigned int;
    #else
        using uint32 = unsigned long;
    #endif

        using int64  = long long;
        using uint64 = unsigned long long;

        static_assert(sizeof(int16) == 2 && sizeof(uint16) == 2, "rpp::int16 and rpp::uint16 must be 16 bits");
        static_assert(sizeof(int32) == 4 && sizeof(uint32) == 4, "rpp::int32 and rpp::uint32 must be 32 bits");
        static_assert(sizeof(int64) == 8 && sizeof(uint64) == 8, "rpp::int64 and rpp::uint64 must be 64 bits");
    #ifdef __UINT32_TYPE__ // <cstdint> takes std::uint32_t from this builtin. MSVC has no builtin, and uses unsigned int
        // A std::atomic_uint32_t needs an rpp::uint32 to CAS. rpp::int32 stays int, so Duration::from_millis(int32) takes an int literal
        static_assert(__is_same(uint32, __UINT32_TYPE__), "rpp::uint32 must be std::uint32_t. Add this target to the __UINT32_TYPE__ condition above");
    #endif

    #endif // RPP_BASIC_INTEGER_TYPEDEFS

    /// wraps a log argument for the formatting macros in <rpp/debugging.h>, returns it unchanged
    template<class T>
    struct __wrap
    {
        static constexpr const T& w(const T& arg) noexcept { return arg; }
    };
}
#endif // __cplusplus
