#pragma once
/**
 * String Tokenizer/View, Copyright (c) 2014-2018, Jorma Rebane
 * Distributed under MIT Software License
 */
#ifndef __cplusplus
#  error <rpp/strview.h> requires C++14 or higher
#endif
#include <cstring>    // C string utilities
#include <string>     // compatibility with std::string
#include "config.h"

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
    template<int N> bool strcontains(const char(&str)[N], char ch) noexcept {
        for (int i = 0; i < N; ++i)
            if (str[i] == ch) return true;
        return false;
    }
    /**
     * @note Same as strpbrk, except we're not dealing with 0-term strings
     * @note This function is optimized for 4-8 char str and 3-4 char control.
     * @note Retains string literal array length information
     */
    template<int N> const char* strcontains(const char* str, int nstr, const char(&control)[N]) noexcept {
        for (; nstr; --nstr, ++str)
            if (strcontains<N>(control, *str))
                return str; // done
        return nullptr; // not found
    }
    template<size_t N> bool strequals(const char* s1, const char(&s2)[N]) noexcept {
        for (size_t i = 0; i < (N - 1); ++i)
            if (s1[i] != s2[i]) return false; // not equal.
        return true;
    }
    template<size_t N> bool strequalsi(const char* s1, const char(&s2)[N]) noexcept {
        for (size_t i = 0; i < (N - 1); ++i)
            if (::toupper(s1[i]) != ::toupper(s2[i])) return false; // not equal.
        return true;
    }


    // This is same as memchr, but optimized for very small control strings
    RPPAPI bool strcontains(const char* str, int len, char ch) noexcept;
    /**
     * @note Same as strpbrk, except we're not dealing with 0-term strings
     * @note This function is optimized for 4-8 char str and 3-4 char control.
     */
    RPPAPI const char* strcontains(const char* str, int nstr, const char* control, int ncontrol) noexcept;
    RPPAPI NOINLINE bool strequals(const char* s1, const char* s2, int len) noexcept;
    RPPAPI NOINLINE bool strequalsi(const char* s1, const char* s2, int len) noexcept;






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
    inline int _tostring(char* buffer, long value)      noexcept { return _tostring(buffer, (int)value);  }
    inline int _tostring(char* buffer, ulong value)     noexcept { return _tostring(buffer, (uint)value); }




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
        const char* str; // start of string
        int len;         // length of string

        FINLINE constexpr strview()                            noexcept : str{""},  len{0} {}
        FINLINE strview(char* str)                             noexcept : str{str}, len{static_cast<int>(strlen(str))} {}
        FINLINE strview(const char* str)                       noexcept : str{str}, len{static_cast<int>(strlen(str))} {}
        FINLINE constexpr strview(const char* str, int len)    noexcept : str{str}, len{len}                           {}
        FINLINE constexpr strview(const char* str, size_t len) noexcept : str{str}, len{static_cast<int>(len)}         {}
        FINLINE strview(const char* str, const char* end)      noexcept : str{str}, len{static_cast<int>(end - str)}   {}
        FINLINE strview(const void* str, const void* end)      noexcept : strview{static_cast<const char*>(str), static_cast<const char*>(end)} {}
        FINLINE strview(const std::string& s)                  noexcept : str{s.c_str()}, len{static_cast<int>(s.length())} {}

        template<class StringT>
        using enable_if_string_like_t = std::enable_if_t<std::is_member_function_pointer<decltype(&StringT::c_str)>::value>;

        template<class StringT, typename = enable_if_string_like_t<StringT>>
        FINLINE strview(const StringT& str) noexcept : str(str.c_str()), len((int)str.length()) {}
        FINLINE const char& operator[](int index) const noexcept { return str[index]; }

        // disallow accidental init from char or bool
        strview(char) = delete;
        strview(bool) = delete;

        // disallow accidental assignment from an std::string&& which is an error, 
        // because the strview will be invalidated immediately
        // however strview(std::string&&) is allowed because it allows passing temporaries as arguments to functions
        strview& operator=(std::string&&) = delete;

        FINLINE strview& operator=(const char* str) noexcept { this->str = str; this->len = (int)strlen(str); return *this; }
        template<int N>
        FINLINE strview& operator=(const char (&str)[N]) noexcept { this->str = str; this->len = N-1; return *this; }

        /** Creates a new string from this string-strview */
        FINLINE std::string& to_string(std::string& out) const { return out.assign(str, (size_t)len); }
        std::string to_string() const { return std::string{str, (size_t)len}; }

        // this is implicit by design; but it may cause some unexpected conversions to std::string
        // main goal is to provide convenient automatic conversion:  string s = my_string_view;
        operator std::string() const { return std::string{str, (size_t)len}; }

        /** 
         * Copies this str[len] string into a C-string array
         * However, if THIS string is null terminated, this operation is a NOP and behaves like c_str()
         */
        NOINLINE const char* to_cstr(char* buf, int max) const noexcept;
        template<int N> 
        FINLINE const char* to_cstr(char (&buf)[N]) const { return to_cstr(buf, N); }
        /** 
         * Copies this str[len] into a max of 512 byte static thread-local C-string array 
         * Result is only valid until next call to this method
         * However, if THIS string is null terminated, this operation is a NOP and behaves like c_str()
         */
        NOINLINE const char* to_cstr() const noexcept;

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
        // Relaxed parsing of this strview as a boolean.
        // Accepts strings that start_with ignorecase: "true", "yes", "on", "1"
        // Everything else is parsed as false.
        // strings such as "true\n" will be parsed as true; same for "trueStatement" --> true,
        // because the conversion is relaxed, not strict.
        // For strict parsing, you should use strview::equalsi("true")
        bool to_bool() const noexcept;

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
        NOINLINE bool is_whitespace() const noexcept;
        /** @return TRUE if the strview ends with a null terminator */
        FINLINE bool is_nullterm() const { return str[len] == '\0'; }

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
        NOINLINE strview& trim_end();
        NOINLINE strview& trim_end(const char* chars, int nchars) noexcept;
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

        /** 
         * UNSAFE view of a new string AFTER this one
         * @code
         * strview content = "key:true";
         * strview value = content.find_sv("key:").after(4); // "true"
         * @endcode
         */
        FINLINE strview after(int len) const { return { end(), len }; }

        /** Similar to after(int), but the end is calculated via @param limit */
        FINLINE strview after_until(const strview& limit) const { return { end(), limit.end() }; }

        /** @return TRUE if the strview contains this char */
        FINLINE bool contains(char c) const { return memchr(str, c, (size_t)len) != nullptr; }
        /** @return TRUE if the strview contains this char */
        FINLINE bool contains(const strview& s) const { return find(s) != nullptr; }
        /** @return TRUE if the strview contains any of the chars */
        NOINLINE bool contains_any(const char* chars, int nchars) const {
            return strcontains(str, len, chars, nchars) != nullptr;
        }
        template<int N> FINLINE bool contains_any(const char (&chars)[N]) const {
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
        int indexof(char ch) const noexcept;
        
        // reverse iterating indexof
        int rindexof(char ch) const noexcept;
        
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
        FINLINE bool operator==(const std::string& s)  const noexcept { return  equals(s); }
        FINLINE bool operator!=(const std::string& s)  const noexcept { return !equals(s); }
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
        FINLINE int compare(const std::string& b)  const noexcept { return compare(b.c_str(),(int)b.size()); }
        
        FINLINE bool operator<(const strview& s) const noexcept { return compare(s.str, s.len) < 0; }
        FINLINE bool operator>(const strview& s) const noexcept { return compare(s.str, s.len) > 0; }
        FINLINE bool operator<(const std::string& s)  const noexcept { return compare(s.c_str(),(int)s.size()) < 0; }
        FINLINE bool operator>(const std::string& s)  const noexcept { return compare(s.c_str(),(int)s.size()) > 0; }
        template<int SIZE> FINLINE bool operator<(const char(&s)[SIZE]) const noexcept {return compare(s,SIZE-1)<0;}
        template<int SIZE> FINLINE bool operator>(const char(&s)[SIZE]) const noexcept {return compare(s,SIZE-1)>0;}
        
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
        template<int N> NOINLINE bool next(strview& out, const char (&delims)[N]) {
            return _next_trim(out, [&delims](const char* s, int n) {
                return strcontains<N>(s, n, delims);
            });
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
        FINLINE strview next_notrim(char delim) noexcept {
            strview out; next_notrim(out, delim); return out;
        }
        FINLINE strview next_notrim(const char* delim, int ndelims) noexcept {
            strview out; next_notrim(out, delim, ndelims); return out;
        }
        template<int N> FINLINE strview next_notrim(const char (&delims)[N]) noexcept {
            strview out;
            _next_notrim(out, [&delims](const char* s, int n) {
                return strcontains<N>(s, n, delims);
            });
            return out;
        }


        // don't forget to mark NOINLINE in the function where you call this...
        template<class SearchFn> FINLINE bool _next_notrim(strview& out, SearchFn searchFn) noexcept
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

        template<class SearchFn> FINLINE bool _next_trim(strview& out, SearchFn searchFn) noexcept
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
        void convertTo(bool& outValue)    const noexcept { outValue = to_bool();   }
        void convertTo(int& outValue)     const noexcept { outValue = to_int();    }
        void convertTo(float& outValue)   const noexcept { outValue = to_float();  }
        void convertTo(double& outValue)  const noexcept { outValue = to_double(); }
        void convertTo(std::string& outValue) const noexcept { to_string(outValue);    }
        void convertTo(strview& outValue)     const noexcept { outValue = *this;       }
        
    private:
        void skipByLength(strview text) noexcept { skip(text.len); }
        void skipByLength(char)         noexcept { skip(1); }
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
        void decompose(const Delim& delim, T& outFirst, Rest&... outRest) noexcept
        {
            if (starts_with(delim)) // this is an empty entry, just skip it;
                skipByLength(delim);
            else
                next(delim).convertTo(outFirst);
            decompose(delim, outRest...);
        }
        template<class Delim, class T>
        void decompose(const Delim& delim, T& outFirst) noexcept
        {
            next(delim).convertTo(outFirst);
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
        NOINLINE std::string as_lower() const noexcept;

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
        NOINLINE std::string as_upper() const noexcept;

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
    };

    //////////////// strview literal operator ///////////////

    inline namespace literals
    {
        using namespace std::string_literals;

        constexpr strview operator "" _sv(const char* str, std::size_t len)
        {
            return { str, (int)len };
        }
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
        (void)s.to_string(out);
        s.skip(s.len);
        return s;
    }

    //////////////// string concatenate operators /////////////////
    
    inline std::string& operator+=(std::string& a, const strview& b)
    {
        return a.append(b.str, size_t(b.len));
    }
    
    inline std::string operator+(const strview& a, const strview& b)
    {
        std::string str;
        size_t al = size_t(a.len), bl = size_t(b.len);
        str.reserve(al + bl);
        str.append(a.str, al).append(b.str, bl);
        return str;
    }

    inline std::string operator+(const std::string& a, const strview& b){ return strview{a} + b;}
    inline std::string operator+(const strview& a, const std::string& b){ return a + strview{b};}
    inline std::string operator+(const char* a, const strview& b)  { return strview{a, strlen(a)} + b; }
    inline std::string operator+(const strview& a, const char* b)  { return a + strview{b, strlen(b)}; }
    inline std::string operator+(const strview& a, char b)      { return a + strview{&b, 1}; }
    inline std::string operator+(char a, const strview& b)      { return strview{&a, 1} + b; }
    inline std::string&& operator+(std::string&& a,const strview& b) { return std::move(a.append(b.str, (size_t)b.len)); }

    //////////////// optimized string join /////////////////

    RPPAPI std::string concat(const strview& a, const strview& b);
    RPPAPI std::string concat(const strview& a, const strview& b, const strview& c);
    RPPAPI std::string concat(const strview& a, const strview& b, const strview& c, const strview& d);
    RPPAPI std::string concat(const strview& a, const strview& b, const strview& c, const strview& d, const strview& e);

    //////////////// string compare operators /////////////////

    inline bool operator< (const std::string& a,const strview& b) {return strview(a) <  b;}
    inline bool operator> (const std::string& a,const strview& b) {return strview(a) >  b;}
    inline bool operator==(const std::string& a,const strview& b) {return strview(a) == b;}
    inline bool operator!=(const std::string& a,const strview& b) {return strview(a) != b;}
    
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
        const char* str;
        int len;
        FINLINE operator strview()        const { return strview{str, len}; }
        FINLINE operator strview&()             { return *(rpp::strview*)this; }
        FINLINE operator const strview&() const { return *(rpp::strview*)this; }
        FINLINE strview* operator->()     const { return  (rpp::strview*)this; }
    };


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
     * Converts an std::string into its lowercase form
     */
    RPPAPI std::string& to_lower(std::string& str) noexcept;

    /**
     * Converts an std::string into its uppercase form
     */
    RPPAPI std::string& to_upper(std::string& str) noexcept;

    /**
     * Replaces characters of 'chOld' with 'chNew' inside the specified string
     */
    RPPAPI char* replace(char* str, int len, char chOld, char chNew) noexcept;

    /**
     * Replaces characters of 'chOld' with 'chNew' inside this std::string
     */
    RPPAPI std::string& replace(std::string& str, char chOld, char chNew) noexcept;


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

        NOINLINE bracket_parser(const void* data, int len);
        FINLINE bracket_parser(const strview& s)             : bracket_parser(s.str, s.len)   {}
        FINLINE bracket_parser(const void* data, size_t len) : bracket_parser(data, (int)len) {}

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
    inline const char* __wrap_arg(const strview& arg) { return arg.to_cstr(); }

} // namespace rpp


/////////////////////// std::hash to use strview in maps ///////////////////////

#if RPP_CLANG_LLVM
#  include <utility> // std::hash
#else
// forward declare std::hash
namespace std { template<class T> struct hash; }
#endif

template<>
struct std::hash<rpp::strview>
{
    size_t operator()(const rpp::strview& s) const noexcept
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

////////////////////////////////////////////////////////////////////////////////

#if _MSC_VER
#pragma warning(pop) // pop nameless struct/union warning
#endif
