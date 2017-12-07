#pragma once
#ifndef RPP_STRVIEW_H
#define RPP_STRVIEW_H
/**
 * String Tokenizer/View, Copyright (c) 2014 - Jorma Rebane
 */
#ifdef _LIBCPP_STD_VER
#  define _HAS_STD_BYTE (_LIBCPP_STD_VER > 14)
#elif !defined(_HAS_STD_BYTE)
#  define _HAS_STD_BYTE 0
#endif
#ifndef __cplusplus
  #error <rpp/strview.h> requires C++14 or higher
#endif
#include <cstring>   // C string utilities
#include <string>     // compatibility with std::string
#include <vector>     // std::vector for split
#include <unordered_map> // std::unordered_map for to_string extensions
#include <iostream>   // std::ostream for << compatibility
#include <functional> // std::hash
#include <memory>     // std::shared_ptr

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
#pragma warning(push)
#pragma warning(disable:4201) // nameless struct/union warning
#endif

namespace rpp
{
    using namespace std; // we love std; you should too.

    #ifndef RPP_BASIC_INTEGER_TYPEDEFS
    #define RPP_BASIC_INTEGER_TYPEDEFS
        #if !_HAS_STD_BYTE
            typedef unsigned char  byte;
        #endif
        typedef unsigned short     ushort;
        typedef unsigned int       uint;
        typedef unsigned long      ulong;
        typedef long long          int64;
        typedef unsigned long long uint64;
    #endif

    //// @note Some functions get inlined too aggressively, leading to some serious code bloat
    ////       Need to hint the compiler to take it easy ^_^'
    #ifndef NOINLINE
        #ifdef _MSC_VER
            #define NOINLINE __declspec(noinline)
        #else
            #define NOINLINE __attribute__((noinline))
        #endif
    #endif

    //// @note Some strong hints that some functions are merely wrappers, so should be forced inline
    #ifndef FINLINE
        #ifdef _MSC_VER
            #define FINLINE __forceinline
        #elif __APPLE__
            #define FINLINE inline __attribute__((always_inline))
        #else
            #define FINLINE __attribute__((always_inline))
        #endif
    #endif
    
    #ifndef RPPAPI
        #if _MSC_VER
            #define RPPAPI __declspec(dllexport)
        #else // clang/gcc
            #define RPPAPI __attribute__((visibility("default")))
        #endif
    #endif

/////////// Small string optimized search functions (low loop setup latency, but bad with large strings)

// This is same as memchr, but optimized for very small control strings
// Retains string literal array length information
    template<int N> FINLINE bool strcontains(const char(&str)[N], char ch) {
        for (int i = 0; i < N; ++i)
            if (str[i] == ch) return true;
        return false;
    }
    /**
     * @note Same as strpbrk, except we're not dealing with 0-term strings
     * @note This function is optimized for 4-8 char str and 3-4 char control.
     * @note Retains string literal array length information
     */
    template<int N> FINLINE const char* strcontains(const char* str, int nstr, const char(&control)[N]) {
        for (; nstr; --nstr, ++str)
            if (strcontains<N>(control, *str))
                return str; // done
        return 0; // not found
    }
    template<int N> NOINLINE bool strequals(const char* s1, const char(&s2)[N]) {
        for (int i = 0; i < (N - 1); ++i)
            if (s1[i] != s2[i]) return false; // not equal.
        return true;
    }
    template<int N> NOINLINE bool strequalsi(const char* s1, const char(&s2)[N]) {
        for (int i = 0; i < (N - 1); ++i)
            if (::toupper(s1[i]) != ::toupper(s2[i])) return false; // not equal.
        return true;
    }


    // This is same as memchr, but optimized for very small control strings
    bool strcontains(const char* str, int len, char ch);
    /**
     * @note Same as strpbrk, except we're not dealing with 0-term strings
     * @note This function is optimized for 4-8 char str and 3-4 char control.
     */
    const char* strcontains(const char* str, int nstr, const char* control, int ncontrol);
    NOINLINE bool strequals(const char* s1, const char* s2, int len);
    NOINLINE bool strequalsi(const char* s1, const char* s2, int len);






    /**
     * C-locale specific, simplified atof that also outputs the end of parsed string
     * @param str Input string, e.g. "-0.25" / ".25", etc.. '+' is not accepted as part of the number
     * @param len Length of the string to parse
     * @return Parsed float
     */
    double to_double(const char* str, int len, const char** end = nullptr);
    inline double to_double(const char* str, const char** end = nullptr)
    {
        return to_double(str, 64, end);
    }

    /**
     * Fast locale agnostic atoi
     * @param str Input string, e.g. "-25" or "25", etc.. '+' is not accepted as part of the number
     * @param len Length of the string to parse
     * @param end (optional) Destination pointer for end of parsed string. Can be NULL.
     * @return Parsed int
     */
    int to_int(const char* str, int len, const char** end = nullptr);
    inline int to_int(const char* str, const char** end = nullptr)
    {
        return to_int(str, 32, end);
    }

    /**
     * Fast locale agnostic atoi
     * @param str Input string, e.g. "-25" or "25", etc.. '+' is not accepted as part of the number
     *            HEX syntax is supported: 0xBA or 0BA will parse hex values instead of regular integers
     * @param len Length of the string to parse
     * @param end (optional) Destination pointer for end of parsed string. Can be NULL.
     * @return Parsed int
     */
    int to_inthx(const char* str, int len, const char** end = nullptr);
    inline int to_inthx(const char* str, const char** end = nullptr)
    {
        return to_inthx(str, 32, end);
    }


    /**
     * C-locale specific, simplified ftoa() that prints pretty human-readable floats
     * @param buffer Destination buffer assumed to be big enough. 32 bytes is more than enough.
     * @param value Float value to convert to string
     * @return Length of the string
     */
    int _tostring(char* buffer, double value);
    inline int _tostring(char* buffer, float value)
    {
        return _tostring(buffer, (double)value);
    }


    /**
     * Fast locale agnostic itoa
     * @param buffer Destination buffer assumed to be big enough. 32 bytes is more than enough.
     * @param value Integer value to convert to string
     * @return Length of the string
     */
    int _tostring(char* buffer, int    value);
    int _tostring(char* buffer, int64  value);
    int _tostring(char* buffer, uint   value);
    int _tostring(char* buffer, uint64 value);

    inline int _tostring(char* buffer, rpp::byte value) { return _tostring(buffer, (uint)value); }
    inline int _tostring(char* buffer, short value)     { return _tostring(buffer, (int)value);  }
    inline int _tostring(char* buffer, ushort value)    { return _tostring(buffer, (uint)value); }




    struct strview_vishelper // VC++ visualization helper
    {
        const char* str;
        int len;
    };




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
    struct strview
    {
    #ifdef _MSC_VER
        union {
            struct {
                const char* str; // start of string
                int len;         // length of string
            };
            strview_vishelper v;	// VC++ visualization helper
        };
    #else
        const char* str; // start of string
        int len;         // length of string
    #endif

        FINLINE constexpr strview()                            : str(""),  len(0) {}
        FINLINE strview(char* str)                             : str(str), len((int)strlen(str)) {}
        FINLINE strview(const char* str)                       : str(str), len((int)strlen(str)) {}
        FINLINE constexpr strview(const char* str, int len)    : str(str), len(len)              {}
        FINLINE constexpr strview(const char* str, size_t len) : str(str), len((int)len)         {}
        FINLINE strview(const char* str, const char* end)      : str(str), len(int(end-str))     {}
        FINLINE strview(const void* str, const void* end)      : strview((const char*)str, (const char*)end) {}
        FINLINE strview(const string& s)                       : str(s.c_str()), len((int)s.length()) {}
        template<class StringT>
        FINLINE strview(const StringT& str) : str(str.c_str()), len((int)str.length()) {}
        FINLINE const char& operator[](int index) const { return str[index]; }


        strview(strview&& t)                 = default;
        strview(const strview& t)            = default;
        strview& operator=(strview&& t)      = default;
        strview& operator=(const strview& t) = default;


        /** Creates a new string from this string-strview */
        FINLINE string& to_string(string& out) const { return out.assign(str, (size_t)len); }
        string to_string() const { return string{str, (size_t)len}; }

        // this is implicit by design; but it may cause some unexpected conversions to std::string
        // main goal is to provide convenient automatic conversion:  string s = my_string_view;
        operator string() const { return string{str, (size_t)len}; }

        /** 
         * Copies this str[len] string into a C-string array
         * However, if THIS string is null terminated, this operation is a NOP and behaves like c_str()
         */
        NOINLINE const char* to_cstr(char* buf, int max) const;
        template<int N> 
        FINLINE const char* to_cstr(char (&buf)[N]) const { return to_cstr(buf, N); }
        /** 
         * Copies this str[len] into a max of 512 byte static thread-local C-string array 
         * Result is only valid until next call to this method
         * However, if THIS string is null terminated, this operation is a NOP and behaves like c_str()
         */
        NOINLINE const char* to_cstr() const;

        /** Parses this strview as an integer */
        FINLINE int to_int() const { return rpp::to_int(str, len); }
        /** Parses this strview as a HEX integer ('0xff' or '0ff' or 'ff') */
        FINLINE int to_int_hex() const { return to_inthx(str, len); }
        /** Parses this strview as a long */
        FINLINE long to_long()     const { return (long)rpp::to_int(str, len); }
        /** Parses this strview as a float */
        FINLINE float to_float()   const { return (float)rpp::to_double(str, len); }
        /** Parses this strview as a double */
        FINLINE double to_double() const { return rpp::to_double(str, len); }
        /** Parses this strview as a bool */
        bool to_bool() const;

        /** Clears the strview */
        FINLINE void clear() { str = ""; len = 0; }
        /** @return Length of the string */
        FINLINE constexpr int length() const  { return len; }
        FINLINE constexpr int size()   const  { return len; }
        /** @return TRUE if length of the string is 0 - thus the string is empty */
        FINLINE constexpr bool empty() const { return !len; }
        /** @return TRUE if string is non-empty */
        explicit FINLINE constexpr operator bool() const { return len != 0; }
        /** @return Pointer to the start of the string */
        FINLINE const char* c_str() const { return str; }
        FINLINE const char* data()  const { return str; }
        FINLINE const char* begin() const { return str; }
        FINLINE const char* end()   const { return str + len; }
        FINLINE char front() const { return *str; }
        FINLINE char back()  const { return str[len - 1]; }
        /** @return TRUE if the strview is only whitespace: " \t\r\n"  */
        NOINLINE bool is_whitespace() const;
        /** @return TRUE if the strview ends with a null terminator */
        FINLINE bool is_nullterm() const { return str[len] == '\0'; }

        /** Trims the start of the string from any whitespace */
        NOINLINE strview& trim_start();
        /** Trims start from this char */
        NOINLINE strview& trim_start(char ch);
        NOINLINE strview& trim_start(const char* chars, int nchars);
        /* Optimized noinline version for specific character sequences */
        template<int N> NOINLINE strview& trim_start(const char (&chars)[N]) { 
            auto s = str;
            auto n = len;
            for (; n && strcontains<N>(chars, *s); ++s, --n)
                ;
            str = s; len = n; // write result
            return *this;
        }
        inline strview& trim_start(strview s) { return trim_start(s.str, s.len); }

        /** Trims end from this char */
        NOINLINE strview& trim_end(char ch);
        /** Trims the end of the string from any whitespace */
        NOINLINE strview& trim_end();
        NOINLINE strview& trim_end(const char* chars, int nchars);
        /* Optimized noinline version for specific character sequences */
        template<int N> NOINLINE strview& trim_end(const char (&chars)[N]) {
            auto n = len;
            auto e = str + n;
            for (; n && strcontains<N>(chars, *--e); --n)
                ;
            len = n; // write result
            return *this;
        }
        inline strview& trim_end(strview s) { return trim_end(s.str, s.len); }

        /** Trims both start and end with whitespace */
        FINLINE strview& trim() { return trim_start().trim_end(); }
        /** Trims both start and end width this char*/
        FINLINE strview& trim(char ch) { return trim_start(ch).trim_end(ch); }
        FINLINE strview& trim(const char* chars, int nchars) { return trim_start(chars, nchars).trim_end(chars, nchars); }
        /** Trims both start and end with any of the given chars */
        template<int N> FINLINE strview& trim(const char (&chars)[N]) { 
            return trim_start(chars, N-1).trim_end(chars, N-1);
        }
        inline strview& trim(strview s) { return trim_start(s.str, s.len).trim_end(s.str, s.len); }

        /** Consumes the first character in the strview if possible. */
        FINLINE strview& chomp_first() { if (len) ++str;--len; return *this; }
        /** Consumes the last character in the strview if possible. */
        FINLINE strview& chomp_last()  { if (len) --len; return *this; }

        /** Pops and returns the first character in the strview if possible. */
        FINLINE char pop_front() { if (len) { char ch = *str++; --len;       return ch; } return '\0'; }
        /** Pops and returns the last character in the strview if possible. */
        FINLINE char pop_back()  { if (len) { char ch = str[len - 1]; --len; return ch; } return '\0'; }

        /** Consumes the first COUNT characters in the strview String if possible. */
        FINLINE strview& chomp_first(int count) { 
            int n = count < len ? count : len;
            str += n; len -= n;
            return *this;
        }
        /** Consumes the last COUNT characters in the strview String if possible. */
        FINLINE strview& chomp_last(int count) {
            len -= (count < len ? count : len);
            return *this; 
        }

        /** @return TRUE if the strview contains this char */
        FINLINE bool contains(char c) const { return memchr(str, c, (size_t)len) != nullptr; }
        /** @return TRUE if the strview contains any of the chars */
        NOINLINE bool contains_any(const char* chars, int nchars) const {
            return strcontains(str, len, chars, nchars) != nullptr;
        }
        template<int N> FINLINE bool contains_any(const char (&chars)[N]) const {
            return strcontains<N>(str, len, chars) != nullptr;
        }

        /** @return Pointer to char if found, NULL otherwise */
        FINLINE const char* find(char c) const { return (const char*)memchr(str, c, (size_t)len); }
        /** @return Pointer to start of substring if found, NULL otherwise */
        NOINLINE const char* find(const char* substr, int len) const;
        FINLINE const char* find(const strview& substr) const { 
            return find(substr.str, substr.len); 
        }
        template<int N> FINLINE const char* find(const char (&substr)[N]) const { 
            return find(substr, N - 1); 
        }


        /** @return Pointer to char if found using reverse search, NULL otherwise */
        NOINLINE const char* rfind(char c) const;

        /** 
         * Forward searches for any of the specified chars
         * @return Pointer to char if found, NULL otherwise.
         */
        const char* findany(const char* chars, int n) const;
        template<int N> FINLINE const char* findany(const char (&chars)[N]) const {
            return findany(chars, N - 1);
        }

        /** 
         * Reverse searches for any of the specified chars
         * @return Pointer to char if found, NULL otherwise.
         */
        const char* rfindany(const char* chars, int n) const;
        template<int N> FINLINE const char* rfindany(const char (&chars)[N]) const {
            return rfindany(chars, N - 1);
        }


        /**
         * Count number of occurrances of this character inside the strview bounds
         */
        int count(char ch) const;

        
        int indexof(char ch) const;
        
        // reverse iterating indexof
        int rindexof(char ch) const;
        
        int indexofany(const char* chars, int n) const;
        template<int N> FINLINE int indexofany(const char (&chars)[N]) const {
            return indexofany(chars, N - 1);
        }

        /** @return TRUE if this strview starts with the specified string */
        FINLINE bool starts_with(const char* s, int length) const {
            return len >= length && strequals(str, s, length);
        }
        template<int N> FINLINE bool starts_with(const char (&s)[N]) const { 
            return len >= (N - 1) && strequals<N>(str, s);
        }
        FINLINE bool starts_with(const strview& s)  const { return starts_with(s.str, s.len); }
        FINLINE bool starts_with(char ch) const { return len && *str == ch; }


        /** @return TRUE if this strview starts with IGNORECASE of the specified string */
        FINLINE bool starts_withi(const char* s, int length) const {
            return len >= length && strequalsi(str, s, length);
        }
        template<int N> FINLINE bool starts_withi(const char (&s)[N]) const { 
            return len >= (N - 1) && strequalsi<N>(str, s);
        }
        FINLINE bool starts_withi(const strview& s) const { return starts_withi(s.str, s.len); }
        FINLINE bool starts_withi(char ch) const { return len && ::toupper(*str) == ::toupper(ch); }


        /** @return TRUE if the strview ends with the specified string */
        FINLINE bool ends_with(const char* s, int slen) const {
            return len >= slen && strequals(str + len - slen, s, slen);
        }
        template<int N> FINLINE bool ends_with(const char (&s)[N]) const { 
            return len >= (N - 1) && strequals<N>(str + len - (N - 1), s);
        }
        FINLINE bool ends_with(const strview s)  const { return ends_with(s.str, s.len); }
        FINLINE bool ends_with(char ch)          const { return len && str[len-1] == ch; }


        /** @return TRUE if this strview ends with IGNORECASE of the specified string */
        FINLINE bool ends_withi(const char* s, int slen) const {
            return len >= slen && strequalsi(str + len - slen, s, slen);
        }
        template<int N> FINLINE bool ends_withi(const char (&s)[N]) const { 
            return len >= (N - 1) && strequalsi<N>(str + len - (N - 1), s);
        }
        FINLINE bool ends_withi(const strview s) const { return ends_withi(s.str, s.len); }
        FINLINE bool ends_withi(char ch) const { return len && ::toupper(str[len-1]) == ::toupper(ch); }


        /** @return TRUE if this strview equals the specified string */
        FINLINE bool equals(const char* s, int length) const { return len == length && strequals(str, s, length); }
        template<int N>
        FINLINE bool equals(const char (&s)[N]) const { return len == (N-1) && strequals<N>(str, s); }
        FINLINE bool equals(const strview& s)   const { return equals(s.str, s.len);                 }

        /** @return TRUE if this strview equals IGNORECASE the specified string */
        FINLINE bool equalsi(const char* s, int length) const { return len == length && strequalsi(str, s, length); }
        template<int N>
        FINLINE bool equalsi(const char (&s)[N]) const { return len == (N-1) && strequalsi<N>(str, s); }
        FINLINE bool equalsi(const strview& s)   const { return equalsi(s.str, s.len);                 }

        template<int SIZE> FINLINE bool operator==(const char(&s)[SIZE]) const { return equals<SIZE>(s); }
        template<int SIZE> FINLINE bool operator!=(const char(&s)[SIZE]) const { return !equals<SIZE>(s); }
        FINLINE bool operator==(const string& s)  const { return  equals(s); }
        FINLINE bool operator!=(const string& s)  const { return !equals(s); }
        FINLINE bool operator==(const strview& s) const { return  equals(s.str, s.len); }
        FINLINE bool operator!=(const strview& s) const { return !equals(s.str, s.len); }
        FINLINE bool operator==(char* s) const { return  strequals(s, str, len); }
        FINLINE bool operator!=(char* s) const { return !strequals(s, str, len); }
        FINLINE bool operator==(char ch) const { return len == 1 && *str == ch; }
        FINLINE bool operator!=(char ch) const { return len != 1 || *str != ch; }

        /** @brief Compares this strview to string data */
        NOINLINE int compare(const char* s, int n) const;
        NOINLINE int compare(const char* s) const;
        FINLINE int compare(const strview& b) const { return compare(b.str, b.len); }
        FINLINE int compare(const string& b)  const { return compare(b.c_str(),(int)b.size()); }
        
        FINLINE bool operator<(const strview& s) const { return compare(s.str, s.len) < 0; }
        FINLINE bool operator>(const strview& s) const { return compare(s.str, s.len) > 0; }
        FINLINE bool operator<(const string& s)  const { return compare(s.c_str(),(int)s.size()) < 0; }
        FINLINE bool operator>(const string& s)  const { return compare(s.c_str(),(int)s.size()) > 0; }
        template<int SIZE> FINLINE bool operator<(const char(&s)[SIZE]) const {return compare(s,SIZE-1)<0;}
        template<int SIZE> FINLINE bool operator>(const char(&s)[SIZE]) const {return compare(s,SIZE-1)>0;}
        
        /**
         * Splits the string into TWO and returns strview to the first one
         * @param delim Delimiter char to split on
         */
        NOINLINE strview split_first(char delim) const;

        /**
         * Splits the string into TWO and returns strview to the first one
         * @param substr Substring to split with
         * @param n Length of the substring
         */
        NOINLINE strview split_first(const char* substr, int n) const;
        template<int N> FINLINE strview split_first(const char(&substr)[N]) {
            return split_first(substr, N-1);
        }

        /**
         * Splits the string into TWO and returns strview to the second one
         * @param delim Delimiter char to split on
         */
        NOINLINE strview split_second(char delim) const;

        /**
         * Gets the next strview; also advances the ptr to next token.
         * @param out Resulting string token. Only valid if result is TRUE.
         * @param delim Delimiter char between string tokens
         * @return TRUE if a token was returned, FALSE if no more tokens (no token [out]).
         */
        NOINLINE bool next(strview& out, char delim);
        /**
         * Gets the next string token; also advances the ptr to next token.
         * @param out Resulting string token. Only valid if result is TRUE.
         * @param delims Delimiter characters between string tokens
         * @param ndelims Number of delimiters in the delims string to consider
         * @return TRUE if a token was returned, FALSE if no more tokens (no token [out]).
         */
        NOINLINE bool next(strview& out, const char* delims, int ndelims);
        /**
         * Gets the next string token; also advances the ptr to next token.
         * @param out Resulting string token. Only valid if result is TRUE.
         * @param delims Delimiter characters between string tokens
         * @return TRUE if a token was returned, FALSE if no more tokens (no token [out]).
         */
        template<int N> NOINLINE bool next(strview& out, const char (&delims)[N]) {
            bool result = _next_notrim(out, [&delims](const char* s, int n) {
                return strcontains<N>(s, n, delims);
            });
            if (result && len) { ++str; --len; } // trim match
            return result;
        }
        /**
         * Same as bool next(strview& out, char delim), but returns a token instead
         */
        FINLINE strview next(char delim) {
            strview out; next(out, delim); return out;
        }
        FINLINE strview next(const char* delim, int ndelims) {
            strview out; next(out, delim, ndelims); return out;
        }
        template<int N> FINLINE strview next(const char (&delims)[N]) {
            strview out; next<N>(out, delims); return out;
        }

        /**
         * Gets the next string token; stops buffer on the identified delimiter.
         * @param out Resulting string token. Only valid if result is TRUE.
         * @param delim Delimiter char between string tokens
         * @return TRUE if a token was returned, FALSE if no more tokens (no token [out]).
         */
        NOINLINE bool next_notrim(strview& out, char delim);
        /**
         * Gets the next string token; stops buffer on the identified delimiter.
         * @param out Resulting string token. Only valid if result is TRUE.
         * @param delims Delimiter characters between string tokens
         * @param ndelims Number of delimiters in the delims string to consider
         * @return TRUE if a token was returned, FALSE if no more tokens (no token [out]).
         */
        NOINLINE bool next_notrim(strview& out, const char* delims, int ndelims);
        /**
         * Gets the next string token; stops buffer on the identified delimiter.
         * @param out Resulting string token. Only valid if result is TRUE.
         * @param delims Delimiter characters between string tokens
         * @return TRUE if a token was returned, FALSE if no more tokens (no token [out]).
         */
        template<int N> NOINLINE bool next_notrim(strview& out, const char (&delims)[N]) {
            return _next_notrim(out, [&delims](const char* s, int n) {
                return strcontains<N>(s, n, delims);
            });
        }
        /**
         * Same as bool next(strview& out, char delim), but returns a token instead
         */
        FINLINE strview next_notrim(char delim) {
            strview out; next_notrim(out, delim); return out;
        }
        FINLINE strview next_notrim(const char* delim, int ndelims) {
            strview out; next_notrim(out, delim, ndelims); return out;
        }
        template<int N> FINLINE strview next_notrim(const char (&delims)[N]) {
            strview out;
            _next_notrim(out, [&delims](const char* s, int n) {
                return strcontains<N>(s, n, delims);
            });
            return out;
        }


        // don't forget to mark NOINLINE in the function where you call this...
        template<class SearchFn> FINLINE bool _next_notrim(strview& out, SearchFn searchFn)
        {
            auto s = str, end = s + len;
            for (;;) { // using a loop to skip empty tokens
                if (s >= end)       // out of bounds?
                    return false;   // no more tokens available
                if (const char* p = searchFn(s, int(end - s))) {
                    if (s == p) {   // this is an empty token?
                        ++s;        // increment search string
                        continue;   // try again
                    }
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

        /**
         * Template friendly conversions
         */
        inline void convertTo(bool& outValue)    const { outValue = to_bool();   }
        inline void convertTo(int& outValue)     const { outValue = to_int();    }
        inline void convertTo(float& outValue)   const { outValue = to_float();  }
        inline void convertTo(double& outValue)  const { outValue = to_double(); }
        inline void convertTo(string& outValue)  const { to_string(outValue);    }
        inline void convertTo(strview& outValue) const { outValue = *this;       }
        
    private:
        inline void skipByLength(strview text) { skip(text.len); }
        inline void skipByLength(char)         { skip(1); }
    public:
        /**
         * Calls strview::next(char delim) for each argument and calls strview::convertTo to
         * convert them to appropriate types.
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
        void decompose(const Delim& delim, T& outFirst, Rest&... outRest)
        {
            if (starts_with(delim)) // this is an empty entry, just skip it;
            {
                skipByLength(delim);
            }
            else
            {
                strview token = next(delim);
                token.convertTo(outFirst);
            }
            decompose(delim, outRest...);
        }
        template<class Delim, class T>
        void decompose(const Delim& delim, T& outFirst)
        {
            strview token = next(delim);
            token.convertTo(outFirst);
        }

        /**
         * Tries to create a substring from specified index with given length.
         * The substring will be clamped to a valid range [0 .. len-1]
         */
        NOINLINE strview substr(int index, int length) const;

        /**
         * Tries to create a substring from specified index until the end of string.
         * Substring will be empty if invalid index is given
         */
        NOINLINE strview substr(int index) const;


        /**
         * Parses next float from current strview, example: "1.0;sad0.0,'as;2.0" will parse [1.0] [0.0] [2.0]
         * @return 0.0f if there's nothing to parse or a parsed float
         */
        NOINLINE double next_double();
        inline float next_float()
        {
            return (float)next_double();
        }

        /** 
         * Parses next int from current Token, example: "1,asvc2,x*3" will parse [1] [2] [3]
         * @return 0 if there's nothing to parse or a parsed int
         */
        NOINLINE int next_int();

        /**
         * Safely chomps N chars while there is something to chomp
         */
        NOINLINE strview& skip(int nchars);

        /**
         * Skips start of the string until the specified character is found or end of string is reached.
         * @param ch Character to skip until
         */
        NOINLINE strview& skip_until(char ch);

        /**
         * Skips start of the string until the specified substring is found or end of string is reached.
         * @param substr Substring to skip until
         * @param sublen Length of the substring
         */
        NOINLINE strview& skip_until(const char* substr, int sublen);

        /**
         * Skips start of the string until the specified substring is found or end of string is reached.
         * @param substr Substring to skip until
         */
        template<int SIZE> FINLINE strview& skip_until(const char (&substr)[SIZE])
        {
            return skip_until(substr, SIZE-1);
        }


        /**
         * Skips start of the string until the specified character is found or end of string is reached.
         * The specified character itself is consumed.
         * @param ch Character to skip after
         */
        NOINLINE strview& skip_after(char ch);

        /**
         * Skips start of the string until the specified substring is found or end of string is reached.
         * The specified substring itself is consumed.
         * @param substr Substring to skip after
         * @param len Length of the substring
         */
        NOINLINE strview& skip_after(const char* substr, int len);

        /**
         * Skips start of the string until the specified substring is found or end of string is reached.
         * The specified substring itself is consumed.
         * @param substr Substring to skip after
         */
        template<int SIZE> FINLINE strview& skip_after(const char (&substr)[SIZE])
        {
            return skip_after(substr, SIZE-1);
        }

        /**
         * Modifies the target string to lowercase
         * @warning The const char* will be recasted and modified!
         */
        NOINLINE strview& to_lower();

        /**
         * Creates a copy of this strview that is in lowercase
         */
        NOINLINE string as_lower() const;

        /**
         * Creates a copy of this strview that is in lowercase
         */
        NOINLINE char* as_lower(char* dst) const;

        /**
         * Modifies the target string to be UPPERCASE
         * @warning The const char* will be recasted and modified!
         */
        NOINLINE strview& to_upper();

        /**
         * Creates a copy of this strview that is in UPPERCASE
         */
        NOINLINE string as_upper() const;

        /**
         * Creates a copy of this strview that is in UPPERCASE
         */
        NOINLINE char* as_upper(char* dst) const;

        /**
         * Modifies the target string by replacing all chOld
         * occurrences with chNew
         * @warning The const char* will be recasted and modified!
         * @param chOld The old character to replace
         * @param chNew The new character
         */
        NOINLINE strview& replace(char chOld, char chNew);
    };

    //////////////// strview literal operator ///////////////

    constexpr strview operator "" _sv(const char* str, std::size_t len)
    {
        return { str, (int)len };
    }

    //////////////// handy stream operators /////////////////
    
    inline strview& operator>>(strview& s, float& out)
    {
        out = s.next_float();
        return s;
    }
    inline strview& operator>>(strview& s, double& out)
    {
        out = s.next_double();
        return s;
    }
    inline strview& operator>>(strview& s, int& out)
    {
        out = s.next_int();
        return s;
    }
    inline strview& operator>>(strview& s, unsigned& out)
    {
        out = (unsigned)s.next_int();
        return s;
    }
    inline strview& operator>>(strview& s, bool& out)
    {
        if      (s.equalsi("true")){ s.skip(4); out = true; }
        else if (s.equalsi("yes")) { s.skip(3); out = true; }
        else if (s.equalsi("on"))  { s.skip(2); out = true; }
        else if (s.equalsi("1"))   { s.skip(1); out = true; }
        else out = false;
        return s;
    }
    inline strview& operator>>(strview& s, std::string& out)
    {
        s.to_string(out);
        s.skip(s.len);
        return s;
    }
    
    inline ostream& operator<<(ostream& stream, const strview& s)
    {
        return stream.write(s.str, s.len);
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

    inline string operator+(const string& a, const strview& b){ return strview{a} + b;}
    inline string operator+(const strview& a, const string& b){ return a + strview{b};}
    inline string operator+(const char* a, const strview& b)  { return strview{a, strlen(a)} + b; }
    inline string operator+(const strview& a, const char* b)  { return a + strview{b, strlen(b)}; }
    inline string operator+(const strview& a, char b)      { return a + strview{&b, 1}; }
    inline string operator+(char a, const strview& b)      { return strview{&a, 1} + b; }
    inline string&& operator+(string&& a,const strview& b) { return move(a.append(b.str, (size_t)b.len)); }

    //////////////// optimized string join /////////////////

    string join(const strview& a, const strview& b);
    string join(const strview& a, const strview& b, const strview& c);
    string join(const strview& a, const strview& b, const strview& c, const strview& d);
    string join(const strview& a, const strview& b, const strview& c, const strview& d, const strview& e);

    //////////////// string compare operators /////////////////

    inline bool operator< (const string& a,const strview& b) {return strview(a) <  b;}
    inline bool operator> (const string& a,const strview& b) {return strview(a) >  b;}
    inline bool operator==(const string& a,const strview& b) {return strview(a) == b;}
    inline bool operator!=(const string& a,const strview& b) {return strview(a) != b;}
    
    inline bool operator< (const char* a,const strview& b){return strncmp(a, b.str, (size_t)b.len) <  0;}
    inline bool operator> (const char* a,const strview& b){return strncmp(a, b.str, (size_t)b.len) >  0;}
    inline bool operator==(const char* a,const strview& b){return strncmp(a, b.str, (size_t)b.len) == 0;}
    inline bool operator!=(const char* a,const strview& b){return strncmp(a, b.str, (size_t)b.len) != 0;}

    ////////////////////////////////////////////////////////////////////////////////


    /**
     * A POD version of strview for use in unions
     */
    struct strview_
    {
        union {
            struct {
                const char* str;
                int len;
            };
            strview_vishelper v;	// VC++ visualization helper
        };
        FINLINE operator strview()              { return strview{str, len}; }
        FINLINE operator strview&()             { return *(rpp::strview*)this; }
        FINLINE operator const strview&() const { return *(rpp::strview*)this; }
        FINLINE strview* operator->()     const { return  (rpp::strview*)this; }
    };


    ////////////////////////////////////////////////////////////////////////////////


    /**
     * Converts a string into its lowercase form
     */
    char* to_lower(char* str, int len);

    /**
     * Converts a string into its uppercase form
     */
    char* to_upper(char* str, int len);

    /**
     * Converts an std::string into its lowercase form
     */
    string& to_lower(string& str);

    /**
     * Converts an std::string into its uppercase form
     */
    string& to_upper(string& str);

    /**
     * Replaces characters of 'chOld' with 'chNew' inside the specified string
     */
    char* replace(char* str, int len, char chOld, char chNew);

    /**
     * Replaces characters of 'chOld' with 'chNew' inside this std::string
     */
    string& replace(string& str, char chOld, char chNew);


    ////////////////////////////////////////////////////////////////////////////////


    /**
     * Parses an input string buffer for individual lines
     * The line is returned trimmed of any \r or \n
     *
     *  This is also an example on how to implement your own custom parsers using the strview structure
     */
    class line_parser
    {
    protected:
        strview buffer;
    public:
        FINLINE line_parser(const strview& buffer)         : buffer(buffer) {}
        FINLINE line_parser(const char* data, int size)    : buffer(data, data + size) {}
        FINLINE line_parser(const char* data, size_t size) : buffer(data, data + size) {}

        /**
         * Reads next line from the base buffer and advances its pointers.
         * The line is returned trimmed of any \r or \n. Empty lines are not skipped.
         *
         * @param out The output line that is read. Only valid if TRUE is returned.
         * @return Reads the next line. If no more lines, FALSE is returned.
         **/
        NOINLINE bool read_line(strview& out);

        // same as read_line(strview&), but returns a strview object instead of a bool
        NOINLINE strview read_line();
    };

    /**
     * Executes specified function for each line in the specified file
     * @param buffer UTF8 data buffer to tokenize
     * @param func Lambda function:   
     *                    bool func(token line)
     *                       -- return false to break early, otherwise return true
     * @return Number of lines processed
     */
    template<class Func> int for_each_buffer_line(const strview& buffer, Func func)
    {
        line_parser parser = buffer;
        int n = 0;
        strview line;
        while (parser.read_line(line)) {
            ++n;
            if (func(line) == false)
                return n;
        }
        return n;
    }


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
    class keyval_parser
    {
    protected:
        strview buffer;
    public:
        FINLINE keyval_parser(const strview& buffer)         : buffer(buffer) {}
        FINLINE keyval_parser(const char* data, int size)    : buffer(data, data + size) {}
        FINLINE keyval_parser(const char* data, size_t size) : buffer(data, data + size) {}

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
        NOINLINE bool read_line(strview& out);

        /**
         * Reads the next key-value pair from the buffer and advances its position
         * @param key Resulting key (only valid if return value is TRUE)
         * @param value Resulting value (only valid if return value is TRUE)
         * @return TRUE if a Key-Value pair was parsed
         */
        NOINLINE bool read_next(strview& key, strview& value);
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
    class bracket_parser
    {
    protected:
        strview buffer;
    public:
        int depth;
        int line; // current line

        NOINLINE bracket_parser(const void* data, int len);
        FINLINE bracket_parser(const strview& s)             : bracket_parser(s.str, s.len)   {}
        FINLINE bracket_parser(const void* data, size_t len) : bracket_parser(data, (int)len) {}

        /**
         * Reads the next line from the buffer and advances its position
         * @param key Resulting line key (only valid if return value != -1)
         * @param value Resulting line value (only valid if return value != -1)
         * @return Resulting depth of the parser. Default top-level depth is 0.
         */
        NOINLINE int read_keyval(strview& key, strview& value);
        
        /** 
         * @brief Peeks at the next interesting token and returns its value
         * @note Whitespace and comments will be skipped
         * @note If buffer is empty, '\0' is returned.
         */
        NOINLINE char peek_next() const;
    };


    ////////////////////////////////////////////////////////////////////////////////

    /**
     * Forward all Misc to_string calls from rpp namespace to std::to_string
     */
    template<class T> std::string to_string(const T& object)
    {
        return std::to_string(object);
    }

    template<class T> std::string to_string(const T* object)
    {
        return object ? to_string(*object) : "null"s;
    }

    /**
     * Always null terminated version of stringstream, which is compatible with strview
     * Not intended for moving or copying
     */
    struct string_buffer
    {
        static constexpr int SIZE = 512;
        char* ptr;
        int   len = 0;
        int   cap = SIZE;
        char  buf[SIZE];

        // controls the separator for generic template write calls
        // the default value " " will turn write("brown", "fox"); into "brown fox"
        // setting it to "..." will turn write("brown", "fox"); into "brown...fox"
        strview separator = " "_sv;

        FINLINE string_buffer() noexcept { ptr = buf; buf[0] = '\0'; }
        FINLINE explicit string_buffer(strview text) noexcept : string_buffer() {
            write(text);
        }
        ~string_buffer() noexcept;

        string_buffer(string_buffer&&)                 = delete;
        string_buffer(const string_buffer&)            = delete;
        string_buffer& operator=(string_buffer&&)      = delete;
        string_buffer& operator=(const string_buffer&) = delete;

        int size() const { return len; }
        const char* c_str() const { return ptr; }
        const char* data()  const { return ptr; }
        strview     view()  const { return { ptr, len }; }
        string       str()  const { return { ptr, ptr+len }; }

        explicit operator bool()    const { return len > 0; }
        explicit operator strview() const { return { ptr, len }; }

        void clear();
        void reserve(int count) noexcept;
        void writef(const char* format, ...);

        void write(const string_buffer& sb)
        {
            write(sb.view());
        }
        template<class T> void write(const T& value)
        {
            write(to_string(value));
        }
        template<class T> void write(const T* ptr)
        {
            if (ptr == nullptr) {
                write("null");
                return;
            }
            write("*{"); write(*ptr); write('}');
        }
        template<class T> void write(T* ptr)                 { write((const T*)ptr); }
        template<class T> void write(const weak_ptr<T>& p)   { write(p.lock()); }
        template<class T> void write(const shared_ptr<T>& p) { write(p.get());  }

        template<int N> 
        void write(const char (&value)[N]) { write(strview{ value }); }
        void write(const string& value)    { write(strview{ value }); }
        void write(const char* value)      { write(strview{ value }); }

        void write(const strview& s);
        void write(const char& value);

        void write(bool value)   { write(value ? "true"_sv : "false"_sv); }
        void write(rpp::byte bv) { reserve(4);  len += _tostring(&ptr[len], bv);    }
        void write(short value)  { reserve(8);  len += _tostring(&ptr[len], value); }
        void write(ushort value) { reserve(8);  len += _tostring(&ptr[len], value); }
        void write(int value)    { reserve(16); len += _tostring(&ptr[len], value); }
        void write(uint value)   { reserve(16); len += _tostring(&ptr[len], value); }
        void write(int64 value)  { reserve(32); len += _tostring(&ptr[len], value); }
        void write(uint64 value) { reserve(32); len += _tostring(&ptr[len], value); }
        void write(float value)  { reserve(32); len += _tostring(&ptr[len], value); }
        void write(double value) { reserve(48); len += _tostring(&ptr[len], value); }

        /**
         * Stringifies and appends the input arguments one by one, filling gaps with delimiter
         * @see string_buffer::delimiter (default = ' ')
         * Ex: write("test:", 10, 20.1f);  --> "test: 10 20.1"
         */
        template<class T, class... Args> void write(const T& first, const Args&... args)
        {
            write(first); write(separator); write(args...);
        }

        void writeln() { write('\n'); }

        template<class T> void writeln(const T& value)
        {
            write(value); writeln();
        }
        template<class T, class... Args> void writeln(const T& first, const Args&... args)
        {
            write(first, args...); writeln();
        }

        ////////////////////////////////////////////////////////////////////////////////////////////

        template<class T> void prettyprint(const T& value) {
            write(to_string(value));
        }
        template<class T> void prettyprint(const T* ptr)
        {
            if (ptr == nullptr) {
                write("null");
                return;
            }
            write("*{"); prettyprint(*ptr); write('}');
        }
        template<class T> void prettyprint(T* ptr)                 { prettyprint((const T*)ptr); }
        template<class T> void prettyprint(const weak_ptr<T>& p)   { prettyprint(p.lock());  }
        template<class T> void prettyprint(const shared_ptr<T>& p) { prettyprint(p.get());   }
        void prettyprint(const string& value) {
            write('"'); write(value); write('"'); 
        }
        void prettyprint(const strview& value) {
            write('"'); write(value); write('"'); 
        }
        void prettyprint(const char& value) {
            write('\''); write(value); write('\'');
        }
        template<class K, class V> void prettyprint(const K& key, const V& value) {
            prettyprint(key); write(": "); prettyprint(value);
        }
        template<class K, class V> void prettyprint(const pair<K, V>& pair) {
            prettyprint(pair.first); write(": "); prettyprint(pair.second);
        }

        template<typename T, template<class,class...> class C, class... Args>
        void prettyprint(const C<T,Args...>& container, bool newLineSeparator = true)
        {
            int count = (int)container.size();
            if (count == 0) return write("{}");
            if (count > 4) {
                write('['); write(count); write("] = { ");
            } else {
                write("{ ");
            }
            if (newLineSeparator) write('\n');
            int i = 0;
            for (const auto& item : container) {
                if (newLineSeparator) write("  ");
                prettyprint(item);
                if (++i < count) write(", ");
                if (newLineSeparator) write('\n');
            }
            write(" }");
            if (newLineSeparator) write('\n');
        }
        
        /**
         * Similar to write(...), but performs prettyprint formatting to each argument
         */
        template<class T, class... Args> void prettyprint(const T& first, const Args&... args)
        {
            prettyprint(first); write(separator); prettyprint(args...);
        }
        template<class T, class... Args> void prettyprintln(const T& first, const Args&... args)
        {
            prettyprint(first, args...); writeln();
        }
    };
    
    template<class T> inline string_buffer& operator<<(string_buffer& sb, const T& value) {
        sb.write(value);
        return sb;
    }

    ////////////////////////////////////////////////////////////////////////////////

    int print(FILE* file, strview value);
    int print(FILE* file, char value);
    int print(FILE* file, rpp::byte value);
    int print(FILE* file, short value);
    int print(FILE* file, ushort value);
    int print(FILE* file, int value);
    int print(FILE* file, uint value);
    int print(FILE* file, int64 value);
    int print(FILE* file, uint64 value);

    int print(strview value);
    int print(char value);
    int print(rpp::byte value);
    int print(short value);
    int print(ushort value);
    int print(int value);
    int print(uint value);
    int print(int64 value);
    int print(uint64 value);

    int println(FILE* file);
    int println();

    template<class T> int println(FILE* file, const T& value)
    {
        return print(value) + println(file);
    }
    template<class T> FINLINE int println(const T& value)
    {
        return println(stdout, value);
    }

    /**
     * Stringifies and appends the input arguments one by one, filling gaps with spaces, similar to Python print()
     * Ex: print("test:", 10, 20.1f);  --> "test: 10 20.1"
     */
    template<class T, class... Args> int print(FILE* file, const T& first, const Args&... args)
    {
        string_buffer buf;
        buf.write(first, args...);
        return (int)fwrite(buf.ptr, (size_t)buf.len, 1, file);
    }
    template<class T, class... Args> int print(const T& first, const Args&... args)
    {
        return print(stdout, first, args...);
    }

    /**
     * Stringifies and appends the input arguments one by one with an endline, filling gaps with spaces, similar to Python print()
     * Ex: print("test:", 10, 20.1f);  --> "test: 10 20.1\n"
     */
    template<class T, class... Args> int println(FILE* file, const T& first, const Args&... args)
    {
        string_buffer buf;
        buf.writeln(first, args...);
        return (int)fwrite(buf.ptr, (size_t)buf.len, 1, file);
    }
    template<class T, class... Args> int println(const T& first, const Args&... args)
    {
        return println(stdout, first, args...);
    }
    
    #if DEBUG
        #define debug_println(...) rpp::println(__VA_ARGS__)
    #else
        #define debug_println(...)
    #endif

    /**
     * Stringifies and appends the input arguments one by one, filling gaps with spaces, similar to Python print()
     * Ex: sprint("test:", 10, 20.1f);  --> "test: 10 20.1"
     */
    template<class T, class... Args> inline string sprint(const T& first, const Args&... args)
    {
        string_buffer buf;
        buf.write(first, args...);
        return buf.str();
    }
    template<class T, class... Args> inline string sprintln(const T& first, const Args&... args)
    {
        string_buffer buf;
        buf.writeln(first, args...);
        return buf.str();
    }

    ////////////////////////////////////////////////////////////////////////////////

} // namespace rpp

namespace std
{
    /////////////////////// std::hash to use strview in maps ///////////////////////

    template<> struct hash<rpp::strview>
    {
        size_t operator()(const rpp::strview& s) const
        {
            #if INTPTR_MAX == INT64_MAX // 64-bit
                static_assert(sizeof(size_t) == 8, "Expected 64-bit build");
                constexpr size_t FNV_offset_basis = 14695981039346656037ULL;
                constexpr size_t FNV_prime = 1099511628211ULL;
            #elif INTPTR_MAX == INT32_MAX // 32-bit
                static_assert(sizeof(size_t) == 4, "Expected 32-bit build");
                constexpr size_t FNV_offset_basis = 2166136261U;
                constexpr size_t FNV_prime = 16777619U;
            #endif
            const char* p = s.str;
            size_t value = FNV_offset_basis;
            for (auto e = p + s.len; p < e; ++p) {
                value ^= (size_t)*p;
                value *= FNV_prime;
            }
            return value;
        }
    };

    /////////////////////// useful container tostr extensions //////////////////////

    /** @return "true" or "false" string */
    string to_string(bool trueOrFalse) noexcept;

    /**
     * @return std::string from cstring. If cstr == nullptr, returns empty string ""
     */
    string to_string(const char* cstr) noexcept;

    /** 
     *  @brief Outputs a linear container like vector<string>{"hello","world"} as:
     *  @code
     *  [2] = {
     *    "hello",
     *    "world"
     *  }
     *  @endcode
     *  
     *  Outputs a map like  unordered_map<strview,strview>{ {"key","value"}, {"name","john"} } as:
     *  @code
     *  [2] = {
     *    "key": "value",
     *    "name": "john"
     *  }
     *  @endcode
     *  
     *  With newLineSeparator = false the output is:
     *  @code
     *  [2] = { "hello", "world" }
     *  [2] = { "key": "value", "name": "john" }
     *  @endcode
     */
    template<class T, template<class,class...> class C, class... Args>
    string to_string(const C<T,Args...>& container, bool newLineSeparator = true) noexcept
    {
        rpp::string_buffer sb; sb.prettyprint(container, newLineSeparator); return sb.str();
    }
    template<class T> string to_string(const shared_ptr<T>& p) noexcept
    {
        rpp::string_buffer sb; sb.prettyprint(p); return sb.str();
    }
    template<class T> string to_string(const weak_ptr<T>& p) noexcept
    {
        rpp::string_buffer sb; sb.prettyprint(p); return sb.str();
    }

    template<class T, class Alloc>
    ostream& operator<<(ostream& os, const vector<T,Alloc>& v) noexcept
    {
        return os << to_string(v, true);
    }

    template<class K, class V, class Hash, class Equal, class Alloc>
    ostream& operator<<(ostream& os, const unordered_map<K,V,Hash,Equal,Alloc>& map) noexcept
    {
        return os << to_string(map, true);
    }
    
    ////////////////////////////////////////////////////////////////////////////////
}

#if _MSC_VER
#pragma warning(pop) // pop nameless struct/union warning
#endif

#endif // MFGRAPHICS_STRVIEW_HPP
