#pragma once
/**
 * String Printing and Formatting, Copyright (c) 2017-2018, Jorma Rebane
 * Distributed under MIT Software License
 */
#include <string>
#include "strview.h"
#include "debugging.h"
#include <cstdio>        // fprintf
#include <memory>        // std::shared_ptr
#include <unordered_map> // std::unordered_map for to_string extensions
#include <sstream>

#ifndef RPP_SPRINT_H
#define RPP_SPRINT_H 1
#endif

namespace rpp
{
    ////////////////////////////////////////////////////////////////////////////////

    /**
     * Converts a single char 'x' into string "x"
     */
    RPPAPI std::string to_string(char v)   noexcept;

    /**
     * Simple and fast locale-agnostic to_string utilities:
     */
    RPPAPI std::string to_string(rpp::byte v) noexcept;
    RPPAPI std::string to_string(short v)  noexcept;
    RPPAPI std::string to_string(ushort v) noexcept;
    RPPAPI std::string to_string(int v)    noexcept;
    RPPAPI std::string to_string(uint v)   noexcept;
    RPPAPI std::string to_string(long v)   noexcept;
    RPPAPI std::string to_string(ulong v)  noexcept;
    RPPAPI std::string to_string(int64 v)  noexcept;
    RPPAPI std::string to_string(uint64 v) noexcept;
    RPPAPI std::string to_string(float v)  noexcept;
    RPPAPI std::string to_string(double v) noexcept;

    /** @return "true" or "false" string */
    RPPAPI std::string to_string(bool trueOrFalse) noexcept;

    /**
     * @return std::string from cstring. If cstr == nullptr, returns empty string ""
     */
    RPPAPI std::string to_string(const char* cstr) noexcept;

    RPPAPI inline const std::string& to_string(const std::string& s) noexcept { return s; }

    namespace detail
    {
        template< class... >
        using void_t = void;

        template<typename, template<typename...> class, typename...>
        struct is_detected : std::false_type {};

        template<template<class...> class Operation, typename... Arguments>
        struct is_detected<void_t<Operation<Arguments...>>, Operation, Arguments...> : std::true_type {};
    }

    template<template<class...> class Operation, typename... Arguments>
    using is_detected = detail::is_detected<detail::void_t<>, Operation, Arguments...>;

    template<template<class...> class Operation, typename... Arguments>
    constexpr bool is_detected_v = detail::is_detected<detail::void_t<>, Operation, Arguments...>::value;


    template<class T> using std_to_string_expression  = decltype(std::to_string(std::declval<T>()));
    template<class T> using to_string_expression      = decltype(to_string(std::declval<T>()));
    template<class T> using to_string_memb_expression = decltype(std::declval<T>().to_string());

    template<class T> constexpr bool has_to_string       = is_detected_v<to_string_expression, T>;
    template<class T> constexpr bool has_to_string_memb  = is_detected_v<to_string_memb_expression, T>;
    template<class T> constexpr bool is_to_stringable = has_to_string<T> || has_to_string_memb<T>;

    template<class T> constexpr bool is_byte_array_type = std::is_same<T, void>::value
                                                       || std::is_same<T, uint8_t>::value;

    enum format_opt
    {
        none,
        lowercase,
        uppercase,
    };

    /**
     * Always null terminated version of stringstream, which is compatible with strview
     * Not intended for moving or copying
     */
    struct RPPAPI string_buffer
    {
        static constexpr int SIZE = 512;
        char* ptr = nullptr;
        int   len = 0;
        int   cap = SIZE;
        char  buf[SIZE];

        // controls the separator for generic template write calls
        // the default value " " will turn write("brown", "fox"); into "brown fox"
        // setting it to "..." will turn write("brown", "fox"); into "brown...fox"
        rpp::strview separator = " "_sv;

        FINLINE string_buffer() noexcept { ptr = buf; buf[0] = '\0'; } // NOLINT
        FINLINE explicit string_buffer(strview text) noexcept : string_buffer{} {
            write(text);
        }
        ~string_buffer() noexcept;

        string_buffer(const string_buffer&)            = delete;
        string_buffer& operator=(const string_buffer&) = delete;

        string_buffer(string_buffer&& sb) noexcept;
        string_buffer& operator=(string_buffer&& sb) noexcept;

        FINLINE int size() const noexcept { return len; }
        FINLINE const char* c_str() const noexcept { return ptr; }
        FINLINE const char* data()  const noexcept { return ptr; }
        FINLINE rpp::strview view() const noexcept { return { ptr, len }; }
        FINLINE std::string  str()  const noexcept { return { ptr, ptr+len }; }
        FINLINE char back() const { return len ? ptr[len-1] : '\0'; }

        FINLINE explicit operator bool()         const noexcept { return len > 0; }
        FINLINE explicit operator rpp::strview() const noexcept { return { ptr, len }; }

        void clear() noexcept;
        void reserve(int count) noexcept;
        void resize(int count) noexcept;

        /** @brief Append a string to this buffer */
        void append(const char* str, int slen) noexcept;

        /** @param count Size of buffer to emplace */
        char* emplace_buffer(int count) noexcept;
        void writef(PRINTF_FMTSTR const char* format, ...) noexcept PRINTF_CHECKFMT2;


        void write_ptr_begin() noexcept;
        void write_ptr_end() noexcept;

        template<class T, std::enable_if_t<is_to_stringable<T>, int> = 0>
        void write(const T* ptr) noexcept
        {
            if (ptr == nullptr) return write(nullptr);
            RPP_CXX17_IF_CONSTEXPR(std::is_function<T>::value)
            {
                this->write_ptr(reinterpret_cast<const void*>(ptr));
            }
            else
            {
                write_ptr_begin();
                this->write(*ptr); 
                write_ptr_end();
            }
        }

        template<class T, std::enable_if_t<is_to_stringable<T>, int> = 0>
        void write(T* ptr) noexcept
        {
            if (ptr == nullptr) return write(nullptr);
            write_ptr_begin();this->write(*ptr); write_ptr_end();
        }

        template<class T> using sbuf_op = decltype(std::declval<string_buffer&>() << std::declval<T>());
        template<class T> static constexpr bool has_sbuf_op = is_detected_v<sbuf_op, T>;

        template<class T> using ostrm_op = decltype(std::declval<std::ostream&>() << std::declval<T>());
        template<class T> static constexpr bool has_ostrm_op = is_detected_v<ostrm_op, T>;

        template<class T, std::enable_if_t<has_to_string<T>, int> = 0>
        FINLINE void write(const T& value) noexcept
        {
            this->write(to_string(value));
        }

        template<class T, std::enable_if_t<has_to_string_memb<T>, int> = 0>
        FINLINE void write(const T& value) noexcept
        {
            this->write(value.to_string());
        }

        template<class T, std::enable_if_t<!is_to_stringable<T> && has_sbuf_op<T>, int> = 0>
        FINLINE void write(const T& value) noexcept
        {
            *this << value;
        }

        template<class T, std::enable_if_t<!is_to_stringable<T> && !has_sbuf_op<T> && has_ostrm_op<T>, int> = 0>
        FINLINE void write(const T& value) noexcept
        {
            std::stringstream ss;
            ss << value;
            this->write(ss.str());
        }

        template<int N>
        FINLINE void write(const char (&value)[N])   noexcept { this->write(strview{ value }); }
        FINLINE void write(const std::string& value) noexcept { this->write(strview{ value }); }
        FINLINE void write(const char* value)        noexcept { this->write(strview{ value }); }
        void write(const strview& s) noexcept;
        void write(char value) noexcept;
        void write(bool   value) noexcept;
        void write(rpp::byte bv) noexcept;
        void write(short  value) noexcept;
        void write(ushort value) noexcept;
        void write(int    value) noexcept;
        void write(uint   value) noexcept;
        void write(long   value) noexcept;
        void write(ulong  value) noexcept;
        void write(int64  value) noexcept;
        void write(uint64 value) noexcept;
        void write(float  value) noexcept;
        void write(double value) noexcept;
        void write(std::nullptr_t) noexcept;
        void write(const string_buffer& sb) noexcept;
        FINLINE void write(const void* ptr) noexcept    { this->write_ptr(ptr); }
        FINLINE void write(const uint8_t* ptr) noexcept { this->write_ptr(ptr); }
        FINLINE void write(void* ptr) noexcept          { this->write_ptr(ptr); }
        FINLINE void write(uint8_t* ptr) noexcept       { this->write_ptr(ptr); }
        template<class T> FINLINE void write(const std::weak_ptr<T>& p) noexcept   { this->write(p.lock()); }
        template<class T> FINLINE void write(const std::shared_ptr<T>& p) noexcept { this->write(p.get());  }


        FINLINE string_buffer& operator<<(const std::string& value) noexcept { this->write(strview{ value }); return *this; }
        FINLINE string_buffer& operator<<(const char* value) noexcept        { this->write(strview{ value }); return *this; }
        FINLINE string_buffer& operator<<(const strview& value) noexcept     { this->write(value);            return *this; }
        FINLINE string_buffer& operator<<(char   value) noexcept { this->write(value); return *this; }
        FINLINE string_buffer& operator<<(bool   value) noexcept { this->write(value); return *this; }
        FINLINE string_buffer& operator<<(rpp::byte bv) noexcept { this->write(bv);    return *this; }
        FINLINE string_buffer& operator<<(short  value) noexcept { this->write(value); return *this; }
        FINLINE string_buffer& operator<<(ushort value) noexcept { this->write(value); return *this; }
        FINLINE string_buffer& operator<<(int    value) noexcept { this->write(value); return *this; }
        FINLINE string_buffer& operator<<(uint   value) noexcept { this->write(value); return *this; }
        FINLINE string_buffer& operator<<(long   value) noexcept { this->write(value); return *this; }
        FINLINE string_buffer& operator<<(ulong  value) noexcept { this->write(value); return *this; }
        FINLINE string_buffer& operator<<(int64  value) noexcept { this->write(value); return *this; }
        FINLINE string_buffer& operator<<(uint64 value) noexcept { this->write(value); return *this; }
        FINLINE string_buffer& operator<<(float  value) noexcept { this->write(value); return *this; }
        FINLINE string_buffer& operator<<(double value) noexcept { this->write(value); return *this; }
        FINLINE string_buffer& operator<<(std::nullptr_t) noexcept { this->write(std::nullptr_t{}); return *this; }
        FINLINE string_buffer& operator<<(const string_buffer& buf) noexcept { this->write(buf); return *this; }
        FINLINE string_buffer& operator<<(const void* ptr) noexcept    { this->write_ptr(ptr); return *this; }
        FINLINE string_buffer& operator<<(const uint8_t* ptr) noexcept { this->write_ptr(ptr); return *this; }
        FINLINE string_buffer& operator<<(void* ptr) noexcept          { this->write_ptr(ptr); return *this; }
        FINLINE string_buffer& operator<<(uint8_t* ptr) noexcept       { this->write_ptr(ptr); return *this; }
        template<class T> FINLINE string_buffer& operator<<(const std::weak_ptr<T>& p) noexcept   { this->write(p.lock()); return *this; }
        template<class T> FINLINE string_buffer& operator<<(const std::shared_ptr<T>& p) noexcept { this->write(p.get());  return *this; }

        template<class T, std::enable_if_t<is_to_stringable<T>, int> = 0>
        FINLINE string_buffer& operator<<(const T* obj) noexcept { this->write(obj); return *this; }

        template<class T, std::enable_if_t<is_to_stringable<T>, int> = 0>
        FINLINE string_buffer& operator<<(T* obj) noexcept       { this->write(obj); return *this; }

        template<class T, std::enable_if_t<is_to_stringable<T>, int> = 0>
        FINLINE string_buffer& operator<<(const T& obj) noexcept { this->write(obj); return *this; }

        template<class T, std::enable_if_t<!is_to_stringable<T> && has_ostrm_op<T>, int> = 0>
        FINLINE string_buffer& operator<<(const T& obj) noexcept
        {
            std::stringstream ss;
            ss << obj;
            this->write(ss.str());
            return *this;
        }

        template<typename T, template<class, class...> class C, class... Args>
        FINLINE string_buffer& operator<<(const C<T, Args...>& container) noexcept
        {
            this->write(container, /*newlines:*/true);
            return *this;
        }

        /**
         * Appends a full hex string from the given byte buffer
         * @param data Bytes
         * @param numBytes Number of bytes
         * @param opt Formatting options [lowercase, uppercase]
         */
        void write_hex(const void* data, int numBytes, format_opt opt = lowercase) noexcept;
        void write_hex(const strview& str, format_opt opt = lowercase) noexcept {
            write_hex(str.str, str.len, opt);
        }
        void write_hex(const std::string& str, format_opt opt = lowercase) noexcept {
            write_hex(str.c_str(), static_cast<int>(str.size()), opt);
        }

        /**
         * Appends a single value (such as int or char) as a hex string
         * @param value Any value to convert to a hex string
         * @param opt Formatting options [lowercase, uppercase]
         */
        template<class T> void write_hex(const T& value, format_opt opt = lowercase) noexcept {
            this->write_hex(&value, sizeof(value), opt);
        }

        void write_ptr(const void* p, format_opt opt = lowercase) noexcept;

        void writeln() noexcept;     // \n
        void write_quote() noexcept; // <">
        void write_apos() noexcept;  // <'>
        void write_colon() noexcept; // <: >
        void write_separator() noexcept; // this->separator

        template<class T> FINLINE void write_with_separator(const T& arg) noexcept
        {
            write_separator();
            write(arg);
        }

        /**
         * Stringifies and appends the input arguments one by one, filling gaps with delimiter
         * @see string_buffer::delimiter (default = ' ')
         * Ex: write("test:", 10, 20.1f);  --> "test: 10 20.1"
         */
        template<class T, class... Args> FINLINE void write(const T& first, const Args&... args) noexcept
        {
            write(first);
            #if RPP_HAS_CXX17
                (..., write_with_separator(args)); // C++17 Fold Expressions
            #else
                write_separator(); write(args...);
            #endif
        }
        template<class T> FINLINE void writeln(const T& value) noexcept
        {
            write(value);
            writeln();
        }
        template<class T, class... Args> FINLINE void writeln(const T& first, const Args&... args) noexcept
        {
            write(first);
            #if RPP_HAS_CXX17
                (..., write_with_separator(args)); // C++17 Fold Expressions
            #else
                write_separator(); write(args...);
            #endif
            writeln();
        }

        ////////////////////////////////////////////////////////////////////////////////////////////

        template<class T> FINLINE void prettyprint(const T& value) noexcept {
            write(value);
        }
        template<class T> void prettyprint(const T* ptr) noexcept
        {
            if (ptr == nullptr) return write(nullptr);
            write_ptr_begin(); prettyprint(*ptr); write_ptr_end();
        }
        template<class T> FINLINE void prettyprint(T* ptr) noexcept                      { prettyprint((const T*)ptr); }
        template<class T> FINLINE void prettyprint(const std::weak_ptr<T>& p) noexcept   { prettyprint(p.lock());  }
        template<class T> FINLINE void prettyprint(const std::shared_ptr<T>& p) noexcept { prettyprint(p.get());   }


        FINLINE void prettyprint(const std::string& value) noexcept  { write_quote(); write(value); write_quote(); }
        FINLINE void prettyprint(const strview& value) noexcept { write_quote(); write(value); write_quote(); }
        FINLINE void prettyprint(const char& value) noexcept    { write_apos();  write(value); write_apos();  }
        template<class K, class V> FINLINE void prettyprint(const K& key, const V& value) noexcept {
            prettyprint(key); write_colon(); prettyprint(value);
        }
        template<class K, class V> FINLINE void prettyprint(const std::pair<K, V>& pair) noexcept {
            prettyprint(pair.first); write_colon(); prettyprint(pair.second);
        }

        void pretty_cont_start(int count, bool newlines) noexcept;
        void pretty_cont_item_start(bool newlines) noexcept;
        void pretty_cont_item_end(int i, int count, bool newlines) noexcept;
        void pretty_cont_end(int count) noexcept;

        template<typename T, template<class, class...> class C, class... Args>
        NOINLINE void write(const C<T, Args...>& container, bool newlines = true) noexcept
        {
            int count = (int)container.size();
            pretty_cont_start(count, newlines);
            int i = 0;
            for (const auto& item : container) {
                pretty_cont_item_start(newlines);
                prettyprint(item);
                pretty_cont_item_end(++i, count, newlines);
            }
            pretty_cont_end(count);
        }

        template<typename T, template<class, class...> class C, class... Args>
        NOINLINE void prettyprint(const C<T, Args...>& container, bool newlines = true) noexcept
        {
            this->write(container, newlines);
        }

        /**
         * Similar to write(...), but performs prettyprint formatting to each argument
         */
        template<class T, class... Args> FINLINE void prettyprint(const T& first, const Args&... args) noexcept
        {
            prettyprint(first); write_separator(); prettyprint(args...);
        }
        template<class T, class... Args> FINLINE void prettyprintln(const T& first, const Args&... args) noexcept
        {
            prettyprint(first, args...); writeln();
        }
    };

    ////////////////////////////////////////////////////////////////////////////////

    RPPAPI inline std::string to_hex_string(const strview& s, format_opt opt = lowercase) noexcept
    {
        rpp::string_buffer sb;
        sb.write_hex(s, opt);
        return sb.str();
    }

    ////////////////////////////////////////////////////////////////////////////////

    RPPAPI int print(FILE* file, strview value) noexcept;
    RPPAPI int print(FILE* file, char value) noexcept;
    RPPAPI int print(FILE* file, rpp::byte value) noexcept;
    RPPAPI int print(FILE* file, short value) noexcept;
    RPPAPI int print(FILE* file, ushort value) noexcept;
    RPPAPI int print(FILE* file, int value) noexcept;
    RPPAPI int print(FILE* file, uint value) noexcept;
    RPPAPI int print(FILE* file, int64 value) noexcept;
    RPPAPI int print(FILE* file, uint64 value) noexcept;

    RPPAPI int print(strview value) noexcept;
    RPPAPI int print(char value) noexcept;
    RPPAPI int print(rpp::byte value) noexcept;
    RPPAPI int print(short value) noexcept;
    RPPAPI int print(ushort value) noexcept;
    RPPAPI int print(int value) noexcept;
    RPPAPI int print(uint value) noexcept;
    RPPAPI int print(int64 value) noexcept;
    RPPAPI int print(uint64 value) noexcept;

    RPPAPI int println(FILE* file) noexcept;
    RPPAPI int println() noexcept;

    template<class T> int println(FILE* file, const T& value) noexcept
    {
        string_buffer buf; buf.writeln(value);
        return (int)fwrite(buf.ptr, (size_t)buf.len, 1, file);
    }
    template<class T> int println(const T& value) noexcept
    {
        string_buffer buf; buf.writeln(value);
        return (int)fwrite(buf.ptr, (size_t)buf.len, 1, stdout);
    }

    /**
     * Stringifies and appends the input arguments one by one, filling gaps with spaces, similar to Python print()
     * Ex: print("test:", 10, 20.1f);  --> "test: 10 20.1"
     */
    template<class T, class... Args> int print(FILE* file, const T& first, const Args&... args) noexcept
    {
        string_buffer buf; buf.write(first, args...);
        return (int)fwrite(buf.ptr, (size_t)buf.len, 1, file);
    }
    template<class T, class... Args> int print(const T& first, const Args&... args) noexcept
    {
        string_buffer buf; buf.write(first, args...);
        return (int)fwrite(buf.ptr, (size_t)buf.len, 1, stdout);
    }

    /**
     * Stringifies and appends the input arguments one by one with an endline, filling gaps with spaces, similar to Python print()
     * Ex: print("test:", 10, 20.1f);  --> "test: 10 20.1\n"
     */
    template<class T, class... Args> int println(FILE* file, const T& first, const Args&... args) noexcept
    {
        string_buffer buf; buf.writeln(first, args...);
        return (int)fwrite(buf.ptr, (size_t)buf.len, 1, file);
    }
    template<class T, class... Args> int println(const T& first, const Args&... args) noexcept
    {
        string_buffer buf; buf.writeln(first, args...);
        return (int)fwrite(buf.ptr, (size_t)buf.len, 1, stdout);
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
    template<class T, class... Args> std::string sprint(const T& first, const Args&... args) noexcept
    {
        string_buffer buf; buf.write(first, args...);
        return buf.str();
    }
    template<class T, class... Args> std::string sprintln(const T& first, const Args&... args) noexcept
    {
        string_buffer buf; buf.writeln(first, args...);
        return buf.str();
    }

    inline const char* __format_wrap(const std::string& s) noexcept { return s.c_str();    }
    inline const char* __format_wrap(const strview& s) noexcept { return s.to_cstr();  }
    template<class T> const T& __format_wrap(const T& t) noexcept { return t; }

    RPPAPI std::string __format(const char* format, ...) noexcept; 

    template<class... Args> std::string format(const char* format, const Args&... args) noexcept
    {
        return __format(format, __format_wrap(args)...);
    }

    /////////////////////// useful container tostr extensions //////////////////////

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
    NOINLINE std::string to_string(const C<T,Args...>& container, bool newLineSeparator = true) noexcept
    {
        rpp::string_buffer sb; sb.prettyprint(container, newLineSeparator); return sb.str();
    }
    template<class T> NOINLINE std::string to_string(const std::shared_ptr<T>& p) noexcept
    {
        rpp::string_buffer sb; sb.prettyprint(p); return sb.str();
    }
    template<class T> NOINLINE std::string to_string(const std::weak_ptr<T>& p) noexcept
    {
        rpp::string_buffer sb; sb.prettyprint(p); return sb.str();
    }

    ////////////////////////////////////////////////////////////////////////////////
}
