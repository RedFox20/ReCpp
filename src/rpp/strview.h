#pragma once
/**
 * String Tokenizer/View, Copyright (c) 2014-2018, Jorma Rebane
 * Distributed under MIT Software License
 */
#ifndef __cplusplus
#  error <rpp/strview.h> requires C++14 or higher
#endif
#include "config.h"
#include <cstring>    // C string utilities
#include <string>     // compatibility with std::string
#include <concepts> // std::same_as

#ifndef RPP_STRVIEW_H
#define RPP_STRVIEW_H 1
#endif

// ReSharper disable IdentifierTypo
// ReSharper disable CommentTypo

/**
 * This is a simplified string tokenizer class.
 *
 * Those who are not familiar with string tokens - these are strings that don't actually
 * hold nor control the string data. These strings are read-only and the only operations
 * we can do are shifting the start/end pointers.
 *
 * That is how the strview class is built and subsequently operations like trim()
 * just shift the start/end pointers towards the middle.
 * This appears to be extremely efficient when parsing large file buffers - instead of
 * creating thousands of string objects, we tokenize substrings of the file buffer.
 *
 * The structures below contain methods for efficiently manipulating the strview class.
 */

#if _MSC_VER
#  pragma warning(push)
#  pragma warning(disable:4201) // nameless struct/union warning
#endif

namespace rpp
{
    /////////// Small string optimized search functions (low loop setup latency, but bad with large strings)

    // This is same as memchr, but optimized for very small control strings
    // Retains string literal array length information
    template<int N> bool strcontains(const char (&str)[N], char ch) noexcept {
        for (int i = 0; i < N; ++i)
            if (str[i] == ch) return true;
        return false;
    }

    /**
     * @note Same as strpbrk, except we're not dealing with 0-term strings
     * @note This function is optimized for 4-8 char str and 3-4 char control.
     * @note Retains string literal array length information
     */
    template<int N> const char* strcontains(const char* str, int nstr, const char (&control)[N]) noexcept {
        for (; nstr; --nstr, ++str)
            if (strcontains<N>(control, *str))
                return str; // done
        return nullptr; // not found
    }

    template<size_t N> bool strequals(const char* s1, const char (&s2)[N]) noexcept {
        for (size_t i = 0; i < (N - 1); ++i)
            if (s1[i] != s2[i]) return false; // not equal.
        return true;
    }
    template<size_t N> bool strequalsi(const char* s1, const char (&s2)[N]) noexcept {
        for (size_t i = 0; i < (N - 1); ++i)
            if (::toupper(s1[i]) != ::toupper(s2[i])) return false; // not equal.
        return true;
    }


    // This is same as memchr, but optimized for very small control strings
    RPPAPI bool strcontains(const char* str, int len, char ch) noexcept;
    RPPAPI bool strcontainsi(const char* str, int len, char ch) noexcept;
    /**
     * @note Same as strpbrk, except we're not dealing with 0-term strings
     * @note This function is optimized for 4-8 char str and 3-4 char control.
     */
    RPPAPI const char* strcontains(const char* str, int nstr, const char* control, int ncontrol) noexcept;
    RPPAPI NOINLINE bool strequals(const char* s1, const char* s2, int len) noexcept;
    RPPAPI NOINLINE bool strequalsi(const char* s1, const char* s2, int len) noexcept;

#if RPP_ENABLE_UNICODE
    template<int N> bool strcontains(const char16_t (&str)[N], char16_t ch) noexcept {
        for (int i = 0; i < N; ++i)
            if (str[i] == ch) return true;
        return false;
    }
    template<int N> const char16_t* strcontains(const char16_t* str, int nstr, const char16_t (&control)[N]) noexcept {
        for (; nstr; --nstr, ++str)
            if (strcontains<N>(control, *str))
                return str; // done
        return nullptr; // not found
    }
    RPPAPI bool strcontains(const char16_t* str, int len, char16_t ch) noexcept;
    RPPAPI const char16_t* strcontains(const char16_t* str, int nstr, const char16_t* control, int ncontrol) noexcept;
    RPPAPI NOINLINE bool strequals(const char16_t* s1, const char16_t* s2, int len) noexcept;
    RPPAPI NOINLINE bool strequalsi(const char16_t* s1, const char16_t* s2, int len) noexcept;
#endif // RPP_ENABLE_UNICODE

    /**
     * C-locale specific, simplified atof that also outputs the end of parsed string
     * @param str Input string, e.g. "-0.25" / ".25", etc.. '+' is not accepted as part of the number
     * @param len Length of the string to parse
     * @param end (optional) Destination pointer for end of parsed string. Can be NULL.
     * @return Parsed float
     */
    RPPAPI double to_double(const char* str, int len, const char** end = nullptr) noexcept;
    inline double to_double(const char* str, const char** end = nullptr) noexcept { return to_double(str, 64, end); }

    /**
     * Fast locale agnostic atoi
     * @param str Input string, e.g. "-25" or "25", etc.. '+' is not accepted as part of the number
     * @param len Length of the string to parse
     * @param end (optional) Destination pointer for end of parsed string. Can be NULL.
     * @return Parsed int
     */
    RPPAPI int to_int(const char* str, int len, const char** end = nullptr) noexcept;
    inline int to_int(const char* str, const char** end = nullptr) noexcept { return to_int(str, 32, end); }

    /**
     * Fast locale agnostic atoi
     * @param str Input string, e.g. "-25" or "25", etc.. '+' is not accepted as part of the number
     * @param len Length of the string to parse
     * @param end (optional) Destination pointer for end of parsed string. Can be NULL.
     * @return Parsed int
     */
    RPPAPI rpp::int64 to_int64(const char* str, int len, const char** end = nullptr) noexcept;
    inline rpp::int64 to_int64(const char* str, const char** end = nullptr) noexcept { return to_int64(str, 32, end); }

    /**
     * Fast locale agnostic atoi
     * @param str Input string, e.g. "-25" or "25", etc.. '+' is not accepted as part of the number
     *            HEX syntax is supported: 0xBA or 0BA will parse hex values instead of regular integers
     * @param len Length of the string to parse
     * @param end (optional) Destination pointer for end of parsed string. Can be NULL.
     * @return Parsed int
     */
    RPPAPI int to_inthx(const char* str, int len, const char** end = nullptr) noexcept;
    inline int to_inthx(const char* str, const char** end = nullptr) noexcept { return to_inthx(str, 32, end); }


    /**
     * C-locale specific, simplified ftoa() that prints pretty human-readable floats
     * @param buffer Destination buffer assumed to be big enough. 32 bytes is more than enough for Float/Double.
     * @param f Float/Double value to convert to string
     * @param maxDecimals Maximum number of decimal digits to output, default=6
     * @return Length of the string
     */
    RPPAPI int _tostring(char* buffer, double f, int maxDecimals = 6) noexcept;
    inline int _tostring(char* buffer, float f, int maxDecimals = 6) noexcept { return _tostring(buffer, (double)f, maxDecimals); }


    /**
     * Fast locale agnostic itoa
     * @param buffer Destination buffer assumed to be big enough. 32 bytes is more than enough.
     * @param value Integer value to convert to string
     * @return Length of the string
     */
    RPPAPI int _tostring(char* buffer, int    value) noexcept;
    RPPAPI int _tostring(char* buffer, int64  value) noexcept;
    RPPAPI int _tostring(char* buffer, uint   value) noexcept;
    RPPAPI int _tostring(char* buffer, uint64 value) noexcept;

    inline int _tostring(char* buffer, rpp::byte value) noexcept { return _tostring(buffer, (uint)value); }
    inline int _tostring(char* buffer, short value)     noexcept { return _tostring(buffer, (int)value);  }
    inline int _tostring(char* buffer, ushort value)    noexcept { return _tostring(buffer, (uint)value); }

    #if RPP_LONG_SIZE == RPP_INT_SIZE
    inline int _tostring(char* buffer, long value)      noexcept { return _tostring(buffer, (int)value);  }
    inline int _tostring(char* buffer, ulong value)     noexcept { return _tostring(buffer, (uint)value); }
    #else
    inline int _tostring(char* buffer, long value)      noexcept { return _tostring(buffer, (int64)value);  }
    inline int _tostring(char* buffer, ulong value)     noexcept { return _tostring(buffer, (uint64)value); }
    #endif

    // NEW to_string() overloads

    /**
     * C-locale specific, simplified ftoa() that prints pretty human-readable floats
     * @param buffer Destination buffer assumed to be big enough. 32 bytes is more than enough for Float/Double.
     * @param f Float/Double value to convert to string
     * @param maxDecimals Maximum number of decimal digits to output, default=6
     * @return Length of the string
     */
    inline int to_string(char* buffer, float  value, int maxDecimals = 6) noexcept { return _tostring(buffer, value, maxDecimals); }
    inline int to_string(char* buffer, double value, int maxDecimals = 6) noexcept { return _tostring(buffer, value, maxDecimals); }

    /**
     * Fast locale agnostic itoa
     * @param buffer Destination buffer assumed to be big enough. 32 bytes is more than enough.
     * @param value Integer value to convert to string
     * @return Length of the string
     */
    inline int to_string(char* buffer, int    value) noexcept { return _tostring(buffer, value); }
    inline int to_string(char* buffer, int64  value) noexcept { return _tostring(buffer, value); }
    inline int to_string(char* buffer, uint   value) noexcept { return _tostring(buffer, value); }
    inline int to_string(char* buffer, uint64 value) noexcept { return _tostring(buffer, value); }

    inline int to_string(char* buffer, rpp::byte value) noexcept { return _tostring(buffer, (uint)value); }
    inline int to_string(char* buffer, short value)     noexcept { return _tostring(buffer, (int)value);  }
    inline int to_string(char* buffer, ushort value)    noexcept { return _tostring(buffer, (uint)value); }

    #if RPP_LONG_SIZE == RPP_INT_SIZE
    inline int to_string(char* buffer, long value)      noexcept { return _tostring(buffer, (int)value);  }
    inline int to_string(char* buffer, ulong value)     noexcept { return _tostring(buffer, (uint)value); }
    #else
    inline int to_string(char* buffer, long value)      noexcept { return _tostring(buffer, (int64)value);  }
    inline int to_string(char* buffer, ulong value)     noexcept { return _tostring(buffer, (uint64)value); }
    #endif


    #ifndef RPP_CONSTEXPR_STRLEN
    #  if _MSC_VER && _MSVC_LANG >= 201703L
    #    define RPP_CONSTEXPR_STRLEN constexpr
    #  elif __GNUG__ && __cplusplus >= 201703L
    #    define RPP_CONSTEXPR_STRLEN constexpr
    #  else
    #    define RPP_CONSTEXPR_STRLEN
    #  endif
    #endif

    using string = std::string;
    using ustring = std::u16string;

    #define RPP_UTF8LEN(c_str) (static_cast<int>(std::char_traits<char>::length((const char*)(c_str))))
    #define RPP_UTF16LEN(u_str) (static_cast<int>(std::char_traits<char16_t>::length((const char16_t*)(u_str))))

    /**
     * String token for efficient parsing.
     * Represents a 'weak' reference string with Start pointer and Length.
     * The string can be parsed, manipulated and tokenized through methods like:
     *  - trim()
     *  - next()
     *  - skip_until() skip_after()
     *  - trim_start() trim_end()
     *  - to_int() to_float()
     */
    struct RPPAPI strview
    {
        using char_t = char; // compatible char_t for rpp::strview
        using string_t = string; // compatible std string for rpp::strview
        using string_view_t = std::string_view; // compatible std string_view for rpp::strview

        const char* str; // start of string
        int len;         // length of string

        FINLINE constexpr strview()                            noexcept : str{""},  len{0} {}
        FINLINE RPP_CONSTEXPR_STRLEN strview(char* str)        noexcept : str{str}, len{RPP_UTF8LEN(str)} {}
        FINLINE RPP_CONSTEXPR_STRLEN strview(const char* str)  noexcept : str{str}, len{RPP_UTF8LEN(str)} {}
        FINLINE constexpr strview(const char* str, int len)    noexcept : str{str}, len{len} {}
        FINLINE constexpr strview(const char* str, size_t len) noexcept : str{str}, len{static_cast<int>(len)} {}
        FINLINE constexpr strview(const char* str, const char* end) noexcept : str{str}, len{static_cast<int>(end - str)} {}
        FINLINE constexpr strview(const void* str, const void* end) noexcept : strview{static_cast<const char*>(str), static_cast<const char*>(end)} {}
        FINLINE strview(const string_t& s RPP_LIFETIMEBOUND) noexcept : str{s.c_str()}, len{static_cast<int>(s.length())} {}
        FINLINE strview(const string_view_t& s)              noexcept : str{s.data()},  len{static_cast<int>(s.length())} {}

    #ifdef __cpp_char8_t // fundamental type char8_t since C++20
        FINLINE strview(const char8_t* str) noexcept : str{reinterpret_cast<const char*>(str)}, len{RPP_UTF8LEN(str)} {}
        strview(const char16_t* str) = delete; // char16_t is not supported by strview
    #endif

        // disallow accidental init from char or bool
        strview(char) = delete;
        strview(bool) = delete;

        // disallow accidental assignment from an std::string&& which is an error, 
        // because the strview will be invalidated immediately
        // however strview(std::string&&) is allowed because it allows passing temporaries as arguments to functions
        strview& operator=(string_t&&) = delete;

        FINLINE RPP_CONSTEXPR_STRLEN strview& operator=(const char* s) noexcept {
            this->str = s ? s : "";
            this->len = s ? RPP_UTF8LEN(str) : 0;
            return *this;
        }
        template<int N>
        FINLINE constexpr strview& operator=(const char (&s)[N]) noexcept {
            this->str = s; this->len = N-1;
            return *this;
        }

        /** Creates a new string from this string-strview */
        FINLINE string_t& to_string(string_t& out) const { return out.assign(str, (size_t)len); }
        string_t to_string() const { return string_t{str, (size_t)len}; }

        // this is implicit by design; but it may cause some unexpected conversions to std::string
        // main goal is to provide convenient automatic conversion:  string s = my_string_view;
        operator string_t() const { return string_t{str, (size_t)len}; }

        /** Clears the strview */
        FINLINE void clear() noexcept { str = ""; len = 0; }
        /** @return Length of the string */
        FINLINE constexpr int length() const noexcept { return len; }
        FINLINE constexpr int size()   const noexcept { return len; }
        /** @return TRUE if length of the string is 0 - thus the string is empty */
        FINLINE constexpr bool empty() const noexcept { return !len; }
        /** @return TRUE if string is non-empty */
        explicit FINLINE constexpr operator bool() const noexcept { return len != 0; }
        /** @return Pointer to the start of the string */
        FINLINE const char* c_str() const noexcept { return str; }
        FINLINE const char* data()  const noexcept { return str; }
        FINLINE const char* begin() const noexcept { return str; }
        FINLINE const char* end()   const noexcept { return str + len; }
        FINLINE char front() const noexcept { return *str; }
        FINLINE char back()  const noexcept { return str[len - 1]; }

        FINLINE constexpr const char& operator[](int index) const noexcept { return str[index]; }

        /** 
         * Copies this str[len] string into a C-string array
         * However, if THIS string is null terminated, this operation is a NOP and behaves like c_str()
         */
        NODISCARD NOINLINE const char* to_cstr(char* buf, int max) const noexcept;
        template<int N>
        NODISCARD FINLINE const char* to_cstr(char (&buf)[N]) const noexcept { return to_cstr(buf, N); }
        /** 
         * Copies this str[len] into a max of 512 byte static thread-local C-string array 
         * Result is only valid until next call to this method
         * However, if THIS string is null terminated, this operation is a NOP and behaves like c_str()
         */
        NODISCARD NOINLINE const char* to_cstr() const noexcept;

        /** Parses this strview as an integer, without modifying this strview */
        FINLINE int to_int() const noexcept { return rpp::to_int(str, len); }
        /** Parses this strview as a HEX integer ('0xff' or '0ff' or 'ff') */
        FINLINE int to_int_hex() const noexcept { return to_inthx(str, len); }
        /** Parses this strview as an int64, without modifying this strview */
        FINLINE rpp::int64 to_int64() const noexcept { return rpp::to_int64(str, len); }
        /** Parses this strview as a long */
        FINLINE long to_long() const noexcept { return static_cast<long>(rpp::to_int(str, len)); }
        /** Parses this strview as a float */
        FINLINE float to_float() const noexcept { return static_cast<float>(rpp::to_double(str, len)); }
        /** Parses this strview as a double */
        FINLINE double to_double() const noexcept { return rpp::to_double(str, len); }
        // Relaxed parsing of this strview as a boolean.
        // Accepts strings that start_with ignorecase: "true", "yes", "on", "1"
        // Everything else is parsed as false.
        // strings such as "true\n" will be parsed as true; same for "trueStatement" --> true,
        // because the conversion is relaxed, not strict.
        // For strict parsing, you should use strview::equalsi("true")
        bool to_bool() const noexcept;

        /** @return TRUE if the strview is only whitespace: " \t\r\n"  */
        NOINLINE bool is_whitespace() const noexcept;
        /** @return TRUE if the strview ends with a null terminator */
        FINLINE bool is_nullterm() const noexcept { return str[len] == '\0'; }

        /** @returns TRUE if this strview starts with a number-like prefix, eg "-1" or "+7" */
        NOINLINE bool is_numberlike() const noexcept;

        /** Trims the start of the string from any whitespace */
        NOINLINE strview& trim_start() noexcept;
        /** Trims start from this char */
        NOINLINE strview& trim_start(const char ch) noexcept;
        NOINLINE strview& trim_start(const char* chars, int nchars) noexcept;
        /* Optimized noinline version for specific character sequences */
        template<int N> NOINLINE strview& trim_start(const char (&chars)[N]) noexcept { 
            auto s = str;
            auto n = len;
            for (; n && strcontains<N>(chars, *s); ++s, --n)
                ;
            str = s; len = n; // write result
            return *this;
        }
        inline strview& trim_start(strview s) noexcept { return trim_start(s.str, s.len); }

        /** Trims end from this char */
        NOINLINE strview& trim_end(char ch) noexcept;
        /** Trims the end of the string from any whitespace */
        NOINLINE strview& trim_end() noexcept;
        NOINLINE strview& trim_end(const char* chars, int nchars) noexcept;
        /* Optimized noinline version for specific character sequences */
        template<int N>NOINLINE strview& trim_end(const char (&chars)[N]) noexcept {
            auto n = len;
            auto e = str + n;
            for (; n && strcontains<N>(chars, *--e); --n)
                ;
            len = n; // write result
            return *this;
        }
        inline strview& trim_end(strview s) noexcept { return trim_end(s.str, s.len); }

        /** Trims both start and end with whitespace */
        FINLINE strview& trim() noexcept { return trim_start().trim_end(); }
        /** Trims both start and end width this char*/
        FINLINE strview& trim(char ch) noexcept { return trim_start(ch).trim_end(ch); }
        FINLINE strview& trim(const char* chars, int nchars) noexcept { return trim_start(chars, nchars).trim_end(chars, nchars); }
        /** Trims both start and end with any of the given chars */
        template<int N> FINLINE strview& trim(const char (&chars)[N]) noexcept { 
            return trim_start(chars, N-1).trim_end(chars, N-1);
        }
        inline strview& trim(strview s) noexcept { return trim_start(s.str, s.len).trim_end(s.str, s.len); }

        /** Consumes the first character in the strview if possible. */
        FINLINE strview& chomp_first() noexcept { if (len) ++str;--len; return *this; }
        /** Consumes the last character in the strview if possible. */
        FINLINE strview& chomp_last() noexcept { if (len) --len; return *this; }

        /** Pops and returns the first character in the strview if possible. */
        FINLINE char pop_front() noexcept { if (len) { char ch = *str++; --len;       return ch; } return '\0'; }
        /** Pops and returns the last character in the strview if possible. */
        FINLINE char pop_back()  noexcept { if (len) { char ch = str[len - 1]; --len; return ch; } return '\0'; }

        /** Consumes the first COUNT characters in the strview String if possible. */
        FINLINE strview& chomp_first(int count) noexcept { 
            int n = count < len ? count : len;
            str += n; len -= n;
            return *this;
        }
        /** Consumes the last COUNT characters in the strview String if possible. */
        FINLINE strview& chomp_last(int count) noexcept {
            len -= (count < len ? count : len);
            return *this; 
        }

        /** 
         * UNSAFE view of a new string AFTER this one
         * @code
         * strview content = "key:true";
         * strview value = content.find_sv("key:").after(4); // "true"
         * @endcode
         */
        FINLINE strview after(int len) const noexcept { return { end(), len }; }

        /** Similar to after(int), but the end is calculated via @param limit */
        FINLINE strview after_until(const strview& limit) const noexcept { return { end(), limit.end() }; }

        /** @return TRUE if the strview contains this char */
        FINLINE bool contains(char c) const noexcept { return memchr(str, c, (size_t)len) != nullptr; }
        /** @return TRUE if the strview contains this string */
        FINLINE bool contains(const strview& s) const noexcept { return find(s) != nullptr; }

        /** @return TRUE if the strview contains this char (ignoring case) */
        FINLINE bool containsi(char c) const noexcept { return strcontainsi(str, len, c); }
        /** @return TRUE if the strview contains this string (ignoring case) */
        FINLINE bool containsi(const strview& s) const noexcept { return find_icase(s) != nullptr; }

        /** @return TRUE if the strview contains any of the chars */
        NOINLINE bool contains_any(const char* chars, int nchars) const noexcept {
            return strcontains(str, len, chars, nchars) != nullptr;
        }
        template<int N> FINLINE bool contains_any(const char (&chars)[N]) const noexcept {
            return strcontains<N>(str, len, chars) != nullptr;
        }

        /** @return Pointer to char if found, NULL otherwise */
        FINLINE const char* find(char c) const noexcept {
            return (const char*)memchr(str, c, (size_t)len);
        }

        /** @return Pointer to start of substring if found, NULL otherwise */
        NOINLINE const char* find(const char* substr, int sublen) const noexcept;
        FINLINE const char* find(const strview& substr) const noexcept { 
            return find(substr.str, substr.len); 
        }
        template<int N> FINLINE const char* find(const char (&substr)[N]) const noexcept { 
            return find(substr, N - 1); 
        }

        /** @return Pointer to start of substring (ignoring case), NULL otherwise */
        NOINLINE const char* find_icase(const char* substr, int sublen) const noexcept;
        FINLINE const char* find_icase(const strview& substr) const noexcept { 
            return find_icase(substr.str, substr.len); 
        }

        strview find_sv(const strview& substr) const noexcept {
            const char* s = find(substr);
            return s ? strview{s, substr.len} : strview{};
        }
        template<int N> strview find_sv(const char (&substr)[N]) const noexcept { 
            const char* s = find<N>(substr);
            return s ? strview{s, N - 1} : strview{};
        }


        /** @return Pointer to char if found using reverse search, NULL otherwise */
        NOINLINE const char* rfind(char c) const noexcept;

        /** 
         * Forward searches for any of the specified chars
         * @return Pointer to char if found, NULL otherwise.
         */
        const char* findany(const char* chars, int n) const noexcept;
        template<int N> FINLINE const char* findany(const char (&chars)[N]) const noexcept {
            return findany(chars, N - 1);
        }

        /** 
         * Reverse searches for any of the specified chars
         * @return Pointer to char if found, NULL otherwise.
         */
        const char* rfindany(const char* chars, int n) const noexcept;
        template<int N> FINLINE const char* rfindany(const char (&chars)[N]) const noexcept {
            return rfindany(chars, N - 1);
        }


        /**
         * Count number of occurrances of this character inside the strview bounds
         */
        int count(char ch) const noexcept;

        /** Index of character, or -1 if not found */
        int indexof(char ch) const noexcept;
        int indexof(char ch, int start) const noexcept;
        int indexof(char ch, int start, int end) const noexcept;

        // Reverse iterating indexof
        int rindexof(char ch) const noexcept;

        // First index of any character that matches Chars array
        int indexofany(const char* chars, int n) const noexcept;
        template<int N> FINLINE int indexofany(const char (&chars)[N]) const noexcept {
            return indexofany(chars, N - 1);
        }

        /** @return TRUE if this strview starts with the specified string */
        FINLINE bool starts_with(const char* s, int length) const noexcept {
            return len >= length && strequals(str, s, length);
        }
        template<int N> FINLINE bool starts_with(const char (&s)[N]) const noexcept { 
            return len >= (N - 1) && strequals<N>(str, s);
        }
        FINLINE bool starts_with(const strview& s) const noexcept { return starts_with(s.str, s.len); }
        FINLINE bool starts_with(char ch) const noexcept { return len && *str == ch; }


        /** @return TRUE if this strview starts with IGNORECASE of the specified string */
        FINLINE bool starts_withi(const char* s, int length) const noexcept {
            return len >= length && strequalsi(str, s, length);
        }
        template<int N> FINLINE bool starts_withi(const char (&s)[N]) const noexcept { 
            return len >= (N - 1) && strequalsi<N>(str, s);
        }
        FINLINE bool starts_withi(const strview& s) const noexcept { return starts_withi(s.str, s.len); }
        FINLINE bool starts_withi(char ch) const noexcept { return len && ::toupper(*str) == ::toupper(ch); }


        /** @return TRUE if the strview ends with the specified string */
        FINLINE bool ends_with(const char* s, int slen) const noexcept {
            return len >= slen && strequals(str + len - slen, s, slen);
        }
        template<int N> FINLINE bool ends_with(const char (&s)[N]) const noexcept { 
            return len >= (N - 1) && strequals<N>(str + len - (N - 1), s);
        }
        FINLINE bool ends_with(const strview s) const noexcept { return ends_with(s.str, s.len); }
        FINLINE bool ends_with(char ch)         const noexcept { return len && str[len-1] == ch; }


        /** @return TRUE if this strview ends with IGNORECASE of the specified string */
        FINLINE bool ends_withi(const char* s, int slen) const noexcept {
            return len >= slen && strequalsi(str + len - slen, s, slen);
        }
        template<int N> FINLINE bool ends_withi(const char (&s)[N]) const noexcept { 
            return len >= (N - 1) && strequalsi<N>(str + len - (N - 1), s);
        }
        FINLINE bool ends_withi(const strview s) const noexcept { return ends_withi(s.str, s.len); }
        FINLINE bool ends_withi(char ch) const noexcept { return len && ::toupper(str[len-1]) == ::toupper(ch); }


        /** @return TRUE if this strview equals the specified string */
        FINLINE bool equals(const char* s, int length) const noexcept { return len == length && strequals(str, s, length); }
        template<int N>
        FINLINE bool equals(const char (&s)[N]) const noexcept { return len == (N-1) && strequals<N>(str, s); }
        FINLINE bool equals(const strview& s)   const noexcept { return equals(s.str, s.len);                 }

        /** @return TRUE if this strview equals IGNORECASE the specified string */
        FINLINE bool equalsi(const char* s, int length) const noexcept {
            return len == length && strequalsi(str, s, length);
        }
        template<int N>
        FINLINE bool equalsi(const char (&s)[N]) const noexcept { return len == (N-1) && strequalsi<N>(str, s); }
        FINLINE bool equalsi(const strview& s)   const noexcept { return equalsi(s.str, s.len);                 }

        template<int SIZE> FINLINE bool operator==(const char(&s)[SIZE]) const noexcept { return equals<SIZE>(s); }
        template<int SIZE> FINLINE bool operator!=(const char(&s)[SIZE]) const noexcept { return !equals<SIZE>(s); }
        FINLINE bool operator==(const string_t& s) const noexcept { return  equals(s.data(), (int)s.size()); }
        FINLINE bool operator!=(const string_t& s) const noexcept { return !equals(s.data(), (int)s.size()); }
        FINLINE bool operator==(const string_view_t& s) const noexcept { return  equals(s.data(), (int)s.size()); }
        FINLINE bool operator!=(const string_view_t& s) const noexcept { return !equals(s.data(), (int)s.size()); }
        FINLINE bool operator==(const strview& s) const noexcept { return  equals(s.str, s.len); }
        FINLINE bool operator!=(const strview& s) const noexcept { return !equals(s.str, s.len); }
        FINLINE bool operator==(char* s) const noexcept { return  strequals(s, str, len); }
        FINLINE bool operator!=(char* s) const noexcept { return !strequals(s, str, len); }
        FINLINE bool operator==(char ch) const noexcept { return len == 1 && *str == ch; }
        FINLINE bool operator!=(char ch) const noexcept { return len != 1 || *str != ch; }

        /** @brief Compares this strview to string data */
        NOINLINE int compare(const char* s, int n) const noexcept;
        NOINLINE int compare(const char* s) const noexcept;
        FINLINE int compare(const strview& b) const noexcept { return compare(b.str, b.len); }
        FINLINE int compare(const string_t& b) const noexcept { return compare(b.c_str(),(int)b.size()); }
        
        FINLINE bool operator< (const strview& s) const noexcept { return compare(s.str, s.len) < 0; }
        FINLINE bool operator> (const strview& s) const noexcept { return compare(s.str, s.len) > 0; }
        FINLINE bool operator<=(const strview& s) const noexcept { return compare(s.str, s.len) <= 0; }
        FINLINE bool operator>=(const strview& s) const noexcept { return compare(s.str, s.len) >= 0; }
        FINLINE bool operator< (const string_t& s) const noexcept { return compare(s.c_str(),(int)s.size()) < 0; }
        FINLINE bool operator> (const string_t& s) const noexcept { return compare(s.c_str(),(int)s.size()) > 0; }
        FINLINE bool operator<=(const string_t& s) const noexcept { return compare(s.c_str(),(int)s.size()) <= 0; }
        FINLINE bool operator>=(const string_t& s) const noexcept { return compare(s.c_str(),(int)s.size()) >= 0; }
        template<int SIZE> FINLINE bool operator< (const char(&s)[SIZE]) const noexcept {return compare(s,SIZE-1) < 0;}
        template<int SIZE> FINLINE bool operator> (const char(&s)[SIZE]) const noexcept {return compare(s,SIZE-1) > 0;}
        template<int SIZE> FINLINE bool operator<=(const char(&s)[SIZE]) const noexcept {return compare(s,SIZE-1) <= 0;}
        template<int SIZE> FINLINE bool operator>=(const char(&s)[SIZE]) const noexcept {return compare(s,SIZE-1) >= 0;}
        
        /**
         * Splits the string into TWO and returns strview to the first one
         * @param delim Delimiter char to split on
         */
        NOINLINE strview split_first(char delim) const noexcept;

        /**
         * Splits the string into TWO and returns strview to the first one
         * @param substr Substring to split with
         * @param sublen Length of the substring
         */
        NOINLINE strview split_first(const char* substr, int sublen) const noexcept;
        template<int N> FINLINE strview split_first(const char(&substr)[N]) noexcept {
            return split_first(substr, N-1);
        }

        /**
         * Splits the string into TWO and returns strview to the second one
         * @param delim Delimiter char to split on
         */
        NOINLINE strview split_second(char delim) const noexcept;

        /**
         * Gets the next strview; also advances the ptr to next token.
         * @param out Resulting string token. Only valid if result is TRUE.
         * @param delim Delimiter char between string tokens
         * @return TRUE if a token was returned, FALSE if no more tokens (no token [out]).
         */
        NOINLINE bool next(strview& out, char delim) noexcept;
        /**
         * Gets the next string token; also advances the ptr to next token.
         * @param out Resulting string token. Only valid if result is TRUE.
         * @param delims Delimiter characters between string tokens
         * @param ndelims Number of delimiters in the delims string to consider
         * @return TRUE if a token was returned, FALSE if no more tokens (no token [out]).
         */
        NOINLINE bool next(strview& out, const char* delims, int ndelims) noexcept;
        /**
         * Gets the next string token; also advances the ptr to next token.
         * @param out Resulting string token. Only valid if result is TRUE.
         * @param delims Delimiter characters between string tokens
         * @return TRUE if a token was returned, FALSE if no more tokens (no token [out]).
         */
        template<int N> NOINLINE bool next(strview& out, const char (&delims)[N]) noexcept {
            return _next_trim(out, [&delims](const char* s, int n) {
                return strcontains<N>(s, n, delims);
            });
        }
        /**
         * Same as bool next(strview& out, char delim), but returns a token instead
         */
        FINLINE strview next(char delim) noexcept {
            strview out; next(out, delim); return out;
        }
        FINLINE strview next(const char* delim, int ndelims) noexcept {
            strview out; next(out, delim, ndelims); return out;
        }
        template<int N> FINLINE strview next(const char (&delims)[N]) noexcept {
            strview out; next<N>(out, delims); return out;
        }

        /**
         * Gets the next strview and skips any empty tokens; also advances the ptr to next token.
         * @param out Resulting string token. Only valid if result is TRUE.
         * @param delim Delimiter char between string tokens
         * @return TRUE if a token was returned, FALSE if no more tokens (no token [out]).
         */
        NOINLINE bool next_skip_empty(strview& out, char delim) noexcept {
            return trim_start(delim).next(out, delim);
        }
        NOINLINE bool next_skip_empty(strview& out, const char* delims, int ndelims) noexcept {
            return trim_start(delims, ndelims).next(out, delims, ndelims);
        }
        template<int N> NOINLINE bool next_skip_empty(strview& out, const char (&delims)[N]) noexcept {
            return trim_start<N>(delims).next(out, delims);
        }
        /**
         * Same as bool next_skip_empty(strview& out, char delim), but returns a token instead
         */
        NOINLINE strview next_skip_empty(char delim) noexcept {
            return trim_start(delim).next(delim);
        }
        NOINLINE strview next_skip_empty(const char* delims, int ndelims) noexcept {
            return trim_start(delims, ndelims).next(delims, ndelims);
        }
        template<int N> NOINLINE strview next_skip_empty(const char (&delims)[N]) noexcept {
            return trim_start<N>(delims).next(delims);
        }

        /**
         * Gets the next string token; stops buffer on the identified delimiter.
         * @param out Resulting string token. Only valid if result is TRUE.
         * @param delim Delimiter char between string tokens
         * @return TRUE if a token was returned, FALSE if no more tokens (no token [out]).
         */
        NOINLINE bool next_notrim(strview& out, char delim) noexcept;
        /**
         * Gets the next string token; stops buffer on the identified delimiter.
         * @param out Resulting string token. Only valid if result is TRUE.
         * @param delims Delimiter characters between string tokens
         * @param ndelims Number of delimiters in the delims string to consider
         * @return TRUE if a token was returned, FALSE if no more tokens (no token [out]).
         */
        NOINLINE bool next_notrim(strview& out, const char* delims, int ndelims) noexcept;
        /**
         * Gets the next string token; stops buffer on the identified delimiter.
         * @param out Resulting string token. Only valid if result is TRUE.
         * @param delims Delimiter characters between string tokens
         * @return TRUE if a token was returned, FALSE if no more tokens (no token [out]).
         */
        template<int N> NOINLINE bool next_notrim(strview& out, const char (&delims)[N]) noexcept {
            return _next_notrim(out, [&delims](const char* s, int n) {
                return strcontains<N>(s, n, delims);
            });
        }
        /**
         * Same as bool next(strview& out, char delim), but returns a token instead
         */
        NOINLINE strview next_notrim(char delim) noexcept {
            strview out; next_notrim(out, delim); return out;
        }
        NOINLINE strview next_notrim(const char* delim, int ndelims) noexcept {
            strview out; next_notrim(out, delim, ndelims); return out;
        }
        template<int N> NOINLINE strview next_notrim(const char (&delims)[N]) noexcept {
            strview out;
            _next_notrim(out, [&delims](const char* s, int n) {
                return strcontains<N>(s, n, delims);
            });
            return out;
        }


        // don't forget to mark NOINLINE in the function where you call this...
        template<class SearchFn> NOINLINE bool _next_notrim(strview& out, SearchFn searchFn) noexcept
        {
            auto s = str, end = s + len;
            for (;;) { // using a loop to skip empty tokens
                if (s >= end)       // out of bounds?
                    return false;   // no more tokens available
                if (const char* p = searchFn(s, int(end - s))) {
                    out.str = s;    // writeout start/end
                    out.len = int(p - s);
                    str = p;        // stop on identified token
                    len = int(end - p);
                    return true;    // we got what we needed
                }
                out.str = s;        // writeout start/end
                out.len = int(end - s);  // 
                str = end;          // last token, set to end for debugging convenience
                len = 0;
                return true;
            }
        }

        template<class SearchFn> NOINLINE bool _next_trim(strview& out, SearchFn searchFn) noexcept
        {
            auto s = str, end = s + len;
            for (;;) { // using a loop to skip empty tokens
                if (s >= end)       // out of bounds?
                    return false;   // no more tokens available
                if (const char* p = searchFn(s, int(end - s))) {
                    out.str = s;    // writeout start/end
                    out.len = int(p - s);
                    str = p;        // stop on identified token
                    len = int(end - p);
                    if (len) { ++str; --len; }
                    return true;    // we got what we needed
                }
                out.str = s;        // writeout start/end
                out.len = int(end - s);  // 
                str = end;          // last token, set to end for debugging convenience
                len = 0;
                return true;
            }
        }

        /**
         * Template friendly conversions
         */
        void convert_to(bool& outValue)    const noexcept { outValue = to_bool();   }
        void convert_to(int& outValue)     const noexcept { outValue = to_int();    }
        void convert_to(float& outValue)   const noexcept { outValue = to_float();  }
        void convert_to(double& outValue)  const noexcept { outValue = to_double(); }
        void convert_to(string_t& outValue) const noexcept { to_string(outValue); }
        void convert_to(strview& outValue)     const noexcept { outValue = *this; }

    private:
        void skip_by_len(strview text) noexcept { skip(text.len); }
        void skip_by_len(char)         noexcept { skip(1); }

    public:
        /**
         * Calls strview::next(char delim) for each argument and calls strview::convert_to
         * in order to convert them to appropriate types.
         *
         * @code
         * strview input = "user@email.com;27;3486.37;true"_sv;
         *
         * User user;
         * input.decompose(';', user.email, user.age, user.coins, user.unlocked);
         *
         * // or an immutable version:
         * strview{input}.decompose(...);
         *
         * @endcode
         */
        template<class Delim, class T, class... Rest>
        void decompose(const Delim& delim, T& outFirst, Rest&... outRest) noexcept
        {
            if (starts_with(delim)) // this is an empty entry, just skip it;
                skip_by_len(delim);
            else
                next(delim).convert_to(outFirst);
            decompose(delim, outRest...);
        }
        template<class Delim, class T>
        void decompose(const Delim& delim, T& outFirst) noexcept
        {
            next(delim).convert_to(outFirst);
        }

        /**
         * Tries to create a substring from specified index with given length.
         * The substring will be clamped to a valid range [0 .. len-1]
         */
        NOINLINE strview substr(int index, int length) const noexcept;

        /**
         * Tries to create a substring from specified index until the end of string.
         * Substring will be empty if invalid index is given
         */
        NOINLINE strview substr(int index) const noexcept;


        /**
         * Parses next float from current strview, example: "1.0;sad0.0,'as;2.0" will parse [1.0] [0.0] [2.0]
         * @return 0.0f if there's nothing to parse or a parsed float
         */
        NOINLINE double next_double() noexcept;
        float next_float() noexcept { return (float)next_double(); }

        /** 
         * Parses next int from current Token, example: "1,asvc2,x*3" will parse [1] [2] [3]
         * @return 0 if there's nothing to parse or a parsed int
         */
        NOINLINE int next_int() noexcept;

        /** 
         * Parses next int64 from current Token, example: "1,asvc2,x*3" will parse [1] [2] [3]
         * @return 0 if there's nothing to parse or a parsed int
         */
        NOINLINE rpp::int64 next_int64() noexcept;

        /**
         * Safely chomps N chars while there is something to chomp
         */
        NOINLINE strview& skip(int nchars) noexcept;

        /**
         * Skips start of the string until the specified character is found or end of string is reached.
         * @param ch Character to skip until
         */
        NOINLINE strview& skip_until(char ch) noexcept;

        /**
         * Skips start of the string until the specified substring is found or end of string is reached.
         * @param substr Substring to skip until
         * @param sublen Length of the substring
         */
        NOINLINE strview& skip_until(const char* substr, int sublen) noexcept;

        /**
         * Skips start of the string until the specified substring is found or end of string is reached.
         * @param substr Substring to skip until
         */
        template<int SIZE> FINLINE strview& skip_until(const char (&substr)[SIZE]) noexcept
        {
            return skip_until(substr, SIZE-1);
        }


        /**
         * Skips start of the string until the specified character is found or end of string is reached.
         * The specified character itself is consumed.
         * @param ch Character to skip after
         */
        NOINLINE strview& skip_after(char ch) noexcept;

        /**
         * Skips start of the string until the specified substring is found or end of string is reached.
         * The specified substring itself is consumed.
         * @param substr Substring to skip after
         * @param sublen Length of the substring
         */
        NOINLINE strview& skip_after(const char* substr, int sublen) noexcept;

        /**
         * Skips start of the string until the specified substring is found or end of string is reached.
         * The specified substring itself is consumed.
         * @param substr Substring to skip after
         */
        template<int SIZE> FINLINE strview& skip_after(const char (&substr)[SIZE]) noexcept
        {
            return skip_after(substr, SIZE-1);
        }

        /**
         * Modifies the target string to lowercase
         * @warning The const char* will be recasted and modified!
         */
        NOINLINE strview& to_lower() noexcept;

        /**
         * Creates a copy of this strview that is in lowercase
         */
        NOINLINE string_t as_lower() const noexcept;

        /**
         * Creates a copy of this strview that is in lowercase
         */
        NOINLINE char* as_lower(char* dst) const noexcept;

        /**
         * Modifies the target string to be UPPERCASE
         * @warning The const char* will be recasted and modified!
         */
        NOINLINE strview& to_upper() noexcept;

        /**
         * Creates a copy of this strview that is in UPPERCASE
         */
        NOINLINE string_t as_upper() const noexcept;

        /**
         * Creates a copy of this strview that is in UPPERCASE
         */
        NOINLINE char* as_upper(char* dst) const noexcept;

        /**
         * Modifies the target string by replacing all chOld
         * occurrences with chNew
         * @warning The const char* will be recasted and modified!
         * @param chOld The old character to replace
         * @param chNew The new character
         */
        NOINLINE strview& replace(char chOld, char chNew) noexcept;
    }; // struct strview

    //////////////// wstrview ///////////////

#if RPP_ENABLE_UNICODE
    /**
     * @brief A minimal version of rpp::strview for UTF16 (char16_t) strings
     * 
     * We don't support std::wstring, because it has portability issues
     */
    struct ustrview
    {
        using char_t = char16_t; // compatible char_t for rpp::ustrview
        using string_t = std::u16string; // compatible std string for rpp::ustrview
        using string_view_t = std::u16string_view; // compatible std string_view for rpp::ustrview

        const char16_t* str;
        int len;

        FINLINE constexpr ustrview()                                noexcept : str{u""}, len{0} {}
        FINLINE RPP_CONSTEXPR_STRLEN ustrview(char16_t* str)        noexcept : str{str}, len{RPP_UTF16LEN(str) } {}
        FINLINE RPP_CONSTEXPR_STRLEN ustrview(const char16_t* str)  noexcept : str{str}, len{RPP_UTF16LEN(str) } {}
        FINLINE constexpr ustrview(const char16_t* str, int len)    noexcept : str{str}, len{len} {}
        FINLINE constexpr ustrview(const char16_t* str, size_t len) noexcept : str{str}, len{static_cast<int>(len)} {}
        FINLINE constexpr ustrview(const char16_t* str, const char16_t* end) noexcept : str{str}, len{static_cast<int>(end - str)} {}
        FINLINE constexpr ustrview(const void* str, const void* end) noexcept : ustrview{static_cast<const char16_t*>(str), static_cast<const char_t*>(end)} {}
        FINLINE ustrview(const string_t& s RPP_LIFETIMEBOUND) noexcept : str{s.c_str()}, len{static_cast<int>(s.length())} {}
        FINLINE ustrview(const string_view_t& s)              noexcept : str{s.data()},  len{static_cast<int>(s.length())} {}

    #if _MSC_VER
        FINLINE ustrview(const wchar_t* wstr) noexcept : str{reinterpret_cast<const char16_t*>(wstr)}, len{RPP_UTF16LEN(wstr)} {}
    #endif

        string_t to_string() const noexcept { return string_t{str, str+len}; }

        // this is implicit by design; but it may cause some unexpected conversions to std::u16string
        // main goal is to provide convenient automatic conversion:  u16string s = my_string_view;
        operator string_t() const { return string_t{str, (size_t)len}; }

        /** Clears the strview */
        FINLINE void clear() noexcept { str = u""; len = 0; }
        /** @return Length of the string */
        FINLINE constexpr int length() const noexcept { return len; }
        FINLINE constexpr int size()   const noexcept { return len; }
        /** @return TRUE if length of the string is 0 - thus the string is empty */
        FINLINE constexpr bool empty() const noexcept { return !len; }
        /** @return TRUE if string is non-empty */
        explicit FINLINE constexpr operator bool() const noexcept { return len != 0; }
        /** @return Pointer to the start of the string */
        FINLINE const char16_t* c_str() const noexcept { return str; }
        FINLINE const char16_t* data()  const noexcept { return str; }
        FINLINE const char16_t* begin() const noexcept { return str; }
        FINLINE const char16_t* end()   const noexcept { return str + len; }
        FINLINE char16_t front() const noexcept { return *str; }
        FINLINE char16_t back()  const noexcept { return str[len - 1]; }

    #if _MSC_VER
        FINLINE const wchar_t* w_str() const noexcept { return reinterpret_cast<const wchar_t*>(str); }
    #endif

        FINLINE constexpr const char16_t& operator[](int index) const noexcept { return str[index]; }

        /** @return TRUE if the strview ends with a null terminator */
        bool is_nullterm() const noexcept { return str[len] == u'\0'; }


        /** Trims the start of the string from any whitespace */
        NOINLINE ustrview& trim_start() noexcept;
        /** Trims start from this char */
        NOINLINE ustrview& trim_start(const char16_t ch) noexcept;
        NOINLINE ustrview& trim_start(const char16_t* chars, int nchars) noexcept;
        inline ustrview& trim_start(ustrview s) noexcept { return trim_start(s.str, s.len); }

        /** Trims end from this char */
        NOINLINE ustrview& trim_end(char16_t ch) noexcept;
        /** Trims the end of the string from any whitespace */
        NOINLINE ustrview& trim_end() noexcept;
        NOINLINE ustrview& trim_end(const char16_t* chars, int nchars) noexcept;
        inline ustrview& trim_end(ustrview s) noexcept { return trim_end(s.str, s.len); }

        /** Trims both start and end with whitespace */
        FINLINE ustrview& trim() noexcept { return trim_start().trim_end(); }
        /** Trims both start and end width this char*/
        FINLINE ustrview& trim(char16_t ch) noexcept { return trim_start(ch).trim_end(ch); }
        FINLINE ustrview& trim(const char16_t* chars, int nchars) noexcept { return trim_start(chars, nchars).trim_end(chars, nchars); }
        inline ustrview& trim(ustrview s) noexcept { return trim_start(s.str, s.len).trim_end(s.str, s.len); }

        /** Consumes the first character in the strview if possible. */
        FINLINE ustrview& chomp_first() noexcept { if (len) ++str;--len; return *this; }
        /** Consumes the last character in the strview if possible. */
        FINLINE ustrview& chomp_last() noexcept { if (len) --len; return *this; }

        /** Pops and returns the first character in the strview if possible. */
        FINLINE char16_t pop_front() noexcept { if (len) { char16_t ch = *str++; --len;       return ch; } return u'\0'; }
        /** Pops and returns the last character in the strview if possible. */
        FINLINE char16_t pop_back()  noexcept { if (len) { char16_t ch = str[len - 1]; --len; return ch; } return u'\0'; }

        /** Consumes the first COUNT characters in the strview String if possible. */
        FINLINE ustrview& chomp_first(int count) noexcept {
            int n = count < len ? count : len;
            str += n; len -= n;
            return *this;
        }
        /** Consumes the last COUNT characters in the strview String if possible. */
        FINLINE ustrview& chomp_last(int count) noexcept {
            len -= (count < len ? count : len);
            return *this;
        }


        /**
         * Copies this str[len] string into a C-string array
         * However, if THIS string is null terminated, this operation is a NOP and behaves like c_str()
         */
        NODISCARD NOINLINE const char16_t* to_cstr(char16_t* buf, int max) const noexcept;
        template<int N>
        NODISCARD FINLINE const char16_t* to_cstr(char16_t (&buf)[N]) const noexcept { return to_cstr(buf, N); }


        /** @return TRUE if this strview starts with the specified string */
        FINLINE bool starts_with(const char16_t* s, int length) const noexcept {
            return len >= length && strequals(str, s, length);
        }
        template<int N> FINLINE bool starts_with(const char16_t (&s)[N]) const noexcept {
            return len >= (N - 1) && strequals<N>(str, s);
        }
        FINLINE bool starts_with(const ustrview& s) const noexcept { return starts_with(s.str, s.len); }
        FINLINE bool starts_with(char16_t ch)       const noexcept { return len && *str == ch; }


        /** @return TRUE if this strview starts with IGNORECASE of the specified string */
        FINLINE bool starts_withi(const char16_t* s, int length) const noexcept {
            return len >= length && strequalsi(str, s, length);
        }
        template<int N> FINLINE bool starts_withi(const char16_t (&s)[N]) const noexcept {
            return len >= (N - 1) && strequalsi<N>(str, s);
        }
        FINLINE bool starts_withi(const ustrview& s) const noexcept { return starts_withi(s.str, s.len); }
        FINLINE bool starts_withi(char16_t ch)       const noexcept { return len && ::toupper(*str) == ::toupper(ch); }


        /** @return TRUE if the strview ends with the specified string */
        FINLINE bool ends_with(const char16_t* s, int slen) const noexcept {
            return len >= slen && strequals(str + len - slen, s, slen);
        }
        template<int N> FINLINE bool ends_with(const char16_t (&s)[N]) const noexcept {
            return len >= (N - 1) && strequals<N>(str + len - (N - 1), s);
        }
        FINLINE bool ends_with(const ustrview& s) const noexcept { return ends_with(s.str, s.len); }
        FINLINE bool ends_with(char16_t ch)      const noexcept { return len && str[len - 1] == ch; }


        /** @return TRUE if this strview ends with IGNORECASE of the specified string */
        FINLINE bool ends_withi(const char16_t* s, int slen) const noexcept {
            return len >= slen && strequalsi(str + len - slen, s, slen);
        }
        template<int N> FINLINE bool ends_withi(const char16_t (&s)[N]) const noexcept {
            return len >= (N - 1) && strequalsi<N>(str + len - (N - 1), s);
        }
        FINLINE bool ends_withi(const ustrview& s) const noexcept { return ends_withi(s.str, s.len); }
        FINLINE bool ends_withi(char16_t ch)       const noexcept { return len && ::toupper(str[len - 1]) == ::toupper(ch); }


        FINLINE bool equals(const char16_t* s, int length) const noexcept { return len == length && strequals(str, s, length); }

        template<int SIZE> FINLINE bool operator==(const char16_t (&s)[SIZE]) const noexcept { return equals(s, SIZE-1); }
        template<int SIZE> FINLINE bool operator!=(const char16_t (&s)[SIZE]) const noexcept { return !equals(s, SIZE-1); }
        FINLINE bool operator==(const string_t& s) const noexcept { return  equals(s.data(), (int)s.size()); }
        FINLINE bool operator!=(const string_t& s) const noexcept { return !equals(s.data(), (int)s.size()); }
        FINLINE bool operator==(const string_view_t& s) const noexcept { return  equals(s.data(), (int)s.size()); }
        FINLINE bool operator!=(const string_view_t& s) const noexcept { return !equals(s.data(), (int)s.size()); }
        FINLINE bool operator==(const ustrview& s) const noexcept { return  equals(s.str, s.len); }
        FINLINE bool operator!=(const ustrview& s) const noexcept { return !equals(s.str, s.len); }
        FINLINE bool operator==(char16_t* s) const noexcept { return  strequals(s, str, len); }
        FINLINE bool operator!=(char16_t* s) const noexcept { return !strequals(s, str, len); }
        FINLINE bool operator==(char16_t ch) const noexcept { return len == 1 && *str == ch; }
        FINLINE bool operator!=(char16_t ch) const noexcept { return len != 1 || *str != ch; }


        /** @brief Compares this strview to string data */
        NOINLINE int compare(const char16_t* s, int n) const noexcept;
        NOINLINE int compare(const char16_t* s) const noexcept;
        FINLINE int compare(const ustrview& b) const noexcept { return compare(b.str, b.len); }
        FINLINE int compare(const string_t& b) const noexcept { return compare(b.c_str(),(int)b.size()); }
        
        FINLINE bool operator< (const ustrview& s) const noexcept { return compare(s.str, s.len) < 0; }
        FINLINE bool operator> (const ustrview& s) const noexcept { return compare(s.str, s.len) > 0; }
        FINLINE bool operator<=(const ustrview& s) const noexcept { return compare(s.str, s.len) <= 0; }
        FINLINE bool operator>=(const ustrview& s) const noexcept { return compare(s.str, s.len) >= 0; }
        FINLINE bool operator< (const string_t& s) const noexcept { return compare(s.c_str(),(int)s.size()) < 0; }
        FINLINE bool operator> (const string_t& s) const noexcept { return compare(s.c_str(),(int)s.size()) > 0; }
        FINLINE bool operator<=(const string_t& s) const noexcept { return compare(s.c_str(),(int)s.size()) <= 0; }
        FINLINE bool operator>=(const string_t& s) const noexcept { return compare(s.c_str(),(int)s.size()) >= 0; }
        template<int SIZE> FINLINE bool operator< (const char16_t(&s)[SIZE]) const noexcept {return compare(s,SIZE-1) < 0;}
        template<int SIZE> FINLINE bool operator> (const char16_t(&s)[SIZE]) const noexcept {return compare(s,SIZE-1) > 0;}
        template<int SIZE> FINLINE bool operator<=(const char16_t(&s)[SIZE]) const noexcept {return compare(s,SIZE-1) <= 0;}
        template<int SIZE> FINLINE bool operator>=(const char16_t(&s)[SIZE]) const noexcept {return compare(s,SIZE-1) >= 0;}

        /** @return Pointer to char if found using reverse search, NULL otherwise */
        NOINLINE const char16_t* rfind(char16_t c) const noexcept;

        /**
         * Forward searches for any of the specified chars
         * @return Pointer to char if found, NULL otherwise.
         */
        const char16_t* findany(const char16_t* chars, int n) const noexcept;
        template<int N> FINLINE const char16_t* findany(const char16_t (&chars)[N]) const noexcept {
            return findany(chars, N - 1);
        }

        /**
         * Reverse searches for any of the specified chars
         * @return Pointer to char if found, NULL otherwise.
         */
        const char16_t* rfindany(const char16_t* chars, int n) const noexcept;
        template<int N> FINLINE const char16_t* rfindany(const char16_t (&chars)[N]) const noexcept {
            return rfindany(chars, N - 1);
        }

        /**
         * Tries to create a substring from specified index with given length.
         * The substring will be clamped to a valid range [0 .. len-1]
         */
        NOINLINE ustrview substr(int index, int length) const noexcept;

        /**
         * Tries to create a substring from specified index until the end of string.
         * Substring will be empty if invalid index is given
         */
        NOINLINE ustrview substr(int index) const noexcept;

        /**
         * Gets the next strview; also advances the ptr to next token.
         * @param out Resulting string token. Only valid if result is TRUE.
         * @param delim Delimiter char between string tokens
         * @return TRUE if a token was returned, FALSE if no more tokens (no token [out]).
         */
        NOINLINE bool next(ustrview& out, char16_t delim) noexcept;
        /**
         * Gets the next string token; also advances the ptr to next token.
         * @param out Resulting string token. Only valid if result is TRUE.
         * @param delims Delimiter characters between string tokens
         * @param ndelims Number of delimiters in the delims string to consider
         * @return TRUE if a token was returned, FALSE if no more tokens (no token [out]).
         */
        NOINLINE bool next(ustrview& out, const char16_t* delims, int ndelims) noexcept;
        /**
         * Gets the next string token; also advances the ptr to next token.
         * @param out Resulting string token. Only valid if result is TRUE.
         * @param delims Delimiter characters between string tokens
         * @return TRUE if a token was returned, FALSE if no more tokens (no token [out]).
         */
        template<int N> NOINLINE bool next(ustrview& out, const char16_t (&delims)[N]) noexcept {
            return _next_trim(out, [&delims](const char16_t* s, int n) {
                return strcontains<N>(s, n, delims);
            });
        }
        /**
         * Same as bool next(strview& out, char delim), but returns a token instead
         */
        FINLINE ustrview next(char16_t delim) noexcept {
            ustrview out; next(out, delim); return out;
        }
        FINLINE ustrview next(const char16_t* delim, int ndelims) noexcept {
            ustrview out; next(out, delim, ndelims); return out;
        }
        template<int N> FINLINE ustrview next(const char16_t (&delims)[N]) noexcept {
            ustrview out; next<N>(out, delims); return out;
        }

        template<class SearchFn> NOINLINE bool _next_trim(ustrview& out, SearchFn searchFn) noexcept
        {
            auto s = str, end = s + len;
            for (;;) { // using a loop to skip empty tokens
                if (s >= end)       // out of bounds?
                    return false;   // no more tokens available
                if (const char16_t* p = searchFn(s, int(end - s))) {
                    out.str = s;    // writeout start/end
                    out.len = int(p - s);
                    str = p;        // stop on identified token
                    len = int(end - p);
                    if (len) { ++str; --len; }
                    return true;    // we got what we needed
                }
                out.str = s;        // writeout start/end
                out.len = int(end - s);  // 
                str = end;          // last token, set to end for debugging convenience
                len = 0;
                return true;
            }
        }
    };
#endif // RPP_ENABLE_UNICODE


    template<class TStrView>
    struct strview_traits { };

    template<>
    struct strview_traits<strview>
    {
        using strview_t = strview;
        using string_t = string;
    };
#if RPP_ENABLE_UNICODE
    template<>
    struct strview_traits<ustrview>
    {
        using strview_t = ustrview;
        using string_t = ustring;
    };
#endif

    template<typename T>
    concept StringViewType = std::same_as<std::decay_t<T>, rpp::strview>
                        #if RPP_ENABLE_UNICODE
                          || std::same_as<std::decay_t<T>, rpp::ustrview>
                        #endif
                          ;

    //////////////// strview literal operator ///////////////

    inline namespace literals
    {
        using namespace std::string_literals;

        inline constexpr strview operator "" _sv(const char* str, std::size_t len) noexcept
        {
            return strview{ str, (int)len };
        }
    #if RPP_ENABLE_UNICODE
        inline constexpr ustrview operator "" _sv(const char16_t* str, std::size_t len) noexcept
        {
            return ustrview{ str, len };
        }
    #endif // RPP_ENABLE_UNICODE
    }

    //////////////// handy stream operators /////////////////
    
    inline strview& operator>>(strview& s, float& out) noexcept
    {
        out = s.next_float();
        return s;
    }
    inline strview& operator>>(strview& s, double& out) noexcept
    {
        out = s.next_double();
        return s;
    }
    inline strview& operator>>(strview& s, int& out) noexcept
    {
        out = s.next_int();
        return s;
    }
    inline strview& operator>>(strview& s, unsigned& out) noexcept
    {
        out = (unsigned)s.next_int();
        return s;
    }
    inline strview& operator>>(strview& s, rpp::int64& out) noexcept
    {
        out = s.next_int64();
        return s;
    }
    inline strview& operator>>(strview& s, rpp::uint64& out) noexcept
    {
        out = (rpp::uint64)s.next_int64();
        return s;
    }
    inline strview& operator>>(strview& s, bool& out) noexcept
    {
        if      (s.equalsi("true")){ s.skip(4); out = true; }
        else if (s.equalsi("yes")) { s.skip(3); out = true; }
        else if (s.equalsi("on"))  { s.skip(2); out = true; }
        else if (s.equalsi("1"))   { s.skip(1); out = true; }
        else out = false;
        return s;
    }
    inline strview& operator>>(strview& s, string& out) noexcept
    {
        (void)s.to_string(out);
        s.skip(s.len);
        return s;
    }

    //////////////// string concatenate operators /////////////////
    
    inline string& operator+=(string& a, const strview& b)
    {
        return a.append(b.str, size_t(b.len));
    }
    inline string operator+(const strview& a, const strview& b)
    {
        string str;
        size_t al = size_t(a.len), bl = size_t(b.len);
        str.reserve(al + bl);
        str.append(a.str, al).append(b.str, bl);
        return str;
    }
    inline string operator+(const string& a, const strview& b){ return strview{a} + b; }
    inline string operator+(const strview& a, const string& b){ return a + strview{b}; }
    inline string operator+(const char* a, const strview& b)  { return strview{a, RPP_UTF8LEN(a)} + b; }
    inline string operator+(const strview& a, const char* b)  { return a + strview{b, RPP_UTF8LEN(b)}; }
    inline string operator+(const strview& a, char b)         { return a + strview{&b, 1}; }
    inline string operator+(char a, const strview& b)         { return strview{&a, 1} + b; }
    inline string&& operator+(string&& a, const strview& b)   { return std::move(a.append(b.str, (size_t)b.len)); }

#if RPP_ENABLE_UNICODE
    inline ustring& operator+=(ustring& a, const ustrview& b)
    {
        return a.append(b.str, size_t(b.len));
    }
    inline ustring operator+(const ustrview& a, const ustrview& b)
    {
        ustring str;
        size_t al = size_t(a.len), bl = size_t(b.len);
        str.reserve(al + bl);
        str.append(a.str, al).append(b.str, bl);
        return str;
    }
    inline ustring operator+(const ustring& a, const ustrview& b) { return ustrview{a} + b; }
    inline ustring operator+(const ustrview& a, const ustring& b) { return a + ustrview{b}; }
    inline ustring operator+(const char16_t* a, const ustrview& b){ return ustrview{a, RPP_UTF16LEN(a)} + b; }
    inline ustring operator+(const ustrview& a, const char16_t* b){ return a + ustrview{b, RPP_UTF16LEN(b)}; }
    inline ustring operator+(const ustrview& a, char16_t b)       { return a + ustrview{&b, 1}; }
    inline ustring operator+(char16_t a, const ustrview& b)       { return ustrview{&a, 1} + b; }
    inline ustring&& operator+(ustring&& a, const ustrview& b)    { return std::move(a.append(b.str, (size_t)b.len)); }
#endif // RPP_ENABLE_UNICODE

    //////////////// optimized string join /////////////////

    RPPAPI string concat(const strview& a, const strview& b);
    RPPAPI string concat(const strview& a, const strview& b, const strview& c);
    RPPAPI string concat(const strview& a, const strview& b, const strview& c, const strview& d);
    RPPAPI string concat(const strview& a, const strview& b, const strview& c, const strview& d, const strview& e);

#if RPP_ENABLE_UNICODE
    RPPAPI ustring concat(const ustrview& a, const ustrview& b);
    RPPAPI ustring concat(const ustrview& a, const ustrview& b, const ustrview& c);
    RPPAPI ustring concat(const ustrview& a, const ustrview& b, const ustrview& c, const ustrview& d);
    RPPAPI ustring concat(const ustrview& a, const ustrview& b, const ustrview& c, const ustrview& d, const ustrview& e);
#endif // RPP_ENABLE_UNICODE

    //////////////// string compare operators /////////////////

    inline bool operator< (const string& a,const strview& b) noexcept { return strview{a}.compare(b.str, b.len) < 0; }
    inline bool operator> (const string& a,const strview& b) noexcept { return strview{a}.compare(b.str, b.len) > 0; }
    inline bool operator<=(const string& a,const strview& b) noexcept { return strview{a}.compare(b.str, b.len) <= 0; }
    inline bool operator>=(const string& a,const strview& b) noexcept { return strview{a}.compare(b.str, b.len) >= 0; }
    inline bool operator==(const string& a,const strview& b) noexcept { return strview{a}.equals(b.str, b.len); }
    inline bool operator!=(const string& a,const strview& b) noexcept { return !strview{a}.equals(b.str, b.len); }
    
    inline bool operator< (const char* a,const strview& b) noexcept { return strview{a}.compare(b.str, b.len) < 0; }
    inline bool operator> (const char* a,const strview& b) noexcept { return strview{a}.compare(b.str, b.len) > 0; }
    inline bool operator<=(const char* a,const strview& b) noexcept { return strview{a}.compare(b.str, b.len) <= 0; }
    inline bool operator>=(const char* a,const strview& b) noexcept { return strview{a}.compare(b.str, b.len) >= 0; }
    inline bool operator==(const char* a,const strview& b) noexcept { return strview{a}.equals(b.str, b.len); }
    inline bool operator!=(const char* a,const strview& b) noexcept { return !strview{a}.equals(b.str, b.len); }

    // unicode string compare operators
#if RPP_ENABLE_UNICODE
    inline bool operator< (const ustring& a,const ustrview& b) noexcept { return ustrview{a}.compare(b.str, b.len) < 0; }
    inline bool operator> (const ustring& a,const ustrview& b) noexcept { return ustrview{a}.compare(b.str, b.len) > 0; }
    inline bool operator<=(const ustring& a,const ustrview& b) noexcept { return ustrview{a}.compare(b.str, b.len) <= 0; }
    inline bool operator>=(const ustring& a,const ustrview& b) noexcept { return ustrview{a}.compare(b.str, b.len) >= 0; }
    inline bool operator==(const ustring& a,const ustrview& b) noexcept { return ustrview{a}.equals(b.str, b.len); }
    inline bool operator!=(const ustring& a,const ustrview& b) noexcept { return !ustrview{a}.equals(b.str, b.len); }
    
    inline bool operator< (const char16_t* a,const ustrview& b) noexcept { return ustrview{a}.compare(b.str, b.len) < 0; }
    inline bool operator> (const char16_t* a,const ustrview& b) noexcept { return ustrview{a}.compare(b.str, b.len) > 0; }
    inline bool operator<=(const char16_t* a,const ustrview& b) noexcept { return ustrview{a}.compare(b.str, b.len) <= 0; }
    inline bool operator>=(const char16_t* a,const ustrview& b) noexcept { return ustrview{a}.compare(b.str, b.len) >= 0; }
    inline bool operator==(const char16_t* a,const ustrview& b) noexcept { return ustrview{a}.equals(b.str, b.len); }
    inline bool operator!=(const char16_t* a,const ustrview& b) noexcept { return !ustrview{a}.equals(b.str, b.len); }
#endif // RPP_ENABLE_UNICODE

    ////////////////////////////////////////////////////////////////////////////////


    /**
     * A POD version of strview for use in unions
     */
    struct strview_
    {
        const char* str;
        int len;
        FINLINE operator strview()        const noexcept { return strview{str, len}; }
        FINLINE operator strview&()             noexcept { return *(rpp::strview*)this; }
        FINLINE operator const strview&() const noexcept { return *(rpp::strview*)this; }
        FINLINE strview* operator->()     const noexcept { return  (rpp::strview*)this; }
    };


    ////////////////////////////////////////////////////////////////////////////////


    /**
     * @returns TRUE if the string appears to contain an UTF-8 start byte sequence.
     * It will not validate if the string is a valid UTF-8 sequence.
     */
    bool is_likely_utf8(const char* str, int len) noexcept;
    FINLINE bool is_likely_utf8(strview str) noexcept { return is_likely_utf8(str.str, str.len); }

#if RPP_ENABLE_UNICODE
    /**
     * @brief Converts a UTF-16 String to a UTF-8 String
     */
    string to_string(const char16_t* utf16, int utf16len = -1) noexcept;
    FINLINE string to_string(ustrview utf16) noexcept { return to_string(utf16.str, utf16.len); }
    /**
     * @brief Buffer style conversion for less allocations
     * @returns -1 on failure, [0..out_max-1] on success
     */
    int to_string(char* out, int out_max, const char16_t* utf16, int utf16len = -1) noexcept;
#ifdef __cpp_char8_t // fundamental type char8_t since C++20
    FINLINE int to_string(char8_t* out, int out_max, const char16_t* utf16, int utf16len = -1) noexcept {
        return to_string((char*)out, out_max, utf16, utf16len);
    }
#endif

    /**
     * @brief Converts a UTF-8 String to a Wide String
     */
    ustring to_ustring(const char* utf8, int utf8len = -1) noexcept;
    FINLINE ustring to_ustring(strview utf8) noexcept { return to_ustring(utf8.str, utf8.len); }
    /**
     * @brief Buffer style conversion for less allocations
     * @returns -1 on failure, [0..out_max-1] on success
     */
    int to_ustring(char16_t* out, int out_max, const char* utf8, int utf8len = -1) noexcept;
    FINLINE int to_ustring(char16_t* out, int out_max, const char8_t* utf8, int utf8len = -1) noexcept {
        return to_ustring(out, out_max, (const char*)utf8, utf8len);
    }
#endif // RPP_ENABLE_UNICODE

    ////////////////////////////////////////////////////////////////////////////////


    /**
     * Converts a string into its lowercase form
     */
    RPPAPI char* to_lower(char* str, int len) noexcept;

    /**
     * Converts a string into its uppercase form
     */
    RPPAPI char* to_upper(char* str, int len) noexcept;

    /**
     * Converts an string into its lowercase form
     */
    RPPAPI string& to_lower(string& str) noexcept;

    /**
     * Converts an string into its uppercase form
     */
    RPPAPI string& to_upper(string& str) noexcept;

    /**
     * Replaces characters of 'chOld' with 'chNew' inside the specified string
     */
    RPPAPI char* replace(char* str, int len, char chOld, char chNew) noexcept;

    /**
     * Replaces characters of 'chOld' with 'chNew' inside this string
     */
    RPPAPI string& replace(string& str, char chOld, char chNew) noexcept;


    ////////////////////////////////////////////////////////////////////////////////


    /**
     * Parses an input string buffer for individual lines
     * The line is returned trimmed of any \r or \n
     *
     *  This is also an example on how to implement your own custom parsers using the strview structure
     */
    class RPPAPI line_parser
    {
    protected:
        strview buffer;
    public:
        FINLINE line_parser(const strview& buffer)         noexcept : buffer{buffer} {}
        FINLINE line_parser(const char* data, int size)    noexcept : buffer{data, data + size} {}
        FINLINE line_parser(const char* data, size_t size) noexcept : buffer{data, data + size} {}

        /**
         * Reads next line from the base buffer and advances its pointers.
         * The line is returned trimmed of any \r or \n. Empty lines are not skipped.
         *
         * @param out The output line that is read. Only valid if TRUE is returned.
         * @return Reads the next line. If no more lines, FALSE is returned.
         **/
        NOINLINE bool read_line(strview& out) noexcept;

        // same as read_line(strview&), but returns a strview object instead of a bool
        NOINLINE strview read_line() noexcept;
    };


    ////////////////////////////////////////////////////////////////////////////////


    /**
     * Parses an input string buffer for 'Key=Value' pairs.
     * The pairs are returned one by one with 'read_next'.
     * 
     * Ex file:
     * # comment line
     * key1 = value1\n
     * key2=value2\r\n
     *  key3 = \t value3 \n
     *
     * This is also an example on how to implement your own custom parsers using strview
     */
    class RPPAPI keyval_parser
    {
    protected:
        strview buffer;
    public:
        FINLINE keyval_parser(const strview& buffer)         noexcept : buffer{buffer} {}
        FINLINE keyval_parser(const char* data, int size)    noexcept : buffer{data, data + size} {}
        FINLINE keyval_parser(const char* data, size_t size) noexcept : buffer{data, data + size} {}

        /**
         * Reads next line from the base buffer and advances its pointers.
         * The line is returned trimmed of any \r or \n.
         * Empty or whitespace lines are skipped.
         * Comment lines starting with ; are skipped.
         * Comments at the end of a line are trimmed off.
         *
         * @param out The output line that is read. Only valid if TRUE is returned.
         * @return Reads the next line. If no more lines, FALSE is returned.
         */
        NOINLINE bool read_line(strview& out) noexcept;

        /**
         * Reads the next key-value pair from the buffer and advances its position
         * @param key Resulting key (only valid if return value is TRUE)
         * @param value Resulting value (only valid if return value is TRUE)
         * @return TRUE if a Key-Value pair was parsed
         */
        NOINLINE bool read_next(strview& key, strview& value) noexcept;
    };


    ////////////////////////////////////////////////////////////////////////////////


    /**
     * Parses an input string buffer for balanced-parentheses structures
     * The lines are returned one by one with 'read_keyval'.
     * 
     * @example:
     *  key value {
     *    key { }
     *    key value {
     *      key value
     *      key value
     *      key value
     *    }
     *  }
     *
     */
    class RPPAPI bracket_parser
    {
    protected:
        strview buffer;
    public:
        int depth;
        int line; // current line

        NOINLINE bracket_parser(const void* data, int len) noexcept;
        FINLINE bracket_parser(const strview& s)             noexcept : bracket_parser{s.str, s.len}   {}
        FINLINE bracket_parser(const void* data, size_t len) noexcept : bracket_parser{data, (int)len} {}

        /**
         * Reads the next line from the buffer and advances its position
         * @param key Resulting line key (only valid if return value != -1)
         * @param value Resulting line value (only valid if return value != -1)
         * @return Resulting depth of the parser. Default top-level depth is 0.
         */
        NOINLINE int read_keyval(strview& key, strview& value) noexcept;
        
        /** 
         * @brief Peeks at the next interesting token and returns its value
         * @note Whitespace and comments will be skipped
         * @note If buffer is empty, '\0' is returned.
         */
        NOINLINE char peek_next() const noexcept;
    };

    // support for "debugging.h"
    template<>
    struct __wrap<strview>
    {
        FINLINE static const char* w(const strview& arg) noexcept { return arg.to_cstr(); }
    };

    template<>
    struct __wrap<std::string_view>
    {
        // convert std::string_view to rpp::strview and then call the specialized to_cstr()
        FINLINE static const char* w(const std::string_view& arg) noexcept
        {
            return strview{arg.data(), arg.size()}.to_cstr();
        }
    };
} // namespace rpp


/////////////////////// std::hash to use strview in maps ///////////////////////

#if RPP_CLANG_LLVM
#  include <utility> // std::hash
#else
// forward declare std::hash
namespace std { template<class T> struct hash; }
#endif
namespace std
{
    template<>
    struct hash<rpp::strview>
    {
        size_t operator()(const rpp::strview& s) const noexcept
        {
        #if _MSC_VER
            return std::_Hash_array_representation<char>(s.str, static_cast<size_t>(s.len));
        #elif __clang__ && _LIBCPP_STD_VER
            return std::__do_string_hash(s.str, s.str+s.len);
        #else
            return std::hash<std::string_view>{}(std::string_view{s.str, static_cast<size_t>(s.len)});
        #endif
        }
    };
}

////////////////////////////////////////////////////////////////////////////////

#if _MSC_VER
#pragma warning(pop) // pop nameless struct/union warning
#endif
