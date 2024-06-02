#pragma once 

// MSVC++ with unknown standard lib
#ifndef RPP_MSVC
#  define RPP_MSVC _MSC_VER
#endif

// MSVC++ with VC standard libs
#ifndef RPP_MSVC_WIN
#  define RPP_MSVC_WIN (_WIN32 && _MSC_VER)
#endif

// GCC with unknown standard lib
#ifndef RPP_GCC
#  define RPP_GCC (__GNUC__ && !__clang__)
#endif

// Clang using GCC/LLVM standard libs
#ifndef RPP_CLANG_LLVM
#  define RPP_CLANG_LLVM (__GNUC__ && __clang__)
#endif

#ifndef RPPAPI
#  if _MSC_VER
#    define RPPAPI //__declspec(dllexport)
#  else // clang/gcc
#    define RPPAPI __attribute__((visibility("default")))
#  endif
#endif

#ifndef RPP_EXTERNC
#  ifdef __cplusplus
#    define RPP_EXTERNC extern "C"
#  else
#    define RPP_EXTERNC
#  endif
#endif

#ifndef RPPCAPI
#  define RPPCAPI RPP_EXTERNC RPPAPI
#endif

#if __cplusplus
#  if _MSC_VER
#    define RPP_HAS_CXX17 (_MSVC_LANG > 201402)
#  else
#    define RPP_HAS_CXX17 (__cplusplus >= 201703L)
#  endif
#endif

#if __cplusplus
#  if _MSC_VER
#    define RPP_HAS_CXX20 (_MSVC_LANG >= 202002L)
#  else
#    define RPP_HAS_CXX20 (__cplusplus >= 202002L)
#  endif
#endif

#if __cplusplus
#  if _MSC_VER
#    define RPP_HAS_CXX23 (_MSVC_LANG > 202002L)
#  else
#    define RPP_HAS_CXX23 (__cplusplus > 202002L)
#  endif
#endif

#if __cplusplus
#  if RPP_HAS_CXX17
#    define RPP_INLINE_STATIC inline static
#  else
#    define RPP_INLINE_STATIC static
#  endif
#endif

#if __cplusplus
#  if __cpp_if_constexpr
#    define RPP_CXX17_IF_CONSTEXPR if constexpr
#  else
#    define RPP_CXX17_IF_CONSTEXPR if
#  endif
#endif

/// @brief evaluates TRUE if AddressSanitizer is enabled
#if defined(__SANITIZE_ADDRESS__)
#  define RPP_ASAN 1
#elif __clang__
#  if __has_feature(address_sanitizer)
#    define RPP_ASAN 1
#  endif
#endif

#if defined(__SANITIZE_THREAD__)
#  define RPP_TSAN 1
#elif __clang__
#  if __has_feature(thread_sanitizer)
#    define RPP_TSAN 1
#  endif
#endif

#if __clang__
#  if __has_feature(undefined_behavior_sanitizer)
#    define RPP_UBSAN 1
#  endif
#endif


/// @brief evaluates TRUE if any sanitizers are enabled
#ifndef RPP_SANITIZERS
#  if defined(RPP_ASAN) || defined(RPP_TSAN) || defined(RPP_UBSAN)
#    define RPP_SANITIZERS 1
#  else
#    define RPP_SANITIZERS 0
#  endif
#endif

//// @note Some functions get inlined too aggressively, leading to some serious code bloat
////       Need to hint the compiler to take it easy ^_^'
#ifndef NOINLINE
#  if _MSC_VER
#    define NOINLINE __declspec(noinline)
#  else
#    define NOINLINE __attribute__((noinline))
#  endif
#endif

//// @note Some strong hints that some functions are merely wrappers, so should be forced inline
#ifndef FINLINE
#  if _MSC_VER
#    define FINLINE __forceinline
#  elif __APPLE__
#    define FINLINE inline __attribute__((always_inline))
#  else
#    define FINLINE __attribute__((always_inline))
#  endif
#endif

#if INTPTR_MAX == INT64_MAX
#  define RPP_64BIT 1
#endif

#ifdef _LIBCPP_STD_VER
#  define _HAS_STD_BYTE (_LIBCPP_STD_VER > 16)
#elif !defined(_HAS_STD_BYTE)
#  define _HAS_STD_BYTE 0
#endif

#ifndef NOCOPY_NOMOVE
#define NOCOPY_NOMOVE(T) \
    T(T&& fwd)             = delete; \
    T& operator=(T&& fwd)  = delete; \
    T(const T&)            = delete; \
    T& operator=(const T&) = delete;
#endif

#ifndef NODISCARD
#  if __cplusplus >= 201500
#    define NODISCARD [[nodiscard]]
#  elif _MSC_VER > 1916
#    ifdef __has_cpp_attribute
#      if __has_cpp_attribute(nodiscard)
#        define NODISCARD [[nodiscard]]
#      endif
#    endif
#    ifndef NODISCARD
#      define NODISCARD
#    endif
#  else
#    define NODISCARD
#  endif
#endif

// Define the noreturn attribute
// Prefixed before function return type:
// RPP_NORETURN void func() { ... }
#ifndef RPP_NORETURN
#  if !__clang__ && __cplusplus >= 201103
#    define RPP_NORETURN [[noreturn]]
#  elif _MSC_VER
#    define RPP_NORETURN __declspec(noreturn)
#  elif __GNUC__ || __clang__
#    define RPP_NORETURN __attribute__((noreturn))
#  else
#    define RPP_NORETURN
#  endif
#endif

// Define the basic size of integer types
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
        using uint32 = unsigned int;
    #else
        using int32  = long;
        using uint32 = unsigned long;
    #endif

        using int64  = long long;
        using uint64 = unsigned long long;

    #endif // RPP_BASIC_INTEGER_TYPEDEFS
}
#endif
